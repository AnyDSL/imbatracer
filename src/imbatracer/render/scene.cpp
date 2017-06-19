#include <cassert>

#include "imbatracer/render/scene.h"
#include "imbatracer/core/adapter.h"
#include "imbatracer/loaders/loaders.h"

namespace imba {

template <typename Node>
void Scene::setup_traversal_buffers(BuildAccelData<Node>& build_data, TraversalData<Node>& traversal_data, anydsl::Platform plat) {
    // Make sure the buffers have the right size (using upper bound on number of BVH nodes)
    const int total_nodes = (2 * instances_.size() - 1) + build_data.node_count;
    if (traversal_data.nodes.size() < total_nodes) {
        traversal_data.nodes = std::move(anydsl::Array<Node>(plat, anydsl::Device(0), total_nodes * sizeof(Node)));
    }
    if (traversal_data.tris.size() < build_data.tris.size()) {
        traversal_data.tris = std::move(anydsl::Array<Vec4>(plat, anydsl::Device(0), build_data.tris.size()));
    }
    if (traversal_data.instances.size() < instance_nodes_.size()) {
        traversal_data.instances = std::move(anydsl::Array<InstanceNode>(plat, anydsl::Device(0), instance_nodes_.size()));
    }
}

void Scene::setup_traversal_buffers() {
    if (cpu_buffers_)
        setup_traversal_buffers(build_cpu_, traversal_cpu_, anydsl::Platform::Host);
    if (gpu_buffers_)
        setup_traversal_buffers(build_gpu_, traversal_gpu_, anydsl::Platform::Cuda);
}

template <typename Node, typename NewAdapterFn, typename LoadAccelFn, typename StoreAccelFn>
void Scene::build_mesh_accels(BuildAccelData<Node>& build_data,
                              const std::vector<std::string>& accel_filenames,
                              NewAdapterFn new_adapter,
                              LoadAccelFn load_accel,
                              StoreAccelFn store_accel) {
    build_data.layout.clear();
    build_data.nodes.clear();
    build_data.tris.clear();

    // Add the nodes for all meshes. Assumes that the adapter appends nodes to the array.
    auto adapter = new_adapter(build_data.nodes, build_data.tris);
    for (int mesh_id = 0; mesh_id < meshes_.size(); mesh_id++) {
        auto& mesh = meshes_[mesh_id];

        build_data.layout.push_back(build_data.nodes.size());
        auto tris_offset = build_data.tris.size();
        auto& filename = accel_filenames[mesh_id];

        if (filename != "" && load_accel(filename, build_data.nodes, build_data.tris, tri_layout_[mesh_id]))
            continue;

        std::cout << "Rebuilding the acceleration structure for mesh " << mesh_id << "..." << std::flush;
        adapter->build_accel(mesh, mesh_id, tri_layout_);
        std::cout << std::endl;

#ifdef STATISTICS
        adapter->print_stats();
#endif

        if (filename != "" && !store_accel(filename, build_data.nodes, build_data.layout.back(), build_data.tris, tris_offset, tri_layout_[mesh_id]))
            std::cout << "The acceleration structure for mesh " << mesh_id << " could not be stored." << std::endl;
    }

    build_data.node_count = build_data.nodes.size();
}

void Scene::build_mesh_accels(const std::vector<std::string>& accel_filenames) {
    // Copy all texture coordinates and indices into one huge array.
    tri_layout_.clear();
    int tri_offset = 0;
    for (auto& mesh : meshes_) {
        auto texcoords = mesh.attribute<float2>(MeshAttributes::TEXCOORDS);
        const int offset = texcoord_buf_.size();

        for (int i = 0; i < mesh.vertex_count(); ++i) {
            const int k = texcoord_buf_.size();
            texcoord_buf_.emplace_back();
            texcoord_buf_[k].x = texcoords[i].x;
            texcoord_buf_[k].y = texcoords[i].y;
        }

        for (int i = 0; i < mesh.index_count(); ++i) {
            // Offset the indices for the texture coordinates, but not for the material id.
            int tex_offset = i % 4 == 3 ? 0 : offset;
            index_buf_.push_back(tex_offset + mesh.indices()[i]);
        }

        tri_layout_.push_back(tri_offset);
        tri_offset += mesh.triangle_count();
    }

    if (cpu_buffers_) build_mesh_accels(build_cpu_, accel_filenames, new_mesh_adapter_cpu, load_accel_cpu, store_accel_cpu);
    if (gpu_buffers_) build_mesh_accels(build_gpu_, accel_filenames, new_mesh_adapter_gpu, load_accel_gpu, store_accel_gpu);
}

template <typename Node, typename NewAdapterFn>
void Scene::build_top_level_accel(BuildAccelData<Node>& build_data, NewAdapterFn new_adapter) {
    assert(!build_data.layout.empty() && instances_.size() > 0);

    build_data.top_nodes.clear();
    instance_nodes_.clear();

    auto adapter = new_adapter(build_data.top_nodes, instance_nodes_);
    adapter->build_accel(meshes_, instances_, build_data.layout, build_data.node_count);
}

void Scene::build_top_level_accel() {
    if (cpu_buffers_) build_top_level_accel(build_cpu_, new_top_level_adapter_cpu);
    if (gpu_buffers_) build_top_level_accel(build_gpu_, new_top_level_adapter_gpu);
}

template <typename Node>
void Scene::upload_mesh_accels(BuildAccelData<Node>& build_data, TraversalData<Node>& traversal_data) {
    anydsl_copy(0, build_data.nodes.data(), 0,
                traversal_data.nodes.device(), traversal_data.nodes.data(), 0,
                sizeof(Node) * build_data.nodes.size());
    anydsl_copy(0, build_data.tris.data(), 0,
                traversal_data.tris.device(), traversal_data.tris.data(), 0,
                sizeof(Vec4) * build_data.tris.size());

    std::vector<Node>().swap(build_data.nodes);
    std::vector<Vec4>().swap(build_data.tris);
}

void Scene::upload_mesh_accels() {
    setup_traversal_buffers();

    if (cpu_buffers_) upload_mesh_accels(build_cpu_, traversal_cpu_);
    if (gpu_buffers_) upload_mesh_accels(build_gpu_, traversal_gpu_);

    std::vector<Vec2>().swap(texcoord_buf_);
    std::vector<int >().swap(index_buf_);
}

template <typename Node>
void Scene::upload_top_level_accel(BuildAccelData<Node>& build_data, TraversalData<Node>& traversal_data) {
    anydsl_copy(0, build_data.top_nodes.data(), 0,
                traversal_data.nodes.device(), traversal_data.nodes.data(), sizeof(Node) * build_data.node_count,
                sizeof(Node) * build_data.top_nodes.size());
    anydsl_copy(0, instance_nodes_.data(), 0,
                traversal_data.instances.device(), traversal_data.instances.data(), 0,
                sizeof(InstanceNode) * instance_nodes_.size());
    traversal_data.root = build_data.node_count;

    std::vector<Node>().swap(build_data.top_nodes);
    std::vector<int>().swap(build_data.layout);
}

void Scene::upload_top_level_accel() {
    setup_traversal_buffers();

    if (cpu_buffers_) upload_top_level_accel(build_cpu_, traversal_cpu_);
    if (gpu_buffers_) upload_top_level_accel(build_gpu_, traversal_gpu_);

    std::vector<InstanceNode>().swap(instance_nodes_);
}

void Scene::compute_bounding_sphere() {
    // We use a box as an approximation
    scene_bb_ = BBox::empty();
    for (auto& inst : instances()) {
        auto bb = transform(inst.mat, mesh(inst.id).bounding_box());
        scene_bb_.extend(bb);
    }
    const float radius = length(scene_bb_.max - scene_bb_.min) * 0.5f;
    sphere_.inv_radius_sqr = 1.0f / sqr(radius);
    sphere_.radius = radius;
    sphere_.center = (scene_bb_.max + scene_bb_.min) * 0.5f;
}

} // namespace imba

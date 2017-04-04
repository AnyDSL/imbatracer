#include <cassert>

#include "scene.h"
#include "adapter.h"

#include "../loaders/loaders.h"

namespace imba {

void Scene::setup_traversal_buffers_cpu() {
    // Make sure the buffers have the right size
    const int total_nodes = (2 * instances_.size() - 1) + node_count_cpu_;
    if (traversal_cpu_.nodes.size() < total_nodes) {
        traversal_cpu_.nodes = std::move(anydsl::Array<char>(total_nodes * sizeof(traversal_cpu::Node)));
    }
    if (traversal_cpu_.tris.size() < tris_cpu_.size()) {
        traversal_cpu_.tris = std::move(anydsl::Array<Vec4>(tris_cpu_.size()));
    }
    if (traversal_cpu_.instances.size() < instance_nodes_.size()) {
        traversal_cpu_.instances = std::move(anydsl::Array<InstanceNode>(instance_nodes_.size()));
    }
    if (traversal_cpu_.indices.size() < index_buf_.size()) {
        traversal_cpu_.indices = anydsl::Array<int>(index_buf_.size());
    }
    if (traversal_cpu_.texcoords.size() < texcoord_buf_.size()) {
        traversal_cpu_.texcoords = anydsl::Array<Vec2>(texcoord_buf_.size());
    }
}

void Scene::setup_traversal_buffers_gpu() {
    // Make sure the buffers have the right size
    const int total_nodes = (2 * instances_.size() - 1) + node_count_gpu_;
    if (traversal_gpu_.nodes.size() < total_nodes) {
        traversal_gpu_.nodes = std::move(anydsl::Array<char>(anydsl::Platform::Cuda, anydsl::Device(0), total_nodes * sizeof(traversal_gpu::Node)));
    }
    if (traversal_gpu_.tris.size() < tris_gpu_.size()) {
        traversal_gpu_.tris = std::move(anydsl::Array<Vec4>(anydsl::Platform::Cuda, anydsl::Device(0), tris_gpu_.size()));
    }
    if (traversal_gpu_.instances.size() < instance_nodes_.size()) {
        traversal_gpu_.instances = std::move(anydsl::Array<InstanceNode>(anydsl::Platform::Cuda, anydsl::Device(0), instance_nodes_.size()));
    }
    if (traversal_gpu_.indices.size() < index_buf_.size()) {
        traversal_gpu_.indices = anydsl::Array<int>(anydsl::Platform::Cuda, anydsl::Device(0), index_buf_.size());
    }
    if (traversal_gpu_.texcoords.size() < texcoord_buf_.size()) {
        traversal_gpu_.texcoords = anydsl::Array<Vec2>(anydsl::Platform::Cuda, anydsl::Device(0), texcoord_buf_.size());
    }
}

void Scene::setup_traversal_buffers() {
    if (cpu_buffers_)
        setup_traversal_buffers_cpu();
    if (gpu_buffers_)
        setup_traversal_buffers_gpu();
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

    if (cpu_buffers_) {
        layout_cpu_.clear();
        nodes_cpu_.clear();
        tris_cpu_.clear();

        // Add the nodes for all meshes. Assumes that the adapter appends nodes to the array.
        auto adapter = new_mesh_adapter_cpu(nodes_cpu_, tris_cpu_);
        for (int mesh_id = 0; mesh_id < meshes_.size(); mesh_id++) {
            auto& mesh = meshes_[mesh_id];

            layout_cpu_.push_back(nodes_cpu_.size());
            const int tris_offset = tris_cpu_.size();
            auto& filename = accel_filenames[mesh_id];

            if (filename != "" && load_accel_cpu(filename, nodes_cpu_, tris_cpu_, tri_layout_[mesh_id]))
                continue;

            std::cout << "Rebuilding the acceleration structure for mesh " << mesh_id << "..." << std::flush;
            adapter->build_accel(mesh, mesh_id, tri_layout_);
            std::cout << std::endl;

    #ifdef STATISTICS
            adapter->print_stats();
    #endif

            if (filename != "" && !store_accel_cpu(filename, nodes_cpu_, layout_cpu_.back(), tris_cpu_, tris_offset, tri_layout_[mesh_id]))
                std::cout << "The acceleration structure for mesh " << mesh_id << " could not be stored." << std::endl;
        }

        node_count_cpu_ = nodes_cpu_.size();
    }

    if (gpu_buffers_) {
        layout_gpu_.clear();
        nodes_gpu_.clear();
        tris_gpu_.clear();

        // Add the nodes for all meshes. Assumes that the adapter appends nodes to the array.
        auto adapter = new_mesh_adapter_gpu(nodes_gpu_, tris_gpu_);
        for (int mesh_id = 0; mesh_id < meshes_.size(); mesh_id++) {
            auto& mesh = meshes_[mesh_id];

            layout_gpu_.push_back(nodes_gpu_.size());
            const int tris_offset = tris_gpu_.size();
            auto& filename = accel_filenames[mesh_id];

            if (filename != "" && load_accel_gpu(filename, nodes_gpu_, tris_gpu_, tri_layout_[mesh_id]))
                continue;

            std::cout << "Rebuilding the acceleration structure for mesh " << mesh_id << "..." << std::flush;
            adapter->build_accel(mesh, mesh_id, tri_layout_);
            std::cout << std::endl;

    #ifdef STATISTICS
            adapter->print_stats();
    #endif

            if (filename != "" && !store_accel_gpu(filename, nodes_gpu_, layout_gpu_.back(), tris_gpu_, tris_offset, tri_layout_[mesh_id]))
                std::cout << "The acceleration structure for mesh " << mesh_id << " could not be stored." << std::endl;
        }

        node_count_gpu_ = nodes_gpu_.size();
    }
}

void Scene::build_top_level_accel() {
    if (cpu_buffers_) {
        assert(!layout_cpu_.empty() && instances_.size() > 0);

        top_nodes_cpu_.clear();
        instance_nodes_.clear();

        auto adapter = new_top_level_adapter_cpu(top_nodes_cpu_, instance_nodes_);
        adapter->build_accel(meshes_, instances_, layout_cpu_, node_count_cpu_);
    }

    if (gpu_buffers_) {
        assert(!layout_gpu_.empty() && instances_.size() > 0);

        top_nodes_gpu_.clear();
        instance_nodes_.clear();

        auto adapter = new_top_level_adapter_gpu(top_nodes_gpu_, instance_nodes_);
        adapter->build_accel(meshes_, instances_, layout_gpu_, node_count_gpu_);
    }
}

void Scene::upload_mask_buffer(const MaskBuffer& masks) {
    if (cpu_buffers_) {
        traversal_cpu_.masks = std::move(anydsl::Array<::TransparencyMask>(masks.mask_count()));
        anydsl_copy(0, masks.descs(), 0,
                    traversal_cpu_.masks.device(), traversal_cpu_.masks.data(), 0,
                    sizeof(MaskBuffer::MaskDesc) * masks.mask_count());

        traversal_cpu_.mask_buffer = std::move(anydsl::Array<char>(masks.buffer_size()));
        anydsl_copy(0, masks.buffer(), 0,
                    traversal_cpu_.mask_buffer.device(), traversal_cpu_.mask_buffer.data(), 0,
                    masks.buffer_size());
    }

    if (gpu_buffers_) {
        traversal_gpu_.masks = std::move(anydsl::Array<::TransparencyMask>(anydsl::Platform::Cuda, anydsl::Device(0), masks.mask_count()));
        anydsl_copy(0, masks.descs(), 0,
                    traversal_gpu_.masks.device(), traversal_gpu_.masks.data(), 0,
                    sizeof(MaskBuffer::MaskDesc) * masks.mask_count());

        traversal_gpu_.mask_buffer = std::move(anydsl::Array<char>(anydsl::Platform::Cuda, anydsl::Device(0), masks.buffer_size()));
        anydsl_copy(0, masks.buffer(), 0,
                    traversal_gpu_.mask_buffer.device(), traversal_gpu_.mask_buffer.data(), 0,
                    masks.buffer_size());
    }
}

void Scene::upload_mesh_accels() {
    setup_traversal_buffers();

    if (cpu_buffers_) {
        using traversal_cpu::Node;
        anydsl_copy(0, nodes_cpu_.data(), 0,
                    traversal_cpu_.nodes.device(), traversal_cpu_.nodes.data(), 0,
                    sizeof(Node) * nodes_cpu_.size());
        anydsl_copy(0, tris_cpu_.data(), 0,
                    traversal_cpu_.tris.device(), traversal_cpu_.tris.data(), 0,
                    sizeof(Vec4) * tris_cpu_.size());

        anydsl_copy(0, index_buf_.data(), 0,
                    traversal_cpu_.indices.device(), traversal_cpu_.indices.data(), 0,
                    sizeof(int) * index_buf_.size());
        anydsl_copy(0, texcoord_buf_.data(), 0,
                    traversal_cpu_.texcoords.device(), traversal_cpu_.texcoords.data(), 0,
                    sizeof(Vec2) * texcoord_buf_.size());
    }

    if (gpu_buffers_) {
        using traversal_gpu::Node;
        anydsl_copy(0, nodes_gpu_.data(), 0,
                    traversal_gpu_.nodes.device(), traversal_gpu_.nodes.data(), 0,
                    sizeof(Node) * nodes_gpu_.size());
        anydsl_copy(0, tris_gpu_.data(), 0,
                    traversal_gpu_.tris.device(), traversal_gpu_.tris.data(), 0,
                    sizeof(Vec4) * tris_gpu_.size());

        anydsl_copy(0, index_buf_.data(), 0,
                    traversal_gpu_.indices.device(), traversal_gpu_.indices.data(), 0,
                    sizeof(int) * index_buf_.size());
        anydsl_copy(0, texcoord_buf_.data(), 0,
                    traversal_gpu_.texcoords.device(), traversal_gpu_.texcoords.data(), 0,
                    sizeof(Vec2) * texcoord_buf_.size());
    }

    // Release the memory associated with the triangles and nodes
    std::vector<traversal_cpu::Node>().swap(nodes_cpu_);
    std::vector<traversal_gpu::Node>().swap(nodes_gpu_);
    std::vector<Vec4>().swap(tris_cpu_);
    std::vector<Vec4>().swap(tris_gpu_);
    std::vector<Vec2>().swap(texcoord_buf_);
    std::vector<int >().swap(index_buf_);
}

void Scene::upload_top_level_accel() {
    setup_traversal_buffers();

    if (cpu_buffers_) {
        using traversal_cpu::Node;
        anydsl_copy(0, top_nodes_cpu_.data(), 0,
                    traversal_cpu_.nodes.device(), traversal_cpu_.nodes.data(), sizeof(Node) * node_count_cpu_,
                    sizeof(Node) * top_nodes_cpu_.size());
        anydsl_copy(0, instance_nodes_.data(), 0,
                    traversal_cpu_.instances.device(), traversal_cpu_.instances.data(), 0,
                    sizeof(InstanceNode) * instance_nodes_.size());

        traversal_cpu_.root = node_count_cpu_;
    }

    if (gpu_buffers_) {
        using traversal_gpu::Node;
        anydsl_copy(0, top_nodes_gpu_.data(), 0,
                    traversal_gpu_.nodes.device(), traversal_gpu_.nodes.data(), sizeof(Node) * node_count_gpu_,
                    sizeof(Node) * top_nodes_gpu_.size());
        anydsl_copy(0, instance_nodes_.data(), 0,
                    traversal_gpu_.instances.device(), traversal_gpu_.instances.data(), 0,
                    sizeof(InstanceNode) * instance_nodes_.size());

        traversal_gpu_.root = node_count_gpu_;
    }

    // Release the memory associated with the top-level nodes
    std::vector<traversal_cpu::Node>().swap(top_nodes_cpu_);
    std::vector<traversal_gpu::Node>().swap(top_nodes_gpu_);
    std::vector<int>().swap(layout_cpu_);
    std::vector<int>().swap(layout_gpu_);
    std::vector<InstanceNode>().swap(instance_nodes_);
}

void Scene::compute_bounding_sphere() {
    // We use a box as an approximation
    BBox scene_bb = BBox::empty();
    for (auto& inst : instances()) {
        auto bb = transform(inst.mat, mesh(inst.id).bounding_box());
        scene_bb.extend(bb);
    }
    const float radius = length(scene_bb.max - scene_bb.min) * 0.5f;
    sphere_.inv_radius_sqr = 1.0f / sqr(radius);
    sphere_.radius = radius;
    sphere_.center = (scene_bb.max + scene_bb.min) * 0.5f;
}

} // namespace imba

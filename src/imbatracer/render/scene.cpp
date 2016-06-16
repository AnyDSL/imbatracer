#include <cassert>

#include "scene.h"
#include "../core/adapter.h"

namespace imba {

void Scene::setup_traversal_buffers() {
    // Make sure the buffers have the right size
    const int node_count = top_nodes_.size() + nodes_.size();
    if (traversal_.nodes.size() < node_count) {
        traversal_.nodes = std::move(thorin::Array<Node>(TRAVERSAL_PLATFORM, TRAVERSAL_DEVICE, node_count));
    }
    if (traversal_.tris.size() < tris_.size()) {
        traversal_.tris = std::move(thorin::Array<Vec4>(TRAVERSAL_PLATFORM, TRAVERSAL_DEVICE, tris_.size()));
    }
    if (traversal_.instances.size() < instance_nodes_.size()) {
        traversal_.instances = std::move(thorin::Array<InstanceNode>(TRAVERSAL_PLATFORM, TRAVERSAL_DEVICE, instance_nodes_.size()));
    }
}

void Scene::build_mesh_accels() {
    layout_.clear();
    nodes_.clear();
    tris_.clear();

    // Add a dummy for the root node (will be added when building the top level acceleration structure)
    nodes_.emplace_back();

    // Add the nodes for all meshes. Assumes that the adapter appends nodes to the array.
    auto adapter = new_mesh_adapter(nodes_, tris_);
    for (auto& mesh : meshes_) {
        layout_.push_back(nodes_.size());
        adapter->build_accel(mesh);
#ifdef STATISTICS
        adapter->print_stats();
#endif
    }
}

void Scene::build_top_level_accel() {
    assert(!layout_.empty());

    top_nodes_.clear();

    auto adapter = new_top_level_adapter(top_nodes_, instance_nodes_);
    adapter->build_accel(meshes_, instances_, layout_, nodes_.size());

    // Copy the root node to the beginning of the nodes array.
    nodes_[0] = top_nodes_[0];
}

void Scene::upload_mask_buffer(const MaskBuffer& masks) {
    traversal_.masks = std::move(thorin::Array<::TransparencyMask>(TRAVERSAL_PLATFORM, TRAVERSAL_DEVICE, masks.mask_count()));
    thorin_copy(0, masks.descs(), 0,
                traversal_.masks.device(), traversal_.masks.data(), 0,
                sizeof(MaskBuffer::MaskDesc) * masks.mask_count());

    traversal_.mask_buffer = std::move(thorin::Array<char>(TRAVERSAL_PLATFORM, TRAVERSAL_DEVICE, masks.buffer_size()));
    thorin_copy(0, masks.buffer(), 0,
                traversal_.mask_buffer.device(), traversal_.mask_buffer.data(), 0,
                masks.buffer_size());
}

void Scene::upload_mesh_accels() {
    setup_traversal_buffers();

    //debug_nodes_gpu(nodes_.data(), top_nodes_.data(), instance_nodes_.data(), nodes_.size(), top_nodes_.size(), instance_nodes_.size());

    thorin_copy(0, nodes_.data(), 0,
                traversal_.nodes.device(), traversal_.nodes.data(), 0,
                sizeof(Node) * nodes_.size());
    thorin_copy(0, tris_.data(), 0,
                traversal_.tris.device(), traversal_.tris.data(), 0,
                sizeof(Vec4) * tris_.size());

    // Release the memory associated with the triangles and nodes

}

void Scene::upload_top_level_accel() {
    setup_traversal_buffers();

    // TODO if dynamic changes are allowed, the first node (root node)
    //      has to be updated as well.

    thorin_copy(0, top_nodes_.data(), 0,
                traversal_.nodes.device(), traversal_.nodes.data(), sizeof(Node) * nodes_.size(),
                sizeof(Node) * top_nodes_.size());
    thorin_copy(0, instance_nodes_.data(), 0,
                traversal_.instances.device(), traversal_.instances.data(), 0,
                sizeof(InstanceNode) * instance_nodes_.size());

    // Release the memory associated with the top-level nodes
    std::vector<Node>().swap(top_nodes_);
    std::vector<Node>().swap(nodes_);
    std::vector<Vec4>().swap(tris_);
}

void Scene::compute_bounding_sphere() {
    // We use a box as an approximation
    BBox scene_bb = BBox::empty();
    for (size_t i = 0; i < meshes_.size(); i++) {
        scene_bb.extend(meshes_[i].bounding_box());
    }
    const float radius = length(scene_bb.max - scene_bb.min) * 0.5f;
    sphere_.inv_radius_sqr = 1.0f / sqr(radius);
    sphere_.radius = radius;
    sphere_.center = (scene_bb.max + scene_bb.min) * 0.5f;
}

} // namespace imba

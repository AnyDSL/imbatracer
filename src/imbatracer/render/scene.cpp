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
    if (traversal_.indices.size() < index_buf_.size()) {
        traversal_.indices = thorin::Array<int>(TRAVERSAL_PLATFORM, TRAVERSAL_DEVICE, index_buf_.size());
    }
    if (traversal_.texcoords.size() < texcoord_buf_.size()) {
        traversal_.texcoords = thorin::Array<Vec2>(TRAVERSAL_PLATFORM, TRAVERSAL_DEVICE, texcoord_buf_.size());
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

    // Copy all texture coordinates and indices into one huge array.
    texcoord_layout_.clear();
    index_layout_.clear();
    for (auto& m : meshes_) {
        auto texcoords = m.attribute<float2>(MeshAttributes::TEXCOORDS);
        texcoord_layout_.push_back(texcoord_buf_.size());
        texcoord_buf_.resize(texcoord_buf_.size() + m.vertex_count());
        for (int i = 0; i < m.vertex_count(); ++i) {
            texcoord_buf_[i].x = texcoords[i].x;
            texcoord_buf_[i].y = texcoords[i].y;
        }

        auto indices = m.indices();
        index_layout_.push_back(index_buf_.size());
        index_buf_.resize(index_buf_.size() + m.index_count());
        for (int i = 0; i < m.index_count(); ++i) {
            index_buf_[i] = indices[i];
        }
    }
}

void Scene::build_top_level_accel() {
    assert(!layout_.empty());

    top_nodes_.clear();

    auto adapter = new_top_level_adapter(top_nodes_, instance_nodes_);
    adapter->build_accel(meshes_, instances_, layout_, texcoord_layout_, index_layout_, nodes_.size());

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

    thorin_copy(0, nodes_.data(), 0,
                traversal_.nodes.device(), traversal_.nodes.data(), 0,
                sizeof(Node) * nodes_.size());
    thorin_copy(0, tris_.data(), 0,
                traversal_.tris.device(), traversal_.tris.data(), 0,
                sizeof(Vec4) * tris_.size());

    thorin_copy(0, index_buf_.data(), 0,
                traversal_.indices.device(), traversal_.indices.data(), 0,
                sizeof(int) * index_buf_.size());

    thorin_copy(0, texcoord_buf_.data(), 0,
                traversal_.texcoords.device(), traversal_.texcoords.data(), 0,
                sizeof(Vec2) * texcoord_buf_.size());

    // Release the memory associated with the triangles and nodes
    node_count_ = nodes_.size();
    std::vector<Node>().swap(nodes_);
    std::vector<Vec4>().swap(tris_);
    std::vector<Vec2>().swap(texcoord_buf_);
    std::vector<int >().swap(index_buf_);
}

void Scene::upload_top_level_accel() {
    setup_traversal_buffers();

    // TODO if dynamic changes are allowed, the first node (root node)
    //      has to be updated as well.

    thorin_copy(0, top_nodes_.data(), 0,
                traversal_.nodes.device(), traversal_.nodes.data(), sizeof(Node) * node_count_,
                sizeof(Node) * top_nodes_.size());
    thorin_copy(0, instance_nodes_.data(), 0,
                traversal_.instances.device(), traversal_.instances.data(), 0,
                sizeof(InstanceNode) * instance_nodes_.size());

    // Release the memory associated with the top-level nodes
    std::vector<Node>().swap(top_nodes_);
    std::vector<InstanceNode>().swap(instance_nodes_);
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

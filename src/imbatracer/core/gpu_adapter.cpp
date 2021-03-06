#include <cstring>

#include "imbatracer/core/adapter.h"
#include "imbatracer/core/sbvh_builder.h"
#include "imbatracer/core/fast_bvh_builder.h"
#include "imbatracer/core/mesh.h"
#include "imbatracer/core/stack.h"
#include "imbatracer/core/common.h"

namespace imba {

using traversal_gpu::Node;

static void fill_dummy_parent(Node& node, const BBox& leaf_bb, int index) {
    node.left  = index;
    node.right = 0x76543210;

    node.left_bb.lo_x = leaf_bb.min.x;
    node.left_bb.lo_y = leaf_bb.min.y;
    node.left_bb.lo_z = leaf_bb.min.z;
    node.left_bb.hi_x = leaf_bb.max.x;
    node.left_bb.hi_y = leaf_bb.max.y;
    node.left_bb.hi_z = leaf_bb.max.z;

    node.right_bb.lo_x = 0.0f;
    node.right_bb.lo_y = 0.0f;
    node.right_bb.lo_z = 0.0f;
    node.right_bb.hi_x = -0.0f;
    node.right_bb.hi_y = -0.0f;
    node.right_bb.hi_z = -0.0f;
}

class GpuMeshAdapter : public MeshAdapter {
    std::vector<Node>& nodes_;
    std::vector<Vec4>& tris_;
public:
    GpuMeshAdapter(std::vector<Node>& nodes, std::vector<Vec4>& tris)
        : nodes_(nodes), tris_(tris)
    {}

    void build_accel(const Mesh& mesh, int mesh_id, const std::vector<int>& tri_layout) override {
        mesh_ = &mesh;
        builder_.build(mesh, NodeWriter(this), LeafWriter(this, mesh_id, tri_layout), 2);
    }

#ifdef STATISTICS
    void print_stats() const override { builder_.print_stats(); }
#endif

private:
    struct CostFn {
        static float leaf_cost(int count, float area) {
            return count * area;
        }
        static float traversal_cost(float area) {
            return area * 1.0f;
        }
    };

    typedef SplitBvhBuilder<2, CostFn> BvhBuilder;

    struct NodeWriter {
        GpuMeshAdapter* adapter;

        NodeWriter(GpuMeshAdapter* adapter)
            : adapter(adapter)
        {}

        template <typename BBoxFn>
        void operator() (const BBox& parent_bb, int count, BBoxFn bboxes) {
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;

            int i = nodes.size();
            nodes.emplace_back();

            if (!stack.is_empty()) {
                StackElem elem = stack.pop();
                *(&nodes[elem.parent].left + elem.child) = i;
            }

            assert(count == 2);

            const BBox& left_bb = bboxes(0);
            nodes[i].left_bb.lo_x = left_bb.min.x;
            nodes[i].left_bb.lo_y = left_bb.min.y;
            nodes[i].left_bb.lo_z = left_bb.min.z;
            nodes[i].left_bb.hi_x = left_bb.max.x;
            nodes[i].left_bb.hi_y = left_bb.max.y;
            nodes[i].left_bb.hi_z = left_bb.max.z;

            const BBox& right_bb = bboxes(1);
            nodes[i].right_bb.lo_x = right_bb.min.x;
            nodes[i].right_bb.lo_y = right_bb.min.y;
            nodes[i].right_bb.lo_z = right_bb.min.z;
            nodes[i].right_bb.hi_x = right_bb.max.x;
            nodes[i].right_bb.hi_y = right_bb.max.y;
            nodes[i].right_bb.hi_z = right_bb.max.z;

            stack.push(i, 1);
            stack.push(i, 0);
        }
    };

    struct LeafWriter {
        GpuMeshAdapter* adapter;
        int mesh_id;
        const std::vector<int>& tri_layout;

        LeafWriter(GpuMeshAdapter* adapter, int mesh_id, const std::vector<int>& tri_layout)
            : adapter(adapter), mesh_id(mesh_id), tri_layout(tri_layout)
        {}

        template <typename RefFn>
        void operator() (const BBox& leaf_bb, int ref_count, RefFn refs) {
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;
            auto& tris = adapter->tris_;
            auto  mesh = adapter->mesh_;

            if (stack.is_empty()) {
                nodes.emplace_back();
                fill_dummy_parent(nodes.back(), leaf_bb, ~tris.size());
            } else {
                const StackElem& elem = stack.pop();
                *(&nodes[elem.parent].left + elem.child) = ~tris.size();
            }

            for (int i = 0; i < ref_count; i++) {
                const int ref = refs(i);
                const Tri& tri = mesh->triangle(ref);
                Vec4 v0 = { tri.v0.x, tri.v0.y, tri.v0.z, 0.0f };
                Vec4 v1 = { tri.v1.x, tri.v1.y, tri.v1.z, int_as_float(ref + tri_layout[mesh_id]) };
                Vec4 v2 = { tri.v2.x, tri.v2.y, tri.v2.z, 0.0f };
                tris.emplace_back(v0);
                tris.emplace_back(v1);
                tris.emplace_back(v2);
            }

            // Add sentinel
            tris.back().w = int_as_float(0x80000000);
        }
    };

    struct StackElem {
        int parent, child;
        StackElem() {}
        StackElem(int parent, int child) : parent(parent), child(child) {}
    };

    Stack<StackElem> stack_;
    const Mesh* mesh_;
    BvhBuilder builder_;
};

class GpuTopLevelAdapter : public TopLevelAdapter {
    std::vector<Node>& nodes_;
    std::vector<InstanceNode>& instance_nodes_;
public:
    GpuTopLevelAdapter(std::vector<Node>& nodes, std::vector<InstanceNode>& instance_nodes)
        : nodes_(nodes), instance_nodes_(instance_nodes)
    {}

    void build_accel(const std::vector<Mesh>& meshes,
                     const std::vector<Mesh::Instance>& instances,
                     const std::vector<int>& layout,
                     int root_offset) override {
        // Copy the bounding boxes and centers of all meshes into an array.
        std::vector<BBox> bounds(instances.size());
        std::vector<float3> centers(instances.size());
        for (int i = 0; i < instances.size(); ++i) {
            auto bb = meshes[instances[i].id].bounding_box();

            centers[i] = instances[i].mat * float4((bb.max + bb.min) * 0.5f, 1.0f);
            float3 abs_ext = abs(instances[i].mat) * float4((bb.max - bb.min) * 0.5f, 0.0f);

            bounds[i].min = centers[i] - abs_ext;
            bounds[i].max = centers[i] + abs_ext;
        }

        // Build the acceleration structure.
        builder_.build(bounds.data(), centers.data(), instances.size(),
            NodeWriter(this, meshes, instances, root_offset),
            LeafWriter(this, meshes, instances, layout), 1);
    }

private:
#ifdef STATISTICS
    void print_stats() const override { builder_.print_stats(); }
#endif

    struct CostFn {
        static float leaf_cost(int count, float area) {
            return count * area;
        }
        static float traversal_cost(float area) {
            return area * 0.5f;
        }
    };

    typedef FastBvhBuilder<2, CostFn> BvhBuilder;

    struct NodeWriter {
        GpuTopLevelAdapter* adapter;
        const std::vector<Mesh>& meshes;
        const std::vector<Mesh::Instance>& instances;
        const int root_offset;

        NodeWriter(GpuTopLevelAdapter* adapter, const std::vector<Mesh>& meshes,
            const std::vector<Mesh::Instance>& instances, int root_offset)
            : adapter(adapter), meshes(meshes), instances(instances), root_offset(root_offset)
        {}

        template <typename BBoxFn>
        void operator() (const BBox& parent_bb, int count, BBoxFn bboxes) {
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;

            int i = nodes.size();
            nodes.emplace_back();

            if (!stack.is_empty()) {
                StackElem elem = stack.pop();
                *(&nodes[elem.parent].left + elem.child) = i + root_offset;
            }

            assert(count == 2);

            const BBox& left_bb = bboxes(0);
            nodes[i].left_bb.lo_x = left_bb.min.x;
            nodes[i].left_bb.lo_y = left_bb.min.y;
            nodes[i].left_bb.lo_z = left_bb.min.z;
            nodes[i].left_bb.hi_x = left_bb.max.x;
            nodes[i].left_bb.hi_y = left_bb.max.y;
            nodes[i].left_bb.hi_z = left_bb.max.z;

            const BBox& right_bb = bboxes(1);
            nodes[i].right_bb.lo_x = right_bb.min.x;
            nodes[i].right_bb.lo_y = right_bb.min.y;
            nodes[i].right_bb.lo_z = right_bb.min.z;
            nodes[i].right_bb.hi_x = right_bb.max.x;
            nodes[i].right_bb.hi_y = right_bb.max.y;
            nodes[i].right_bb.hi_z = right_bb.max.z;

            stack.push(i, 1);
            stack.push(i, 0);
        }
    };

    struct LeafWriter {
        GpuTopLevelAdapter* adapter;
        const std::vector<Mesh>& meshes;
        const std::vector<Mesh::Instance>& instances;
        const std::vector<int>& layout;

        LeafWriter(GpuTopLevelAdapter* adapter, const std::vector<Mesh>& meshes,
            const std::vector<Mesh::Instance>& instances, const std::vector<int>& layout)
            : adapter(adapter), meshes(meshes), instances(instances), layout(layout)
        {}

        template <typename RefFn>
        void operator() (const BBox& leaf_bb, int ref_count, RefFn refs) {
            auto& instance_nodes = adapter->instance_nodes_;
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;

            if (stack.is_empty()) {
                nodes.emplace_back();
                fill_dummy_parent(nodes.back(), leaf_bb, ~instance_nodes.size());
            } else {
                // Link the node as the child node of the parent.
                const StackElem& elem = stack.pop();
                *(&nodes[elem.parent].left + elem.child) = ~instance_nodes.size(); // Negative values mark leaf nodes.
            }

            for (int j = 0; j < ref_count; ++j) {
                int inst_idx = refs(j);
                Mesh::Instance inst = instances[inst_idx];

                // Create an instance node.
                int i = instance_nodes.size();
                instance_nodes.emplace_back();

                // Write instance data to the node.
                auto& inst_node = instance_nodes[i];
                memcpy(&inst_node.transf, &inst.inv_mat, sizeof(inst_node.transf));
                inst_node.id = inst_idx; // id
                inst_node.next = layout[inst.id]; // sub-bvh
                inst_node.pad[0] = 0;
                inst_node.pad[1] = 0;
            }

            // Write sentinel value
            instance_nodes.back().pad[0] = -1;
            instance_nodes.back().pad[1] = -1;
        }
    };

    struct StackElem {
        int parent, child;
        StackElem() {}
        StackElem(int parent, int child) : parent(parent), child(child) {}
    };

    Stack<StackElem> stack_;
    const Mesh* mesh_;
    BvhBuilder builder_;
};

std::unique_ptr<MeshAdapter> new_mesh_adapter_gpu(std::vector<Node>& nodes, std::vector<Vec4>& tris) {
    return std::unique_ptr<MeshAdapter>(new GpuMeshAdapter(nodes, tris));
}

std::unique_ptr<TopLevelAdapter> new_top_level_adapter_gpu(std::vector<Node>& nodes, std::vector<InstanceNode>& instance_nodes) {
    return std::unique_ptr<TopLevelAdapter>(new GpuTopLevelAdapter(nodes, instance_nodes));
}

} // namespace imba

#include <cstring>

#include "adapter.h"
#include "sbvh_builder.h"
#include "fast_bvh_builder.h"
#include "mesh.h"
#include "stack.h"
#include "common.h"

namespace imba {

class GpuMeshAdapter : public MeshAdapter {
public:
    GpuMeshAdapter(std::vector<Node>& nodes, std::vector<Vec4>& tris)
        : MeshAdapter(nodes, tris)
    {}

    void build_accel(const Mesh& mesh) override {
        mesh_ = &mesh;
        builder_.build(mesh, NodeWriter(this), LeafWriter(this), 2);
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

            if (!stack.empty()) {
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

        LeafWriter(GpuMeshAdapter* adapter)
            : adapter(adapter)
        {}

        template <typename RefFn>
        void operator() (const BBox& leaf_bb, int ref_count, RefFn refs) {
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;
            auto& tris = adapter->tris_;
            auto  mesh = adapter->mesh_;

            const StackElem& elem = stack.pop();
            *(&nodes[elem.parent].left + elem.child) = ~tris.size();

            for (int i = 0; i < ref_count; i++) {
                const int ref = refs(i);
                const Tri& tri = mesh->triangle(ref);
                Vec4 v0 = { tri.v0.x, tri.v0.y, tri.v0.z, 0.0f };
                Vec4 v1 = { tri.v1.x, tri.v1.y, tri.v1.z, int_as_float(ref) };
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
public:
    GpuTopLevelAdapter(std::vector<Node>& nodes, std::vector<InstanceNode>& instance_nodes)
        : TopLevelAdapter(nodes, instance_nodes)
    {}

    virtual void build_accel(const std::vector<Mesh>& meshes,
                             const std::vector<Mesh::Instance>& instances,
                             const std::vector<int>& layout,
                             int root_offset) override {
        // Copy the bounding boxes and centers of all meshes into an array.
        std::vector<BBox> bounds(instances.size());
        std::vector<float3> centers(instances.size());
        for (int i = 0; i < instances.size(); ++i) {
            auto bb = meshes[instances[i].id].bounding_box();

            centers[i] = transform_point (instances[i].mat, (bb.max + bb.min) * 0.5f);
            float3 abs_ext = transform_vector(abs(instances[i].mat), (bb.max - bb.min) * 0.5f);

            bounds[i].min = centers[i] - abs_ext;
            bounds[i].max = centers[i] + abs_ext;
        }

        // Build the acceleration structure.
        builder_.build(bounds.data(), centers.data(), instances.size(),
            NodeWriter(this, meshes, instances, layout, root_offset),
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
        const std::vector<int>& layout;
        const int root_offset;

        NodeWriter(GpuTopLevelAdapter* adapter, const std::vector<Mesh>& meshes,
            const std::vector<Mesh::Instance>& instances, const std::vector<int>& layout, int root_offset)
            : adapter(adapter), meshes(meshes), instances(instances), layout(layout), root_offset(root_offset)
        {}

        template <typename BBoxFn>
        void operator() (const BBox& parent_bb, int count, BBoxFn bboxes) {
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;

            int i = nodes.size();
            nodes.emplace_back();

            if (!stack.empty()) {
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

            if (stack.empty()) {
                // No parent node was created, because there were too few primitives and no split was performed.
                // Create and link a parent node.
                int i = nodes.size();
                nodes.emplace_back();
                nodes[i].left  = ~instance_nodes.size();
                nodes[i].right = ~instance_nodes.size();

                nodes[i].left_bb.lo_x = leaf_bb.min.x;
                nodes[i].left_bb.lo_y = leaf_bb.min.y;
                nodes[i].left_bb.lo_z = leaf_bb.min.z;
                nodes[i].left_bb.hi_x = leaf_bb.max.x;
                nodes[i].left_bb.hi_y = leaf_bb.max.y;
                nodes[i].left_bb.hi_z = leaf_bb.max.z;

                nodes[i].right_bb.lo_x = leaf_bb.min.x;
                nodes[i].right_bb.lo_y = leaf_bb.min.y;
                nodes[i].right_bb.lo_z = leaf_bb.min.z;
                nodes[i].right_bb.hi_x = leaf_bb.max.x;
                nodes[i].right_bb.hi_y = leaf_bb.max.y;
                nodes[i].right_bb.hi_z = leaf_bb.max.z;
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
            }

            // Write sentinel value
            instance_nodes.back().pad[0] = 0x00ABABAB;
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

std::unique_ptr<MeshAdapter> new_mesh_adapter(std::vector<Node>& nodes, std::vector<Vec4>& tris) {
    return std::unique_ptr<MeshAdapter>(new GpuMeshAdapter(nodes, tris));
}

std::unique_ptr<TopLevelAdapter> new_top_level_adapter(std::vector<Node>& nodes, std::vector<InstanceNode>& instance_nodes) {
    return std::unique_ptr<TopLevelAdapter>(new GpuTopLevelAdapter(nodes, instance_nodes));
}

} // namespace imba

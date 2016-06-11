#include "adapter.h"
#include "fast_bvh_builder.h"
#include "mesh.h"
#include "stack.h"
#include "common.h"

#include <traversal.h>

#include <vector>
#include <cstring>

namespace imba {

class CpuTopLevelAdapter : public TopLevelAdapter {
public:
    CpuTopLevelAdapter(std::vector<Node>& nodes, std::vector<InstanceNode>& instance_nodes)
        : TopLevelAdapter(nodes, instance_nodes)
    {}

    virtual void build_accel(const std::vector<Mesh>& meshes,
                             const std::vector<Mesh::Instance>& instances,
                             const std::vector<int>& layout) override {
        // Copy the bounding boxes and centers of all meshes into an array.
        std::vector<BBox> bounds(instances.size());
        std::vector<float3> centers(instances.size());
        for (int i = 0; i < instances.size(); ++i) {
            bounds[i] = meshes[instances[i].id].bounding_box();
            bounds[i].min = transform_point(instances[i].mat, bounds[i].min);
            bounds[i].max = transform_point(instances[i].mat, bounds[i].max);

            centers[i] = (bounds[i].min + bounds[i].max) * 0.5f;
        }

        // Build the acceleration structure.
        builder_.build(bounds.data(), centers.data(), instances.size(),
            NodeWriter(this, meshes, instances, layout),
            LeafWriter(this, meshes, instances, layout), 1);
    }

private:
#ifdef STATISTICS
    void print_stats() const override { builder_.print_stats(); }
#endif

    struct CostFn {
        static float leaf_cost(int count, float area) {
            return ((count - 1) / 4 + 1) * area;
        }
        static float traversal_cost(float area) {
            return area * 0.5f;
        }
    };

    typedef FastBvhBuilder<4, CostFn> BvhBuilder;

    struct NodeWriter {
        CpuTopLevelAdapter* adapter;
        const std::vector<Mesh>& meshes;
        const std::vector<Mesh::Instance>& instances;
        const std::vector<int>& layout;

        NodeWriter(CpuTopLevelAdapter* adapter, const std::vector<Mesh>& meshes,
            const std::vector<Mesh::Instance>& instances, const std::vector<int>& layout)
            : adapter(adapter), meshes(meshes), instances(instances), layout(layout)
        {}

        template <typename BBoxFn>
        void operator() (const BBox& parent_bb, int count, BBoxFn bboxes) {
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;

            int i = nodes.size();
            nodes.emplace_back();

            if (!stack.empty()) {
                StackElem elem = stack.pop();
                nodes[elem.parent].children[elem.child] = i;
            }

            assert(count >= 2 && count <= 4);

            for (int j = count - 1; j >= 0; j--) {
                const BBox& bbox = bboxes(j);
                nodes[i].min_x[j] = bbox.min.x;
                nodes[i].min_y[j] = bbox.min.y;
                nodes[i].min_z[j] = bbox.min.z;

                nodes[i].max_x[j] = bbox.max.x;
                nodes[i].max_y[j] = bbox.max.y;
                nodes[i].max_z[j] = bbox.max.z;

                stack.push(i, j);
            }

            for (int j = 3; j >= count; j--) {
                nodes[i].min_x[j] = FLT_MAX;
                nodes[i].min_y[j] = FLT_MAX;
                nodes[i].min_z[j] = FLT_MAX;

                nodes[i].max_x[j] = -FLT_MAX;
                nodes[i].max_y[j] = -FLT_MAX;
                nodes[i].max_z[j] = -FLT_MAX;

                nodes[i].children[j] = 0;
            }
        }
    };

    struct LeafWriter {
        CpuTopLevelAdapter* adapter;
        const std::vector<Mesh>& meshes;
        const std::vector<Mesh::Instance>& instances;
        const std::vector<int>& layout;

        LeafWriter(CpuTopLevelAdapter* adapter, const std::vector<Mesh>& meshes,
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
                nodes[i].children[0] = ~instance_nodes.size();
                nodes[i].children[1] = 0;

                nodes[i].min_x[0] = leaf_bb.min.x;
                nodes[i].min_y[0] = leaf_bb.min.y;
                nodes[i].min_z[0] = leaf_bb.min.z;

                nodes[i].max_x[0] = leaf_bb.max.x;
                nodes[i].max_y[0] = leaf_bb.max.y;
                nodes[i].max_z[0] = leaf_bb.max.z;
            } else {
                // Link the node as the child node of the parent.
                const StackElem& elem = stack.pop();
                nodes[elem.parent].children[elem.child] = ~instance_nodes.size(); // Negative values mark leaf nodes.
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

std::unique_ptr<TopLevelAdapter> new_top_level_adapter(std::vector<Node>& nodes, std::vector<InstanceNode>& instance_nodes) {
    return std::unique_ptr<TopLevelAdapter>(new CpuTopLevelAdapter(nodes, instance_nodes));
}

} // namespace imba
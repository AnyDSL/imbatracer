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
    CpuTopLevelAdapter(std::vector<Node>& nodes)
        : TopLevelAdapter(nodes)
    {}

    virtual void build_accel(const std::vector<Mesh>& meshes,
                             const std::vector<Mesh::Instance>& instances,
                             const std::vector<int>& layout) override {
        // Copy the bounding boxes and centers of all meshes into an array.
        std::vector<BBox> bounds(meshes.size());
        std::vector<float3> centers(meshes.size());
        for (int i = 0; i < meshes.size(); ++i) {
            bounds[i] = meshes[i].bounding_box();
            centers[i] = (bounds[i].min + bounds[i].max) * 0.5f;
        }

        // Build the acceleration structure.
        builder_.build(bounds.data(), centers.data(), meshes.size(),
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
            }

            if (count < 4) {
                nodes[i].children[count] = 0;
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

        struct InstNode {
            float4 transf[3];
            int id;
            int next;
        };

        template <typename RefFn>
        void operator() (const BBox& leaf_bb, int ref_count, RefFn refs) {
            assert(ref_count == 1); // Leaves must contain exactly one primitive, i.e. one instance.

            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;

            int inst_idx = refs(0);
            Mesh::Instance inst = instances[inst_idx];

            // Create a leaf node.
            int i = nodes.size();
            nodes.emplace_back();

            // Write instance data to the node.
            InstNode* inst_node = reinterpret_cast<InstNode*>(nodes.data() + i);
            memcpy(inst_node->transf, &inst.mat, sizeof(inst_node->transf));
            inst_node->id = inst_idx; // id
            inst_node->next = layout[inst.id]; // sub-bvh

            // Link the node as the child node of the parent.
            const StackElem& elem = stack.pop();
            nodes[elem.parent].children[elem.child] = ~i; // Negative values mark leaf nodes.
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

std::unique_ptr<TopLevelAdapter> new_top_level_adapter(std::vector<Node>& nodes) {
    return std::unique_ptr<TopLevelAdapter>(new CpuTopLevelAdapter(nodes));
}

} // namespace imba
#include "adapter.h"
#include "sbvh_builder.h"
#include "mesh.h"
#include "stack.h"
#include "common.h"

namespace imba {

class GpuAdapter : public Adapter {
public:
    GpuAdapter(std::vector<Node>& nodes, std::vector<Vec4>& tris)
        : Adapter(nodes, tris)
    {}

    void build_accel(const Mesh& mesh) override {
        mesh_ = &mesh;
        builder_.build(mesh, NodeWriter(this), LeafWriter(this), 2, 1e-5f);
#ifdef STATISTICS
        builder_.print_stats();
#endif
    }

private:
    struct CostFn {
        static float split_cost(int left_count, float left_area, int right_count, float right_area) {
            return left_count * left_area + right_count * right_area;
        }
        static float leaf_cost(int count, float area) {
            return count * area;
        }
        static float traversal_cost(float area) {
            return area * 1.0f;
        }
    };

    struct NodeWriter {
        GpuAdapter* adapter;

        NodeWriter(GpuAdapter* adapter)
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
        typedef SplitBvhBuilder<2, CostFn>::Ref Ref;

        GpuAdapter* adapter;

        LeafWriter(GpuAdapter* adapter)
            : adapter(adapter)
        {}

        void operator() (const BBox& leaf_bb, const Ref* refs, int ref_count) {
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;
            auto& tris = adapter->tris_;
            auto  mesh = adapter->mesh_;

            const StackElem& elem = stack.pop();
            *(&nodes[elem.parent].left + elem.child) = ~tris.size();

            for (int i = 0; i < ref_count; i++) {
                const Tri& tri = mesh->triangle(refs[i].id);
                Vec4 v0 = { tri.v0.x, tri.v0.y, tri.v0.z, 0.0f };
                Vec4 v1 = { tri.v1.x, tri.v1.y, tri.v1.z, int_as_float(refs[i].id) };
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
    SplitBvhBuilder<2, CostFn> builder_;
};

std::unique_ptr<Adapter> new_adapter(std::vector<Node>& nodes, std::vector<Vec4>& tris) {
    return std::unique_ptr<Adapter>(new GpuAdapter(nodes, tris));
}

} // namespace imba

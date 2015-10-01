#include "adapter.h"
#include "sbvh_builder.h"
#include "mesh.h"
#include "stack.h"
#include "common.h"

namespace imba {

class CpuAdapter : public Adapter {
public:
    CpuAdapter(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris)
        : Adapter(nodes, tris)
    {}

    void build_accel(const Mesh& mesh) override {
        mesh_ = &mesh;
        builder_.build(mesh, NodeWriter(this), LeafWriter(this), 2, 1e-4f);
#ifdef STATISTICS
        builder_.print_stats();
#endif
    }

private:
    struct CostFn {
        static float split_cost(int left_count, float left_area, int right_count, float right_area) {
            return ((left_count  - 1) / 4 + 1) * left_area +
                   ((right_count - 1) / 4 + 1) * right_area;
        }
        static float leaf_cost(int count, float area) {
            return ((count - 1) / 4 + 1) * area;
        }
        static float traversal_cost(float area) {
            return area * 1.0f;
        }
    };

    struct NodeWriter {
        CpuAdapter* adapter;

        NodeWriter(CpuAdapter* adapter)
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
                nodes[elem.parent].children[elem.child] = i;
            }

            assert(count == 2);

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
        }
    };

    struct LeafWriter {
        typedef SplitBvhBuilder<4, CostFn>::Ref Ref;

        CpuAdapter* adapter;

        LeafWriter(CpuAdapter* adapter)
            : adapter(adapter)
        {}

        void operator() (const BBox& leaf_bb, const Ref* refs, int ref_count) {
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;
            auto& tris = adapter->tris_;
            auto  mesh = adapter->mesh_;

            const StackElem& elem = stack.pop();
            nodes[elem.parent].children[elem.child] = ~tris.size();

            // Group triangles by packets of 4
            for (int i = 0; i < ref_count; i += 4) {
                const int c = i + 4 <= ref_count ? 4 : ref_count - i;
                union{
                    struct {
                        Vec4 v0_x, v0_y, v0_z;
                        Vec4 e1_x, e1_y, e1_z;
                        Vec4 e2_x, e2_y, e2_z;
                        Vec4 n_x, n_y, n_z;
                        Vec4 ids;
                    } tri;
                    float raw[4 * 4 * 3];
                } data;

                for (int j = 0; j < c; j++) {
                    const Tri& tri = mesh->triangle(refs[i].id);
                    const float3 e1 = tri.v0 - tri.v1;
                    const float3 e2 = tri.v2 - tri.v0;
                    const float3 n = cross(e1, e2);
                    data.raw[j + 0] = tri.v0.x;
                    data.raw[j + 4] = tri.v0.y;
                    data.raw[j + 8] = tri.v0.z;

                    data.raw[j + 12] = e1.x;
                    data.raw[j + 16] = e1.y;
                    data.raw[j + 20] = e1.z;

                    data.raw[j + 24] = e2.x;
                    data.raw[j + 28] = e2.y;
                    data.raw[j + 32] = e2.z;

                    data.raw[j + 36] = n.x;
                    data.raw[j + 40] = n.y;
                    data.raw[j + 44] = n.z;

                    data.raw[j + 48] = int_as_float(refs[i].id);
                }

                for (int j = c; j < 4; j++) {
                    data.raw[j + 0] = 0.0f;
                    data.raw[j + 4] = 0.0f;
                    data.raw[j + 8] = 0.0f;
                    data.raw[j + 12] = 0.0f;
                    data.raw[j + 16] = 0.0f;
                    data.raw[j + 20] = 0.0f;
                    data.raw[j + 24] = 0.0f;
                    data.raw[j + 28] = 0.0f;
                    data.raw[j + 32] = 0.0f;
                    data.raw[j + 36] = 0.0f;
                    data.raw[j + 40] = 0.0f;
                    data.raw[j + 44] = 0.0f;
                    data.raw[j + 48] = int_as_float(-1);
                }

                tris.emplace_back(data.tri.v0_x);
                tris.emplace_back(data.tri.v0_y);
                tris.emplace_back(data.tri.v0_z);

                tris.emplace_back(data.tri.e1_x);
                tris.emplace_back(data.tri.e1_y);
                tris.emplace_back(data.tri.e1_z);

                tris.emplace_back(data.tri.e2_x);
                tris.emplace_back(data.tri.e2_y);
                tris.emplace_back(data.tri.e2_z);

                tris.emplace_back(data.tri.n_x);
                tris.emplace_back(data.tri.n_y);
                tris.emplace_back(data.tri.n_z);

                tris.emplace_back(data.tri.ids);
            }

            // Add sentinel
            const float s = int_as_float(0x80000000);
            Vec4 sentinel = { s, s, s, s };
            tris.emplace_back(sentinel);
        }
    };
    
    struct StackElem {
        int parent, child;
        StackElem() {}
        StackElem(int parent, int child) : parent(parent), child(child) {}
    };

    Stack<StackElem> stack_;
    const Mesh* mesh_;
    SplitBvhBuilder<4, CostFn> builder_;
};

std::unique_ptr<Adapter> new_adapter(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris) {
    return std::unique_ptr<Adapter>(new CpuAdapter(nodes, tris));
}

} // namespace imba

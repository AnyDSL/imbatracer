#include "adapter.h"
#include "bvh_builder.h"
#include "mesh.h"
#include "stack.h"

namespace imba {

struct StackElem {
    int parent, child;
    StackElem() {}
    StackElem(int parent, int child) : parent(parent), child(child) {}
};

class GpuAdapter : public Adapter {
public:
    GpuAdapter(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris)
        : Adapter(nodes, tris), builder_(NodeWriter(this), LeafWriter(this))
    {}

    void build_accel(const Mesh& mesh) override {
        mesh_ = &mesh;
        builder_.build(mesh);
    }

private:
    struct NodeWriter {
        GpuAdapter* adapter;

        NodeWriter(GpuAdapter* adapter)
            : adapter(adapter)
        {}

        void operator() (const BBox& parent_bb, const BBox& left_bb, const BBox& right_bb) {
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;

            int i = nodes.size();
            nodes.emplace_back();

            StackElem elem = stack.pop();
            *(&nodes[elem.parent].left + elem.child) = i;

            nodes[i].left_bb.lo_x = left_bb.min.x;
            nodes[i].left_bb.lo_y = left_bb.min.y;
            nodes[i].left_bb.lo_z = left_bb.min.z;
            nodes[i].left_bb.hi_x = left_bb.max.x;
            nodes[i].left_bb.hi_y = left_bb.max.y;
            nodes[i].left_bb.hi_z = left_bb.max.z;

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
        GpuAdapter* adapter;

        LeafWriter(GpuAdapter* adapter)
            : adapter(adapter)
        {}

        void operator() (const BBox& leaf_bb, const uint32_t* refs, int ref_count) {
            auto& nodes = adapter->nodes_;
            auto& stack = adapter->stack_;
            auto& tris = adapter->tris_;
            auto  mesh = adapter->mesh_;

            StackElem elem = stack.pop();
            *(&nodes[elem.parent].left + elem.child) = tris.size();

            const float* verts = mesh->vertices();
            const uint32_t* indices = mesh->indices();
            for (int i = 0; i < ref_count; i++) {
                const Tri& tri = mesh->triangle(refs[i]);
                Vec4 v0 = { tri.v0.x, tri.v0.y, tri.v0.z, 0.0f };
                Vec4 v1 = { tri.v1.x, tri.v1.y, tri.v1.z, 0.0f };
                Vec4 v2 = { tri.v2.x, tri.v2.y, tri.v2.z, 0.0f };
                tris.emplace_back(v0);
                tris.emplace_back(v1);
                tris.emplace_back(v2);
            }

            // Add sentinel
            tris.back().w = -0.0f;
        }
    };

    Stack<StackElem> stack_;
    const Mesh* mesh_;
    BvhBuilder builder_;
};

std::unique_ptr<Adapter> new_adapter(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris) {
    return std::unique_ptr<Adapter>(new GpuAdapter(nodes, tris));
}

} // namespace imba

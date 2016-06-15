#include <cstring>

#include "adapter.h"
#include "sbvh_builder.h"
#include "fast_bvh_builder.h"
#include "mesh.h"
#include "stack.h"
#include "common.h"

namespace imba {

class CpuMeshAdapter : public MeshAdapter {
public:
    CpuMeshAdapter(std::vector<Node>& nodes, std::vector<Vec4>& tris)
        : MeshAdapter(nodes, tris)
    {}

    void build_accel(const Mesh& mesh) override {
        mesh_ = &mesh;
        builder_.build(mesh, NodeWriter(this), LeafWriter(this), 2, 1e-4f);
    }

#ifdef STATISTICS
    void print_stats() const override { builder_.print_stats(); }
#endif

private:
    struct CostFn {
        static float leaf_cost(int count, float area) {
            return ((count - 1) / 4 + 1) * area;
        }
        static float traversal_cost(float area) {
            return area * 0.5f;
        }
    };

    typedef SplitBvhBuilder<4, CostFn> BvhBuilder;

    struct NodeWriter {
        CpuMeshAdapter* adapter;

        NodeWriter(CpuMeshAdapter* adapter)
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
        }
    };

    struct LeafWriter {
        CpuMeshAdapter* adapter;

        LeafWriter(CpuMeshAdapter* adapter)
            : adapter(adapter)
        {}

        template <typename RefFn>
        void operator() (const BBox& leaf_bb, int ref_count, RefFn refs) {
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
                    const int id = refs(i + j);
                    const Tri& tri = mesh->triangle(id);
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

                    data.raw[j + 48] = int_as_float(id);
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
                    data.raw[j + 48] = int_as_float(0x80000000);
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
    BvhBuilder builder_;
};

class CpuTopLevelAdapter : public TopLevelAdapter {
public:
    CpuTopLevelAdapter(std::vector<Node>& nodes, std::vector<InstanceNode>& instance_nodes)
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
            bounds[i] = meshes[instances[i].id].bounding_box();
            bounds[i].min = transform_point(instances[i].mat, bounds[i].min);
            bounds[i].max = transform_point(instances[i].mat, bounds[i].max);

            centers[i] = (bounds[i].min + bounds[i].max) * 0.5f;
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
        const int root_offset;

        NodeWriter(CpuTopLevelAdapter* adapter, const std::vector<Mesh>& meshes,
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
                nodes[elem.parent].children[elem.child] = i + root_offset;
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

std::unique_ptr<MeshAdapter> new_mesh_adapter(std::vector<Node>& nodes, std::vector<Vec4>& tris) {
    return std::unique_ptr<MeshAdapter>(new CpuMeshAdapter(nodes, tris));
}

std::unique_ptr<TopLevelAdapter> new_top_level_adapter(std::vector<Node>& nodes, std::vector<InstanceNode>& instance_nodes) {
    return std::unique_ptr<TopLevelAdapter>(new CpuTopLevelAdapter(nodes, instance_nodes));
}

} // namespace imba

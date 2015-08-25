#ifndef IMBA_BVH_BUILDER_H
#define IMBA_BVH_BUILDER_H

#include <cstdint>
#include <functional>
#include <cassert>
#include <chrono>
#include <iostream>

#include "mem_pool.h"
#include "float3.h"
#include "stack.h"
#include "split.h"
#include "mesh.h"
#include "bbox.h"
#include "tri.h"

#define STATISTICS

namespace imba {

/// Builds a SBVH (Spatial split BVH), given the set of triangles and the alpha parameter
/// that controls when to do a spatial split. The tree is built in depth-first order.
/// See  Stich et al., "Spatial Splits in Bounding Volume Hierarchies", 2009
/// http://www.nvidia.com/docs/IO/77714/sbvh.pdf
template <int N, typename NodeWriter, typename LeafWriter, bool LargestAxisOnly = false>
class BvhBuilder {
public:
    BvhBuilder(NodeWriter node, LeafWriter leaf)
        : write_node_(node), write_leaf_(leaf) {
        static_assert(N >= 2, "Incorrect number of children specified");
    }

    void build(const Mesh& mesh, int leaf_threshold = 2, float alpha = 1e-5f) {
        assert(leaf_threshold >= 1);

#ifdef STATISTICS
        total_tris_ += mesh.triangle_count();
        auto time_start = std::chrono::high_resolution_clock::now();
#endif

        const size_t tri_count = mesh.triangle_count();
        StdMemoryPool mem_pool(sizeof(uint32_t) * tri_count * 4 +
                               sizeof(BBox) * tri_count +
                               sizeof(float3) * tri_count);

        uint32_t* initial_refs = mem_pool.alloc<uint32_t>(tri_count);
        BBox* bboxes = mem_pool.alloc<BBox>(tri_count);
        float3* centroids = mem_pool.alloc<float3>(tri_count);
        BBox mesh_bb = BBox::empty();
        for (size_t i = 0; i < tri_count; i++) {
            const Tri& tri = mesh.triangle(i);
            tri.compute_bbox(bboxes[i]);
            mesh_bb.extend(bboxes[i]);
            centroids[i] = (tri.v0 + tri.v1 + tri.v2) * (1.0f / 3.0f);
            initial_refs[i] = i;
        }

        const float spatial_threshold = mesh_bb.half_area() * alpha;

        Stack<StackElem> stack;
        int node_count = 0;

        stack.push(initial_refs, tri_count, mesh_bb);

        while (!stack.empty()) {
            const StackElem& elem = stack.pop();
            uint32_t* refs = elem.refs;
            int ref_count = elem.ref_count;
            const BBox& parent_bb = elem.bbox;

            assert(ref_count != 0);

            if (ref_count <= leaf_threshold) {
                // When there are not enough triangles, make a leaf
                make_leaf(node_count == 0, parent_bb, refs, ref_count);
                continue;
            }

            // Find centroids bounds
            BBox center_bb(centroids[refs[0]]);
            for (int i = 1; i < ref_count; i++)
                center_bb.extend(centroids[refs[i]]);

            // Try object splits
            SplitCandidate split;
            for (int axis = 0; axis < 3; axis++) {
                if (center_bb.min[axis] == center_bb.max[axis])
                    continue;

                object_split(split, parent_bb, center_bb, axis, refs, ref_count, centroids, bboxes);
            }

            if (!split.is_empty() && BBox(split.left_bb).overlap(split.right_bb).half_area() > spatial_threshold) {
                // Try spatial splits
                for (int axis = 0; axis < 3; axis++) {
                    if (parent_bb.min[axis] == parent_bb.max[axis])
                        continue;

                    spatial_split(split, parent_bb, axis, refs, ref_count, mesh, bboxes);
                }
            }

            if (split.is_empty() || split.cost + 1 >= ref_count * parent_bb.half_area() ||
                split.left_bb.is_empty() || split.right_bb.is_empty()) {
                // The node cannot be split
                make_leaf(node_count == 0, parent_bb, refs, ref_count);
            } else {
                // Invariant : either the bounding boxes are smaller, or
                //             the number of primitives in the children has decreased.
                assert((split.left_bb.is_strictly_included(parent_bb) || split.left_count < ref_count) &&
                       (split.right_bb.is_strictly_included(parent_bb) || split.right_count < ref_count));
                assert(!split.left_bb.is_empty() && !split.right_bb.is_empty());
                make_node(parent_bb, split);
                node_count++;

                if (split.spatial) {
                    uint32_t* left_refs = mem_pool.alloc<uint32_t>(split.left_count);
                    uint32_t* right_refs = mem_pool.alloc<uint32_t>(split.right_count);

                    spatial_partition(split, refs, ref_count, left_refs, right_refs, bboxes);

                    stack.push(right_refs, split.right_count, split.right_bb);
                    stack.push(left_refs, split.left_count, split.left_bb);
                } else {
                    // Partitioning can be done in-place
                    object_partition(split, refs, ref_count, centroids);

                    stack.push(refs + split.left_count, split.right_count, split.right_bb);
                    stack.push(refs, split.left_count, split.left_bb);
                }
            }
        }

#ifdef STATISTICS
        auto time_end = std::chrono::high_resolution_clock::now();
        total_time_ += std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count();
#endif
    }

#ifdef STATISTICS
    void print_stats() const {
        std::cout << "BVH built in " << total_time_ << "ms ("
                  << total_nodes_ << " nodes, "
                  << total_leaves_ << " leaves, "
                  << object_splits_ << " object splits, "
                  << spatial_splits_ << " spatial splits, "
                  << "+" << (total_refs_ - total_tris_) * 100  / total_tris_ << "% references"
                  << std::endl;
    }
#endif

private:
    void make_empty(const BBox& parent_bb, const uint32_t* refs, int ref_count) {
        write_node_(parent_bb, parent_bb, BBox::empty());
        write_leaf_(parent_bb, refs, ref_count);
        write_leaf_(BBox::empty(), nullptr, 0);

#ifdef STATISTICS
        total_nodes_++;
        total_leaves_ += 2;
        total_refs_ += ref_count;
#endif
    }

    void make_node(const BBox& parent_bb, const SplitCandidate& split) {
        write_node_(parent_bb, split.left_bb, split.right_bb);

#ifdef STATISTICS
        if (split.spatial)
            spatial_splits_++;
        else
            object_splits_++;
        total_nodes_++;
#endif
    }

    void make_leaf(bool empty_bvh, const BBox& parent_bb, const uint32_t* refs, int ref_count) {
        if (!empty_bvh) {
            write_leaf_(parent_bb, refs, ref_count);

    #ifdef STATISTICS
            total_leaves_++;
            total_refs_ += ref_count;
    #endif
        } else {
            make_empty(parent_bb, refs, ref_count);
        }
    }

    struct StackElem {
        uint32_t* refs;
        int ref_count;
        BBox bbox;

        StackElem() {}
        StackElem(uint32_t* refs, int ref_count, const BBox& bbox)
            : refs(refs), ref_count(ref_count), bbox(bbox)
        {}
    };

#ifdef STATISTICS
    long total_time_ = 0;
    int total_nodes_ = 0;
    int total_leaves_ = 0;
    int total_refs_ = 0;
    int total_tris_ = 0;
    int spatial_splits_ = 0;
    int object_splits_ = 0;
#endif

    NodeWriter write_node_;
    LeafWriter write_leaf_;
};

} // namespace imba

#endif // IMBA_BVH_BUILDER_H

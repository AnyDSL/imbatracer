#include <algorithm>
#include <iostream>
#include <chrono>
#include "bvh_builder.h"
#include "split.h"
#include "mem_pool.h"
#include "mesh.h"
#include "stack.h"

#define STATISTICS

namespace imba {

void BvhBuilder::build(const Mesh& mesh, int leaf_threshold, float alpha) {
    // A memory pool ensures that allocation is fast (for spatial splits)
    assert(mesh.triangle_count() > 1 && leaf_threshold >= 1);

#ifdef STATISTICS
    int node_count = 0;
    int leaf_count = 0;
    int spc_splits = 0;
    int obj_splits = 0;
    int total_refs = 0;
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
        bboxes[i] = tri.bbox();
        centroids[i] = (tri.v0 + tri.v1 + tri.v2) * (1.0f / 3.0f);
        initial_refs[i] = i;
        mesh_bb = extend(mesh_bb, bboxes[i]);
    }

    Stack<StackElem> stack;
    stack.push(initial_refs, tri_count, mesh_bb);

    while (!stack.empty()) {
        const StackElem& elem = stack.pop();
        uint32_t* refs = elem.refs;
        int ref_count = elem.ref_count;
        const BBox& parent_bb = elem.bbox;

        if (ref_count <= leaf_threshold) {
            // When there are not enough triangles, make a leaf
            write_leaf_(parent_bb, refs, ref_count);
#ifdef STATISTICS
            leaf_count++;
            total_refs += ref_count;
#endif
            continue;
        }

        // Find centroids bounds
        BBox center_bb(centroids[refs[0]]);
        for (int i = 1; i < ref_count; i++)
            center_bb = extend(center_bb, centroids[refs[i]]);

        // Try object splits
        SplitCandidate best;
        for (int axis = 0; axis < 3; axis++) {
            if (center_bb.min[axis] == center_bb.max[axis])
                continue;

            SplitCandidate candidate = object_split(axis, center_bb.min[axis], center_bb.max[axis],
                                                    refs, ref_count, centroids, bboxes);
            if (candidate < best)
                best = candidate;
        }

        if (half_area(overlap(best.left_bb, best.right_bb)) >= alpha * half_area(parent_bb)) {
            // Try spatial splits
            BBox full_bb(bboxes[refs[0]]);
            for (int i = 1; i < ref_count; i++)
                full_bb = extend(full_bb, bboxes[refs[i]]);

            for (int axis = 0; axis < 3; axis++) {
                if (full_bb.min[axis] == full_bb.max[axis])
                    continue;

                SplitCandidate candidate = spatial_split(axis, full_bb.min[axis], full_bb.max[axis],
                                                         refs, ref_count, mesh, bboxes);
                if (candidate < best)
                    best = candidate;
            }
        }

        float split_cost = half_area(overlap(best.left_bb, parent_bb)) * best.left_count +
                           half_area(overlap(best.right_bb, parent_bb)) * best.right_count;
        if (best.empty() || (split_cost >= ref_count * half_area(parent_bb) && node_count > 1)) {
            // The node cannot be split
            write_leaf_(parent_bb, refs, ref_count);

#ifdef STATISTICS
            leaf_count++;
            total_refs += ref_count;
#endif
        } else {
            write_node_(parent_bb, best.left_bb, best.right_bb);

#ifdef STATISTICS
            if (best.spatial)
                spc_splits++;
            else
                obj_splits++;
            node_count++;
#endif

            if (best.spatial) {
                uint32_t* left_refs = mem_pool.alloc<uint32_t>(best.left_count);
                uint32_t* right_refs = mem_pool.alloc<uint32_t>(best.right_count);

                spatial_partition(best, refs, ref_count, left_refs, right_refs, bboxes);

                stack.push(right_refs, best.right_count, best.right_bb);
                stack.push(left_refs, best.left_count, best.left_bb);
            } else {
                // Partitioning can be done in-place
                object_partition(best, refs, ref_count, centroids);

                stack.push(refs + best.left_count, best.right_count, best.right_bb);
                stack.push(refs, best.left_count, best.left_bb);
            }
        }
    }

#ifdef STATISTICS
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - time_start);
    const int ref_increase = (total_refs - mesh.triangle_count()) * 100 / mesh.triangle_count();
    std::clog << "BVH built in " << duration.count() << "ms ("
              << node_count << " nodes, "
              << leaf_count << " leaves, "
              << obj_splits << " object splits, "
              << spc_splits << " spatial splits, "
              << "+" << ref_increase << "% references)"
              << std::endl;
#endif
}

} // namespace imba

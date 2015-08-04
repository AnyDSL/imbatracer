#include <algorithm>
#include "bvh_builder.h"
#include "split.h"
#include "mem_pool.h"
#include "mesh.h"
#include "stack.h"

namespace imba {

struct StackElem {
    uint32_t* refs;
    int ref_count;
    StackElem() {}
    StackElem(uint32_t* refs, int ref_count)
        : refs(refs), ref_count(ref_count)
    {}
};

void BvhBuilder::build(const Mesh& mesh, float alpha) {
    // A memory pool ensures that allocation is fast (for spatial splits)
    const size_t tri_count = mesh.triangle_count();
    StdMemoryPool mem_pool(sizeof(uint32_t) * tri_count * 4 +
                           sizeof(BBox) * tri_count +
                           sizeof(float3) * tri_count);

    uint32_t* initial_refs = mem_pool.alloc<uint32_t>(tri_count);
    BBox* bboxes = mem_pool.alloc<BBox>(tri_count);
    float3* centroids = mem_pool.alloc<float3>(tri_count);
    for (size_t i = 0; i < tri_count; i++) {
        const Tri& tri = mesh.triangle(i);
        bboxes[i] = tri.bbox();
        centroids[i] = (tri.v0 + tri.v1 + tri.v2) * (1.0f / 3.0f);
        initial_refs[i] = i;
    }

    Stack<StackElem> stack;
    stack.push(initial_refs, tri_count);

    while (!stack.empty()) {
        const StackElem& elem = stack.pop();
        uint32_t* refs = elem.refs;
        int ref_count = elem.ref_count;

        // Find centroids bounds
        BBox center_bb(centroids[refs[0]]);
        for (int i = 1; i < ref_count; i++)
            center_bb = extend(center_bb, centroids[refs[i]]);

        // Try object splits
        SplitCandidate best;
        for (int axis = 0; axis < 3; axis++) {
            SplitCandidate candidate = object_split(axis, center_bb.min[axis], center_bb.max[axis],
                                                    refs, ref_count, centroids, bboxes);
            if (candidate < best)
                best = candidate;
        }

        // Find parent bounds
        BBox parent_bb = bboxes[refs[0]];
        for (int i = 1; i < ref_count; i++)
            parent_bb = extend(parent_bb, bboxes[refs[i]]);

        if (half_area(overlap(best.left_bb, best.right_bb)) >= alpha * half_area(parent_bb)) {
            // Try spatial splits
            for (int axis = 0; axis < 3; axis++) {
                SplitCandidate candidate = spatial_split(axis, parent_bb.min[axis], parent_bb.max[axis],
                                                         refs, ref_count, mesh, bboxes);
                if (candidate < best)
                    best = candidate;
            }
        }

        if (best.empty() || best.cost >= elem.ref_count * half_area(parent_bb)) {
            // The node cannot be split
            write_leaf_(parent_bb, refs, ref_count);
        } else {
            write_node_(parent_bb, best.left_bb, best.right_bb);

            if (best.spatial) {
                uint32_t* left_refs = mem_pool.alloc<uint32_t>(best.left_count);
                uint32_t* right_refs = mem_pool.alloc<uint32_t>(best.right_count);
                spatial_partition(best, refs, ref_count, left_refs, right_refs, bboxes);

                stack.push(right_refs, best.right_count);
                stack.push(left_refs, best.left_count);
            } else {
                // Partitioning can be done in-place
                object_partition(best, refs, ref_count, centroids);

                stack.push(refs, best.left_count);
                stack.push(refs + best.left_count, ref_count - best.left_count);
            }
        }
    }
}

} // namespace imba

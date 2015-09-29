#ifndef IMBA_SBVH_BUILDER_H
#define IMBA_SBVH_BUILDER_H

#include <cstdint>
#include <functional>
#include <cassert>
#include <chrono>
#include <iostream>

#include "common.h"
#include "mem_pool.h"
#include "float3.h"
#include "stack.h"
#include "mesh.h"
#include "bbox.h"
#include "tri.h"

#define STATISTICS

namespace imba {

/// Builds a SBVH (Spatial split BVH), given the set of triangles and the alpha parameter
/// that controls when to do a spatial split. The tree is built in depth-first order.
/// See  Stich et al., "Spatial Splits in Bounding Volume Hierarchies", 2009
/// http://www.nvidia.com/docs/IO/77714/sbvh.pdf
template <int N, typename CostFn>
class SplitBvhBuilder {
public:
    struct Ref {
        uint32_t id;
        BBox bb;

        Ref() {}
        Ref(uint32_t id, const BBox& bb) : id(id), bb(bb) {}
    };

    /// Builds a SBVH with arity N
    template <typename NodeWriter, typename LeafWriter>
    void build(const Mesh& mesh, NodeWriter write_node, LeafWriter write_leaf, int leaf_threshold, float alpha) {
        assert(leaf_threshold >= 1);

#ifdef STATISTICS
        total_tris_ += mesh.triangle_count();
        auto time_start = std::chrono::high_resolution_clock::now();
#endif

        const int tri_count = mesh.triangle_count();

        mem_pool_.cleanup();

        Ref* initial_refs = mem_pool_.alloc<Ref>(tri_count);
        right_bbs_ = mem_pool_.alloc<BBox>(std::max((int)spatial_bins, tri_count));
        BBox mesh_bb = BBox::empty();
        for (size_t i = 0; i < tri_count; i++) {
            const Tri& tri = mesh.triangle(i);
            tri.compute_bbox(initial_refs[i].bb);
            mesh_bb.extend(initial_refs[i].bb);
            initial_refs[i].id = i;
        }

        // Create a one-leaf SBVH
        if (tri_count <= leaf_threshold) {
            write_node(mesh_bb, 1, [&] (int) {
                return mesh_bb;
            });
            for (int i = 1; i < N; i++)
                write_leaf(BBox::empty(), nullptr, 0);
            write_leaf(mesh_bb, initial_refs, tri_count);
#ifdef STATISTICS
            total_nodes_++;
            total_leaves_ += N;
            total_refs_ += tri_count;
#endif
            return;
        }

        const float spatial_threshold = mesh_bb.half_area() * alpha;

        Stack<SplitCandidate> stack;
        stack.push(initial_refs, tri_count, mesh_bb);

        while (!stack.empty()) {
            MultiNode multi_node(stack.pop());

            // Iterate over the available split candidates in the multi-node
            do {
                int candidate_id = multi_node.next_candidate();
                SplitCandidate candidate = multi_node.candidates[candidate_id];
                Ref* refs = candidate.refs;
                int ref_count = candidate.ref_count;
                const BBox& parent_bb = candidate.bbox;
                assert(N > 2 || ref_count != 0);

                if (ref_count <= leaf_threshold) {
                    // This candidate does not have enough triangles
                    multi_node.mark_candidate(candidate_id);
                    continue;
                }

                // Try object splits
                ObjectSplit object_split;
                for (int axis = 0; axis < 3; axis++)
                    find_object_split(object_split, axis, refs, ref_count);

                SpatialSplit spatial_split;
                if (BBox(object_split.left_bb).overlap(object_split.right_bb).half_area() > spatial_threshold) {
                    // Try spatial splits
                    for (int axis = 0; axis < 3; axis++) {
                        if (parent_bb.min[axis] == parent_bb.max[axis])
                            continue;
                        find_spatial_split(spatial_split, parent_bb, mesh, axis, refs, ref_count);
                    }
                }

                bool spatial = spatial_split.cost < object_split.cost;
                const float split_cost = spatial ? spatial_split.cost : object_split.cost;

                if (split_cost + CostFn::traversal_cost(candidate.bbox.half_area()) >= candidate.cost) {
                    // Split is not beneficial
                    multi_node.mark_candidate(candidate_id);
                    continue;
                }

                if (spatial) {
                    Ref* left_refs, *right_refs;
                    BBox left_bb, right_bb;
                    int left_count, right_count;
                    apply_spatial_split(spatial_split, mesh,
                                        refs, ref_count,
                                        left_refs, left_count, left_bb,
                                        right_refs, right_count, right_bb);

                    multi_node.split_candidate(candidate_id,
                                               SplitCandidate(left_refs,  left_count,  left_bb),
                                               SplitCandidate(right_refs, right_count, right_bb));

#ifdef STATISTICS
                    spatial_splits_++;
#endif
                } else {
                    // Partitioning can be done in-place
                    apply_object_split(object_split, refs, ref_count);

                    const int right_count = ref_count - object_split.left_count;
                    const int left_count = object_split.left_count;

                    Ref *right_refs = refs + object_split.left_count;
                    Ref* left_refs = refs;

                    multi_node.split_candidate(candidate_id,
                                               SplitCandidate(left_refs,  left_count,  object_split.left_bb),
                                               SplitCandidate(right_refs, right_count, object_split.right_bb));
#ifdef STATISTICS
                    object_splits_++;
#endif
                }
            } while (!multi_node.full() && multi_node.candidate_available());

            assert(multi_node.count > 0);

            // The multi-node is ready to be stored
            if (multi_node.is_leaf()) {
                // Store a leaf if it could not be split
                assert(multi_node.tested[0]);
                const SplitCandidate& c = multi_node.candidates[0];
                write_leaf(c.bbox, c.refs, c.ref_count);
#ifdef STATISTICS
                total_leaves_++;
                total_refs_ += c.ref_count;
#endif
            } else {
                // Store a multi-node
                write_node(multi_node.bbox, multi_node.count, [&] (int i) {
                    return multi_node.candidates[i].bbox;
                });

                for (int i = N - 1; i >= multi_node.count; i--) {
                    // Push empty leaves
                    stack.push(nullptr, 0, BBox::empty());
                }
                for (int i = multi_node.count - 1; i >= 0; i--) {
                    stack.push(multi_node.candidates[i]);
                }
#ifdef STATISTICS
                total_nodes_++;
#endif
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
                  << "+" << (total_refs_ - total_tris_) * 100  / total_tris_ << "% references)"
                  << std::endl;
    }
#endif

private:
    static constexpr int spatial_bins = 256;

    struct Bin {
        BBox bb;
        int entry;
        int exit;
    };

    struct ObjectSplit {
        int axis;
        float cost;
        BBox left_bb, right_bb;
        int left_count;

        ObjectSplit() : cost (FLT_MAX) {}
    };

    struct SpatialSplit {
        int axis;
        float cost;
        float position;

        SpatialSplit() : cost (FLT_MAX) {}
    };

    struct SplitCandidate {
        Ref* refs;
        int ref_count;
        BBox bbox;
        float cost;

        SplitCandidate() {}
        SplitCandidate(Ref* refs, int ref_count, const BBox& bbox)
            : refs(refs), ref_count(ref_count), bbox(bbox),
              cost(CostFn::leaf_cost(ref_count, bbox.half_area()))
        {}
    };

    struct MultiNode {
        SplitCandidate candidates[N];
        BBox bbox;
        bool tested[N];
        int count;
        int nodes;

        MultiNode(const SplitCandidate& split) {
            candidates[0] = split;
            tested[0] = false;
            bbox = split.bbox;
            count = 1;
            nodes = 0;
        }

        bool full() const { return count == N; }
        bool is_leaf() const { return count == 1; }

        int next_candidate() const {
            assert(candidate_available());
            if (N == 2)
                return 0;
            else {
                float min_cost = FLT_MAX;
                int min_idx = 0;
                for (int i = 0; i < count; i++) {
                    if (!tested && min_cost > candidates[i].cost) {
                        min_idx = i;
                        min_cost = candidates[i].cost;
                    }
                }
                return min_idx;
            }
        }

        bool candidate_available() const {
            for (int i = 0; i < count; i++) {
                if (!tested[i]) return true;
            }
            return false;
        }

        void split_candidate(int i, const SplitCandidate& left, const SplitCandidate& right) {
            assert(count < N);
            candidates[i] = left;
            tested[i] = false;
            candidates[count] = right;
            tested[count++] = false;
        }

        void mark_candidate(int i) {
            tested[i] = true;
            nodes++;
        }
    };

    void sort_refs(int axis, Ref* refs, int ref_count) {
        // Sort the primitives based on their centroids
        std::sort(refs, refs + ref_count, [axis] (const Ref& a, const Ref& b) {
            const float ca = a.bb.min[axis] + a.bb.max[axis];
            const float cb = b.bb.min[axis] + b.bb.max[axis];
            return (ca < cb) || (ca == cb && a.id < b.id);
        });
    }

    void find_object_split(ObjectSplit& split, int axis, Ref* refs, int ref_count) {
        assert(ref_count > 0);

        sort_refs(axis, refs, ref_count);

        // Sweep from the right and accumulate the bounding boxes
        BBox cur_bb = BBox::empty();
        for (int i = ref_count - 1; i > 0; i--) {
            cur_bb.extend(refs[i].bb);
            right_bbs_[i - 1] = cur_bb;
        }

        // Sweep from the left and compute the SAH cost
        cur_bb = BBox::empty();
        for (int i = 0; i < ref_count - 1; i++) {
            cur_bb.extend(refs[i].bb);
            const float cost = CostFn::split_cost(i + 1, cur_bb.half_area(), ref_count - i - 1, right_bbs_[i].half_area());
            if (cost < split.cost) {
                split.axis = axis;
                split.cost = cost;
                split.left_count = i + 1;
                split.left_bb = cur_bb;
                split.right_bb = right_bbs_[i];
            }
        }

        assert(split.left_count != 0 && split.left_count != ref_count);
    }

    void apply_object_split(const ObjectSplit& split, Ref* refs, int ref_count) {
        sort_refs(split.axis, refs, ref_count);
    }

    void find_spatial_split(SpatialSplit& split, const BBox& parent_bb,
                            const Mesh& mesh, int axis,
                            Ref* refs, int ref_count) {
        const float min = parent_bb.min[axis];
        const float max = parent_bb.max[axis];
        assert(max > min);
        Bin bins[spatial_bins];

        // Initialize bins
        for (int i = 0; i < spatial_bins; i++) {
            bins[i].entry = 0;
            bins[i].exit = 0;
            bins[i].bb = BBox::empty();
        }

        // Put the primitives in the bins
        const float bin_size = (max - min) / spatial_bins;
        const float inv_size = 1.0f / bin_size;
        for (int i = 0; i < ref_count; i++) {
            const Ref& ref = refs[i];

            assert(ref.bb.is_included(parent_bb));

            const int first_bin = clamp((int)(inv_size * (ref.bb.min[axis] - min)), 0, spatial_bins - 1);
            const int last_bin  = clamp((int)(inv_size * (ref.bb.max[axis] - min)), 0, spatial_bins - 1);

            BBox cur_bb = ref.bb;
            for (int j = first_bin; j < last_bin; j++) {
                BBox left_bb, right_bb;
                mesh.triangle(ref.id).compute_split(left_bb, right_bb, axis, min + (j + 1) * bin_size);
                bins[j].bb.extend(left_bb.overlap(cur_bb));
                cur_bb.overlap(right_bb);
            }

            bins[last_bin].bb.extend(cur_bb);
            bins[first_bin].entry++;
            bins[last_bin].exit++;
        }

        // Sweep from the right and accumulate the bounding boxes
        BBox cur_bb = BBox::empty();
        for (int i = spatial_bins - 1; i > 0; i--) {
            cur_bb.extend(bins[i].bb);
            right_bbs_[i - 1] = cur_bb;
        }

        // Sweep from the left and compute the SAH cost
        int left_count = 0, right_count = ref_count;
        cur_bb = BBox::empty();

        for (int i = 0; i < spatial_bins - 1; i++) {
            left_count += bins[i].entry;
            right_count -= bins[i].exit;
            cur_bb.extend(bins[i].bb);

            const float cost = CostFn::split_cost(left_count, cur_bb.half_area(), right_count, right_bbs_[i].half_area());
            if (cost < split.cost) {
                split.axis = axis;
                split.cost = cost;
                split.position = min + (i + 1) * bin_size;
            }
        }
    }

    void apply_spatial_split(const SpatialSplit& split, const Mesh& mesh,
                             Ref* refs, int ref_count,
                             Ref*& left_refs, int& left_count, BBox& left_bb,
                             Ref*& right_refs, int& right_count, BBox& right_bb) {
        // Split the reference array in three parts:
        // [0.. left_count[ : references that are completely on the left
        // [left_count.. first_right[ : references that lie in between
        // [first_right.. ref_count[ : references that are completely on the right
        int first_right = ref_count;
        int cur_ref = 0;

        left_count = 0;
        left_bb = BBox::empty();
        right_bb = BBox::empty();

        while (cur_ref < first_right) {
            if (refs[cur_ref].bb.max[split.axis] <= split.position) {
                left_bb.extend(refs[cur_ref].bb);
                std::swap(refs[cur_ref++], refs[left_count++]);
            } else if (refs[cur_ref].bb.min[split.axis] >= split.position) {
                right_bb.extend(refs[cur_ref].bb);
                std::swap(refs[cur_ref], refs[--first_right]);
            } else {
                cur_ref++;
            }
        }

        right_count = ref_count - first_right;

        // Handle straddling references
        std::vector<Ref> dup_refs;
        while (left_count < first_right) {
            const Ref& ref = refs[left_count];
            BBox left_split_bb, right_split_bb;
            mesh.triangle(ref.id).compute_split(left_split_bb, right_split_bb, split.axis, split.position);
            left_split_bb.overlap(ref.bb);
            right_split_bb.overlap(ref.bb);

            const BBox left_unsplit_bb  = BBox(ref.bb).extend(left_bb);
            const BBox right_unsplit_bb = BBox(ref.bb).extend(right_bb);
            const BBox left_dup_bb  = BBox(left_split_bb).extend(left_bb);
            const BBox right_dup_bb = BBox(right_split_bb).extend(right_bb);

            const float left_unsplit_area  = left_unsplit_bb.half_area();
            const float right_unsplit_area = right_unsplit_bb.half_area();
            const float left_dup_area  = left_dup_bb.half_area();
            const float right_dup_area = right_dup_bb.half_area();

            // Compute the cost of unsplitting to the left and the right
            const float unsplit_left_cost  = CostFn::split_cost(left_count + 1, left_unsplit_area,   right_count,     right_bb.half_area());
            const float unsplit_right_cost = CostFn::split_cost(left_count,     left_bb.half_area(), right_count + 1, right_unsplit_area);
            const float dup_cost           = CostFn::split_cost(left_count + 1, left_dup_area,       right_count + 1, right_dup_area);

            const float min_cost = std::min(dup_cost, std::min(unsplit_left_cost, unsplit_right_cost));

            if (min_cost == unsplit_left_cost) {
                // Unsplit to the left
                left_bb = left_unsplit_bb;
                left_count++;
            } else if (min_cost == unsplit_right_cost) {
                // Unsplit to the right
                right_bb = right_unsplit_bb;
                std::swap(refs[--first_right], refs[left_count]);
                right_count++;
            } else {
                // Duplicate
                left_bb = left_dup_bb;
                right_bb = right_dup_bb;
                refs[left_count].bb = left_split_bb;
                dup_refs.emplace_back(refs[left_count].id, right_split_bb);
                left_count++;
                right_count++;
            }
        }

        if (dup_refs.size() == 0) {
            // We can reuse the original arrays
            left_refs = refs;
            right_refs = refs + left_count;
        } else {
            // We need to reallocate a new array for the right child
            left_refs = refs;
            right_refs = mem_pool_.alloc<Ref>(right_count);
            std::copy(refs + first_right, refs + ref_count, right_refs + dup_refs.size());
            std::copy(dup_refs.begin(), dup_refs.end(), right_refs);
        }

        assert(left_count != 0 && right_count != 0);
        assert(!left_bb.is_empty() && !right_bb.is_empty());
    }


#ifdef STATISTICS
    long total_time_ = 0;
    int total_nodes_ = 0;
    int total_leaves_ = 0;
    int total_refs_ = 0;
    int total_tris_ = 0;
    int spatial_splits_ = 0;
    int object_splits_ = 0;
#endif

    BBox* right_bbs_;
    MemoryPool<> mem_pool_;
};

} // namespace imba

#endif // IMBA_SBVH_BUILDER_H

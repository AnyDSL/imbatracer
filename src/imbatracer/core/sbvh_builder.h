#ifndef IMBA_BVH_BUILDER_H
#define IMBA_BVH_BUILDER_H

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
class SplitBvhBuilder {
public:
    struct Ref {
        uint32_t id;
        BBox bb;

        Ref() {}
        Ref(uint32_t id, const BBox& bb) : id(id), bb(bb) {}
    };

    typedef std::function<void (const BBox&, const BBox&, const BBox&)> NodeWriter;
    typedef std::function<void (const BBox&, const Ref*, int)> LeafWriter;

    SplitBvhBuilder(NodeWriter write_node, LeafWriter write_leaf)
        : write_node_(write_node), write_leaf_(write_leaf)
    {}

    void build(const Mesh& mesh, int leaf_threshold = 2, float alpha = 1e-5f) {
        assert(leaf_threshold >= 1);

#ifdef STATISTICS
        total_tris_ += mesh.triangle_count();
        auto time_start = std::chrono::high_resolution_clock::now();
#endif

        const size_t tri_count = mesh.triangle_count();

        mem_pool_.cleanup();

        Ref* initial_refs = mem_pool_.alloc<Ref>(tri_count);
        right_bbs_ = mem_pool_.alloc<BBox>(tri_count);
        BBox mesh_bb = BBox::empty();
        for (size_t i = 0; i < tri_count; i++) {
            const Tri& tri = mesh.triangle(i);
            tri.compute_bbox(initial_refs[i].bb);
            mesh_bb.extend(initial_refs[i].bb);
            initial_refs[i].id = i;
        }

        if (tri_count <= leaf_threshold) {
            make_empty(mesh_bb, initial_refs, tri_count);
            return;
        }

        const float spatial_threshold = mesh_bb.half_area() * alpha;

        Stack<StackElem> stack;
        stack.push(initial_refs, tri_count, mesh_bb);

        while (!stack.empty()) {
            const StackElem& elem = stack.pop();
            Ref* refs = elem.refs;
            const int ref_count = elem.ref_count;
            const BBox& parent_bb = elem.bbox;

            assert(ref_count != 0);

            if (ref_count <= leaf_threshold) {
                // When there are not enough triangles, make a leaf
                make_leaf(parent_bb, refs, ref_count);
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

            if (split_cost >= (ref_count - 1) * parent_bb.half_area()) {
                // The node cannot be split
                make_leaf(parent_bb, refs, ref_count);
            } else {
                if (spatial) {
                    Ref* left_refs, *right_refs;
                    BBox left_bb, right_bb;
                    int left_count, right_count;
                    apply_spatial_split(spatial_split, mesh,
                                        refs, ref_count,
                                        left_refs, left_count, left_bb,
                                        right_refs, right_count, right_bb);

                    make_node(parent_bb, left_bb, right_bb);

                    stack.push(right_refs, right_count, right_bb);
                    stack.push(left_refs, left_count, left_bb);

#ifdef STATISTICS
                    spatial_splits_++;
#endif
                } else {
                    // Partitioning can be done in-place
                    apply_object_split(object_split, refs, ref_count);

                    make_node(parent_bb, object_split.left_bb, object_split.right_bb);

                    const int right_count = ref_count - object_split.left_count;
                    const int left_count = object_split.left_count;

                    Ref *right_refs = refs + object_split.left_count;
                    Ref* left_refs = refs;

                    stack.push(right_refs, right_count, object_split.right_bb);
                    stack.push(left_refs,  left_count,  object_split.left_bb);

#ifdef STATISTICS
                    object_splits_++;
#endif
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
            const float cost = (i + 1) * cur_bb.half_area() +
                               (ref_count - i - 1) * right_bbs_[i].half_area();
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

    void find_spatial_split(SpatialSplit& split, const BBox& parent_bb, const Mesh& mesh, int axis, Ref* refs, int ref_count) {
        const float min = parent_bb.min[axis];
        const float max = parent_bb.max[axis];
        constexpr int bin_count = 256;
        Bin bins[bin_count];

        // Initialize bins
        for (int i = 0; i < bin_count; i++) {
            bins[i].entry = 0;
            bins[i].exit = 0;
            bins[i].bb = BBox::empty();
        }

        // Put the primitives in the bins
        const float bin_size = (max - min) / bin_count;
        const float inv_size = 1.0f / bin_size;
        for (int i = 0; i < ref_count; i++) {
            const Ref& ref = refs[i];

            assert(ref.bb.is_included(parent_bb));

            const int first_bin = clamp((int)(inv_size * (ref.bb.min[axis] - min)), 0, bin_count - 1);
            const int last_bin  = clamp((int)(inv_size * (ref.bb.max[axis] - min)), 0, bin_count - 1);

            BBox cur_bb = ref.bb;
            for (int j = first_bin; j < last_bin; j++) {
                BBox left_bb, right_bb;
                mesh.triangle(ref.id).compute_split(left_bb, right_bb, axis, min + (j + 1) * bin_size);
                left_bb.overlap(cur_bb);
                right_bb.overlap(cur_bb);
                bins[j].bb.extend(left_bb);
                cur_bb = right_bb;
            }

            bins[last_bin].bb.extend(cur_bb);
            bins[first_bin].entry++;
            bins[last_bin].exit++;
        }

        // Sweep from the right and accumulate the bounding boxes
        BBox cur_bb = BBox::empty();
        for (int i = bin_count - 1; i > 0; i--) {
            cur_bb.extend(bins[i].bb);
            right_bbs_[i - 1] = cur_bb;
        }

        // Sweep from the left and compute the SAH cost
        int left_count = 0, right_count = ref_count;
        cur_bb = BBox::empty();

        for (int i = 0; i < bin_count - 1; i++) {
            left_count += bins[i].entry;
            right_count -= bins[i].exit;
            cur_bb.extend(bins[i].bb);

            const float cost = left_count * cur_bb.half_area() +
                               right_count * right_bbs_[i].half_area();
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
            const float unsplit_left_cost  = left_unsplit_area * (left_count + 1) + right_bb.half_area() * right_count;
            const float unsplit_right_cost = left_bb.half_area() * left_count + right_unsplit_area * (right_count + 1);
            const float dup_cost = left_dup_area * (left_count + 1) + right_dup_area * (right_count + 1);

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
                dup_refs.push_back(Ref(refs[left_count].id, right_split_bb));
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

    void make_empty(const BBox& parent_bb, const Ref* refs, int ref_count) {
        write_node_(parent_bb, parent_bb, BBox::empty());
        write_leaf_(parent_bb, refs, ref_count);
        write_leaf_(BBox::empty(), refs, 1);

#ifdef STATISTICS
        total_nodes_++;
        total_leaves_ += 2;
        total_refs_ += ref_count;
#endif
    }

    void make_node(const BBox& parent_bb, const BBox& left_bb, const BBox& right_bb) {
        write_node_(parent_bb, left_bb, right_bb);

#ifdef STATISTICS
        total_nodes_++;
#endif
    }

    void make_leaf(const BBox& parent_bb, const Ref* refs, int ref_count) {
        write_leaf_(parent_bb, refs, ref_count);

#ifdef STATISTICS
        total_leaves_++;
        total_refs_ += ref_count;
#endif
    }

    struct StackElem {
        Ref* refs;
        int ref_count;
        BBox bbox;

        StackElem() {}
        StackElem(Ref* refs, int ref_count, const BBox& bbox)
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

    BBox* right_bbs_;
    MemoryPool<> mem_pool_;
    NodeWriter write_node_;
    LeafWriter write_leaf_;
};

} // namespace imba

#endif // IMBA_BVH_BUILDER_H

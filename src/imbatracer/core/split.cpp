#include <algorithm>
#include <cassert>
#include <cmath>
#include "split.h"
#include "mesh.h"
#include "common.h"

namespace imba {

struct Bin {
    int entry_count;
    int exit_count;
    float lower, upper;
    float accum_cost;
    BBox bbox;
};

inline void initialize_bins(Bin* bins, int bin_count, float min, float max) {
    const float step = (max - min) / bin_count;

    for (int i = 0; i < bin_count; i++) {
        bins[i].bbox = BBox::empty();
        bins[i].entry_count = 0;
        bins[i].exit_count = 0;
        bins[i].lower = fmaf(i, step, min);
        bins[i].upper = bins[i].lower + step;
    }

    bins[bin_count - 1].upper = max;
}

static void find_best_split(SplitCandidate& split, int axis, bool spatial, Bin* bins, int bin_count) {
    // Sweep from the left: accumulate SAH cost
    BBox cur_bb = bins[0].bbox;
    int cur_count = bins[0].entry_count;
    bins[0].accum_cost = 0;
    for (int i = 1; i < bin_count; i++) {
        bins[i].accum_cost = cur_bb.half_area() * cur_count;
        cur_bb.extend(bins[i].bbox);
        cur_count += bins[i].entry_count;
    }

    // Sweep from the right: find best partition
    int best_split = bin_count - 1;
    cur_bb = bins[bin_count - 1].bbox;
    cur_count = bins[bin_count - 1].exit_count;

    float best_cost = cur_bb.half_area() * cur_count + bins[bin_count - 1].accum_cost;
    BBox right_bb = cur_bb;
    int right_count = cur_count;

    for (int i = bin_count - 2; i > 0; i--) {
        cur_bb.extend(bins[i].bbox);
        cur_count += bins[i].exit_count;
        float cost = cur_bb.half_area() * cur_count + bins[i].accum_cost;

        if (cost < best_cost) {
            right_bb = cur_bb;
            right_count = cur_count;
            best_cost = cost;
            best_split = i;
        }
    }

    // Save that split if it is better than the current one
    if (best_cost < split.cost) {
        split.spatial = spatial;
        split.axis = axis;
        split.cost = best_cost;
        split.right_bb = right_bb;
        split.right_count = right_count;
        split.position = bins[best_split].lower;

        // Find the bounding box and primitive count of the left child
        split.left_bb = bins[0].bbox;
        split.left_count = bins[0].entry_count;
        for (int i = 1; i < best_split; i++) {
            split.left_bb.extend(bins[i].bbox);
            split.left_count += bins[i].entry_count;
        }
    }
}

inline void lower_bound(int& bin_id, float value, const Bin* bins, int bin_count) {
    while (bin_id < bin_count - 1 && value >= bins[bin_id + 1].lower) bin_id++;
}

inline void upper_bound(int& bin_id, float value, const Bin* bins, int bin_count) {
    while (bin_id > 0 && value < bins[bin_id].lower) bin_id--;
}

void object_split(SplitCandidate& split,
                  const BBox& parent_bb, const BBox& center_bb, int axis,
                  const uint32_t* refs, int ref_count,
                  const float3* centroids, const BBox* bboxes) {
    const float min = center_bb.min[axis];
    const float max = center_bb.max[axis];
    assert(max > min);

    constexpr int bin_count = 32;
    Bin bins[bin_count];
    initialize_bins(bins, bin_count, min, max);

    // Put the primitives in each bin
    const float factor = bin_count / (max - min);
    for (int i = 0; i < ref_count; i++) {
        const float3& center = centroids[refs[i]];
        int bin_id = std::min((int)((center[axis] - min) * factor), bin_count - 1);
        assert(bin_id >= 0);

        // Because of numerical imprecision, it is possible (though unlikely)
        // that bin_id points to the wrong bin
        lower_bound(bin_id, center[axis], bins, bin_count);
        upper_bound(bin_id, center[axis], bins, bin_count);

        assert(bboxes[refs[i]].is_overlapping(parent_bb));

        bins[bin_id].bbox.extend(bboxes[refs[i]]);
        bins[bin_id].entry_count++;
    }

    for (int i = 0; i < bin_count; i++) {
        bins[i].bbox.overlap(parent_bb);
        bins[i].exit_count = bins[i].entry_count;
    }

    find_best_split(split, axis, false, bins, bin_count);
}

void spatial_split(SplitCandidate& split,
                   const BBox& parent_bb, int32_t axis,
                   const uint32_t* refs, int ref_count,
                   const Mesh& mesh, const BBox* bboxes) {
    const float min = parent_bb.min[axis];
    const float max = parent_bb.max[axis];
    assert(max > min);

    constexpr int bin_count = 256;
    Bin bins[bin_count];
    initialize_bins(bins, bin_count, min, max);

    // Put the primitives in each bin
    const float factor = bin_count / (max - min);
    const float bbox_offset = (max - min) / (bin_count * 10);
    for (int i = 0; i < ref_count; i++) {
        assert(bboxes[refs[i]].is_overlapping(parent_bb));

        const float bb_min = bboxes[refs[i]].min[axis];
        const float bb_max = bboxes[refs[i]].max[axis];
        int first_bin = clamp((int)((bb_min - bbox_offset - min) * factor), 0, bin_count - 1);
        int last_bin  = clamp((int)((bb_max + bbox_offset - min) * factor), 0, bin_count - 1);

        // Since we are using a slighly larger bounding box for the triangle,
        // we need to adapt the bin ids
        lower_bound(first_bin, bb_min, bins, bin_count);
        upper_bound(last_bin,  bb_max, bins, bin_count);

        for (int j = first_bin; j <= last_bin; j++) {
            BBox clipped_bbox;
            mesh.triangle(refs[i]).compute_clipped_bbox(clipped_bbox, axis, bins[j].lower, bins[j].upper);
            bins[j].bbox.extend(clipped_bbox);
        }

        bins[first_bin].entry_count++;
        bins[last_bin].exit_count++;
    }

    for (int i = 0; i < bin_count; i++)
        bins[i].bbox.overlap(parent_bb);

    find_best_split(split, axis, true, bins, bin_count);
}

void object_partition(const SplitCandidate& split,
                      uint32_t* refs, int ref_count,
                      const float3* centroids) {
    assert(!split.spatial);
    auto left_count = std::partition(refs, refs + ref_count, [=] (uint32_t ref) {
       return centroids[ref][split.axis] < split.position;
    }) - refs;
    assert(split.left_count  == left_count &&
           split.right_count == ref_count - left_count);
}

void spatial_partition(const SplitCandidate& split,
                       const uint32_t* refs, int ref_count,
                       uint32_t* left_refs, uint32_t* right_refs, 
                       const BBox* bboxes) {
    assert(split.spatial);
    int left_count = 0, right_count = 0;
    for (int i = 0; i < ref_count; i++) {
        const uint32_t ref = refs[i];
        const float bb_min = bboxes[ref].min[split.axis];
        const float bb_max = bboxes[ref].max[split.axis];

        if (bb_max >= split.position) {
            assert(right_count < split.right_count);
            assert(bboxes[ref].is_overlapping(split.right_bb));
            right_refs[right_count++] = ref;
        }

        if (bb_min < split.position) {
            assert(left_count < split.left_count);
            assert(bboxes[ref].is_overlapping(split.left_bb));
            left_refs[left_count++] = ref;
        }
    }

    assert(split.left_count  == left_count &&
           split.right_count == right_count);
}

} // namespace imba

#ifndef IMBA_SPLIT_H
#define IMBA_SPLIT_H

#include <cstdint>
#include "float3.h"
#include "bbox.h"
#include "tri.h"

namespace imba {

class Mesh;

struct SplitCandidate {
    bool spatial;                 ///< Set to true if the split is spatial
    int axis;                     ///< Split axis (0: x, 1: y, 2: z)
    float position;               ///< Split position along the axis
    float cost;                   ///< SAH cost
    BBox left_bb, right_bb;       ///< Children bounding boxes
    int left_count, right_count;  ///< Children primitive count

    SplitCandidate() : cost(FLT_MAX), left_count(0), right_count(0) {}

    SplitCandidate(float position, float cost, BBox left_bb, BBox right_bb)
        : position(position), cost(cost), left_bb(left_bb), right_bb(right_bb)
    {}

    bool empty() const { return left_count == 0 || right_count == 0; }

    bool operator < (const SplitCandidate& other) const { return !empty() && cost < other.cost; }
};

/// Finds the best object split along an axis (min and max are the _centroids_ bounds)
SplitCandidate object_split(int axis, float min, float max,
                            const uint32_t* refs, int ref_count,
                            const float3* centroids, const BBox* bboxes);

/// Finds the best spatial split along an axis (min and max are the _triangles_ bounds)
SplitCandidate spatial_split(int axis, float min, float max,
                             const uint32_t* refs, int ref_count,
                             const Mesh& mesh, const BBox* bboxes);

/// Partitions the sets of objects based on the given split candidate
void object_partition(const SplitCandidate& candidate,
                      uint32_t* refs, int ref_count,
                      const float3* centroids);

/// Partitions the sets of objects based on the given split candidate
void spatial_partition(const SplitCandidate& candidate,
                       const uint32_t* refs, int ref_count,
                       uint32_t* left_refs, uint32_t* right_refs,
                       const BBox* bboxes);

}

#endif // IMBA_SPLIT_H

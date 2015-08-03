#ifndef IMBA_SBVH_H
#define IMBA_SBVH_H

#include <cstdint>
#include <functional>

#include "float3.h"
#include "bbox.h"
#include "tri.h"

namespace imba {

typedef std::function<void (const BBox&, const BBox&, const BBox&)> NodeWriter;
typedef std::function<void (const BBox&, const uint32_t*, int)> LeafWriter;

/// Builds a SBVH (Spatial split BVH), given the set of triangles and the alpha parameter
/// that controls when to do a spatial split. The tree is built in depth-first order.
/// See  Stich et al., "Spatial Splits in Bounding Volume Hierarchies", 2009
/// http://www.nvidia.com/docs/IO/77714/sbvh.pdf
void build_sbvh(const Tri* tris, int tri_count, NodeWriter write_node, LeafWriter write_leaf, float alpha = 1e-5f);

} // namespace imba

#endif // IMBA_SBVH_H

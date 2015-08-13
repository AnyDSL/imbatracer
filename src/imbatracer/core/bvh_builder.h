#ifndef IMBA_BVH_BUILDER_H
#define IMBA_BVH_BUILDER_H

#include <cstdint>
#include <functional>
#include "float3.h"
#include "bbox.h"
#include "tri.h"

namespace imba {

class Mesh;

/// Builds a SBVH (Spatial split BVH), given the set of triangles and the alpha parameter
/// that controls when to do a spatial split. The tree is built in depth-first order.
/// See  Stich et al., "Spatial Splits in Bounding Volume Hierarchies", 2009
/// http://www.nvidia.com/docs/IO/77714/sbvh.pdf
class BvhBuilder {
public:
    typedef std::function<void (const BBox&, const BBox&, const BBox&)> NodeWriter;
    typedef std::function<void (const BBox&, const uint32_t*, int)>     LeafWriter;

    BvhBuilder(NodeWriter node, LeafWriter leaf)
        : write_node_(node), write_leaf_(leaf)
    {}

    void build(const Mesh& mesh, int leaf_threshold = 2, float alpha = 1e-5f);

private:
    struct StackElem {
        uint32_t* refs;
        int ref_count;
        BBox bbox;

        StackElem() {}
        StackElem(uint32_t* refs, int ref_count, const BBox& bbox)
            : refs(refs), ref_count(ref_count), bbox(bbox)
        {}
    };

    NodeWriter write_node_;
    LeafWriter write_leaf_;
};

} // namespace imba

#endif // IMBA_BVH_BUILDER_H

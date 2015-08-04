#ifndef ADAPTER_H
#define ADAPTER_H

#include <memory>
#include <traversal.h>
#include "allocator.h"

namespace imba {

class Mesh;

class Adapter {
public:
    Adapter(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris)
        : nodes_(nodes), tris_(tris)
    {}

    virtual ~Adapter() {}

    /// Writes the acceleration structure for the given mesh
    /// sequentially in the array of nodes.
    virtual void build_accel(const Mesh& mesh) = 0;

protected:
    ThorinVector<Node>& nodes_;
    ThorinVector<Vec4>& tris_;
};

/// Returns the right acceleration structure adapter for the traversal implementation.
std::unique_ptr<Adapter> new_adapter(ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris);

} // namespace imba

#endif

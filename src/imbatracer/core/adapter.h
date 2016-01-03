#ifndef ADAPTER_H
#define ADAPTER_H

#include <vector>
#include <memory>
#include <traversal.h>

namespace imba {

class Mesh;

class Adapter {
public:
    Adapter(std::vector<Node>& nodes, std::vector<Vec4>& tris)
        : nodes_(nodes), tris_(tris)
    {}

    virtual ~Adapter() {}

    /// Writes the acceleration structure for the given mesh
    /// sequentially in the array of nodes.
    virtual void build_accel(const Mesh& mesh) = 0;

protected:
    std::vector<Node>& nodes_;
    std::vector<Vec4>& tris_;
};

/// Returns the right acceleration structure adapter for the traversal implementation.
std::unique_ptr<Adapter> new_adapter(std::vector<Node>& nodes, std::vector<Vec4>& tris);

} // namespace imba

#endif

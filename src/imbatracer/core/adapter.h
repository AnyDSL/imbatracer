#ifndef IMBA_ADAPTER_H
#define IMBA_ADAPTER_H

#include <vector>
#include <memory>

#include <traversal.h>

#include "mesh.h"

namespace imba {

class MeshAdapter {
public:
    MeshAdapter(std::vector<Node>& nodes, std::vector<Vec4>& tris)
        : nodes_(nodes), tris_(tris)
    {}

    virtual ~MeshAdapter() {}

    /// Writes the acceleration structure for the given mesh
    /// sequentially in the array of nodes.
    virtual void build_accel(const Mesh& mesh) = 0;

#ifdef STATISTICS
    virtual void print_stats() const {};
#endif

protected:
    std::vector<Node>& nodes_;
    std::vector<Vec4>& tris_;
};

class TopLevelAdapter {
public:
    TopLevelAdapter(std::vector<Node>& nodes)
        : nodes_(nodes)
    {}

    virtual ~TopLevelAdapter() {}

    /// Writes the acceleration structure for the given mesh
    /// sequentially in the array of nodes.
    virtual void build_accel(const std::vector<Mesh>& meshes,
                             const std::vector<Mesh::Instance>& instances,
                             const std::vector<int>& layout) = 0;

#ifdef STATISTICS
    virtual void print_stats() const {};
#endif

protected:
    std::vector<Node>& nodes_;
};

/// Returns the correct mesh acceleration structure adapter for the traversal implementation.
std::unique_ptr<MeshAdapter> new_mesh_adapter(std::vector<Node>& nodes, std::vector<Vec4>& tris);
/// Returns the correct top-level acceleration structure adapter for the traversal implementation.
std::unique_ptr<TopLevelAdapter> new_top_level_adapter(std::vector<Node>& nodes);

} // namespace imba

#endif // IMBA_ADAPTER_H

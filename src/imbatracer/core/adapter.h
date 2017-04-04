#ifndef IMBA_ADAPTER_H
#define IMBA_ADAPTER_H

#include <vector>
#include <memory>

#include "traversal_interface.h"
#include "mesh.h"

namespace imba {

class MeshAdapter {
public:
    virtual ~MeshAdapter() {}

    /// Writes the acceleration structure for the given mesh
    /// sequentially in the array of nodes.
    virtual void build_accel(const Mesh& mesh, int mesh_id, const std::vector<int>& tri_layout) = 0;

#ifdef STATISTICS
    virtual void print_stats() const {};
#endif
};

class TopLevelAdapter {
public:
    virtual ~TopLevelAdapter() {}

    /// Writes the acceleration structure for the given mesh
    /// sequentially in the array of nodes.
    virtual void build_accel(const std::vector<Mesh>& meshes,
                             const std::vector<Mesh::Instance>& instances,
                             const std::vector<int>& layout,
                             int root_offset) = 0;

#ifdef STATISTICS
    virtual void print_stats() const {};
#endif
};

/// Returns the correct mesh acceleration structure adapter for the traversal implementation.
std::unique_ptr<MeshAdapter> new_mesh_adapter_cpu(std::vector<traversal_cpu::Node>& nodes, std::vector<Vec4>& tris);
std::unique_ptr<MeshAdapter> new_mesh_adapter_gpu(std::vector<traversal_gpu::Node>& nodes, std::vector<Vec4>& tris);
/// Returns the correct top-level acceleration structure adapter for the traversal implementation.
std::unique_ptr<TopLevelAdapter> new_top_level_adapter_cpu(std::vector<traversal_cpu::Node>& nodes, std::vector<InstanceNode>& instance_nodes);
std::unique_ptr<TopLevelAdapter> new_top_level_adapter_gpu(std::vector<traversal_gpu::Node>& nodes, std::vector<InstanceNode>& instance_nodes);

} // namespace imba

#endif // IMBA_ADAPTER_H

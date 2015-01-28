#ifndef IMBA_INSTANCE_HPP
#define IMBA_INSTANCE_HPP

#include "object.hpp"
#include "../common/matrix.hpp"

namespace imba {

/// Instance object, represents a copy of a mesh along with a transformation.
class Instance {
public:
    Instance(TriangleMeshId mesh_id, const Mat4& mat = Mat4::identity()) {
        Mat4 inv = inverse(mat);
        std::copy(mat.m, mat.m + 16, inst_.mat.c0.values);
        std::copy(inv.m, inv.m + 16, inst_.inv_mat.c0.values);
        inst_.mesh_id = mesh_id.id;
    }

    const Mat4& matrix() const { return *reinterpret_cast<const Mat4*>(inst_.mat.c0.values); }

    void set_matrix(const Mat4& mat) {
        Mat4 inv = inverse(mat);
        std::copy(mat.m, mat.m + 16, inst_.mat.c0.values);
        std::copy(inv.m, inv.m + 16, inst_.inv_mat.c0.values);
    }

    TriangleMeshId mesh_id() const { return TriangleMeshId(inst_.mesh_id); }

    void set_mesh_id(TriangleMeshId id) {
        inst_.mesh_id = id.id;
    }

private:
    ::MeshInstance inst_;
};

} // namespace imba

#endif // IMBA_INSTANCE_HPP


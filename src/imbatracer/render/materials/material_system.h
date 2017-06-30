#ifndef IMBA_MATERIAL_SYSTEM
#define IMBA_MATERIAL_SYSTEM

#include "imbatracer/render/materials/bsdf.h"

#include <memory>
#include <string>

namespace imba {

/// Stores the local material information at a hit point (BSDF and emitted radiance)
struct MaterialValue {
    rgb  emit;
    BSDF bsdf;
};

/// Sets up a material system that uses Open Shading Language to create BSDF objects.
class MaterialSystem {
public:
    MaterialSystem(const std::string& search_path);
    ~MaterialSystem();

    /// Evaluates the material at the given intersections and returns the emission and BSDF.
    void eval_material(const Intersection& isect, bool adjoint, MaterialValue& res) const;

    /// Creates a new shader
    /// \param search_path      the path where the required .oso files are located
    /// \param name             the name of the shader group to be created
    /// \param serialized_graph a shader graph description following the suggested format from the OSL specs, Chapter 9
    void add_shader(const std::string& search_path, const std::string& name, const std::string& serialized_graph);

    int shader_count() const;

private:
    struct MatSysInternal;
    std::unique_ptr<MatSysInternal> internal_;
};

} // namespace imba

#endif // IMBA_MATERIAL_SYSTEM
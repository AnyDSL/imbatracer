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

    MaterialValue eval_material(const float3& pos, const float2& uv, const float3& dir, const float3& normal,
                                const float3& geom_normal, float area, int shader, bool adjoint);

    // TODO specify shader parameters, connections, etc.
    void add_shader(const std::string& name, const std::string& search_path);

    int shader_count() const;

private:
    struct MatSysInternal;
    std::unique_ptr<MatSysInternal> internal_;
};

} // namespace imba

#endif // IMBA_MATERIAL_SYSTEM
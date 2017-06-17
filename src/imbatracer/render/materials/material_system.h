#ifndef IMBA_MATERIAL_SYSTEM
#define IMBA_MATERIAL_SYSTEM

#include "imbatracer/render/scene.h"

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
    MaterialSystem(Scene* scene, const std::string& search_path);
    ~MaterialSystem();

    void eval_material(const Hit& hit, const Ray& ray, MaterialValue& res);

    // TODO parse shader parameters, connections, etc (what file format?)
    void add_shader();

private:
    struct MatSysInternal;
    std::unique_ptr<MatSysInternal> internal_;
};

} // namespace imba

#endif // IMBA_MATERIAL_SYSTEM
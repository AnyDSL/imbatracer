#include "imbatracer/render/materials/material_system.h"
#include "imbatracer/render/materials/brdfs.h"
#include "imbatracer/render/materials/btdfs.h"

#define TBB_USE_EXCEPTIONS 0
#include <tbb/enumerable_thread_specific.h>

#include <OSL/oslexec.h>
#include <OSL/genclosure.h>
#include <OSL/dual.h>
#include <OSL/oslclosure.h>
#include <OSL/oslconfig.h>

constexpr int OSL_DEBUG_LVL = 1;
constexpr int OSL_OPTIMIZE_LVL = 2;

using namespace OSL;

namespace imba {

float3 make_float3(const Color3& cl) {
    return float3(cl.x, cl.y, cl.z);
}

struct ThreadLocalContext {
    ShadingContext* ctx;
    PerThreadInfo* tinfo;

    ThreadLocalContext() : ctx(nullptr), tinfo(nullptr) {}
};

using ThreadLocalMemArena = tbb::enumerable_thread_specific<ThreadLocalContext, tbb::cache_aligned_allocator<ThreadLocalContext>, tbb::ets_key_per_instance>;
static ThreadLocalMemArena thread_local_contexts;

/// Implements the RendererServices interface for OpenShadingLanguage callbacks to the renderer.
class RenServ : public RendererServices {
public:

    RenServ() {}
    ~RenServ() {}

    int supports (string_view feature) const { return false; }

    bool get_matrix(ShaderGlobals *sg, Matrix44 &result, TransformationPtr xform, float time) override {
        // assumes that xform is a simple float4x4
        result = *reinterpret_cast<const Matrix44*>(xform);
        return true;
    }

    bool get_matrix (ShaderGlobals *sg, Matrix44 &result, TransformationPtr xform) override {
        // assumes that xform is a simple float4x4
        result = *reinterpret_cast<const Matrix44*>(xform);
        return true;
    }

    bool get_matrix (ShaderGlobals *sg, Matrix44 &result, ustring from, float time) override { return false; }
    bool get_matrix(ShaderGlobals *sg, Matrix44 &result, ustring from) override { return false; }

    bool get_inverse_matrix (ShaderGlobals *sg, Matrix44 &result, ustring to, float time) override { return false; }

    bool get_attribute(ShaderGlobals *sg, bool derivatives, ustring object, TypeDesc type, ustring name, void *val ) override {
        if (!sg) return false;
        return false;
    }

    void name_transform (const char *name, const Matrix44 &xform) {}

    bool get_array_attribute (ShaderGlobals *sg, bool derivatives, ustring object, TypeDesc type, ustring name, int index, void *val ) override {
        if (!sg) return false;
        return false;
    }

    bool get_userdata (bool derivatives, ustring name, TypeDesc type, ShaderGlobals *sg, void *val) { return false; }
};

struct MaterialSystem::MatSysInternal {
    std::unique_ptr<ShadingSystem> sys_;
    RenServ ren_serv_;
    ErrorHandler err_hand_;
    std::vector<ShaderGroupRef> shaders_;

    void register_closures();
    ShaderGlobals isect_to_globals(const float3& pos, const float2& uv, const float3& dir,
                                   const float3& normal, const float3& geom_normal, float area);
    void process_closure(MaterialValue& res, const ClosureColor* closure, bool adjoint);
};

MaterialSystem::MaterialSystem(const std::string& search_path) {
    internal_.reset(new MatSysInternal);

    internal_->sys_.reset(new ShadingSystem(&internal_->ren_serv_, nullptr, &internal_->err_hand_));

    internal_->register_closures();

    auto sys = internal_->sys_.get();

    // Default all shader parameters to be locked w.r.t geometry (no override by geometry)
    sys->attribute("lockgeom", 1);

    sys->attribute("debug", OSL_DEBUG_LVL);
    sys->attribute("compile_report", OSL_DEBUG_LVL);

    sys->attribute("optimize", OSL_OPTIMIZE_LVL);

    // TODO Do we need this?
    // shadingsys->attribute ("options", extraoptions);
}

int MaterialSystem::shader_count() const {
    return internal_->shaders_.size();
}

MaterialValue MaterialSystem::eval_material(const float3& pos, const float2& uv, const float3& dir, const float3& normal,
                                            const float3& geom_normal, float area, int shader_id, bool adjoint) {
    auto& ctx = thread_local_contexts.local();
    if (!ctx.ctx) {
        ctx.tinfo = internal_->sys_->create_thread_info();
        ctx.ctx = internal_->sys_->get_context(ctx.tinfo);

        // TODO remove debugging code
        printf("created new context\n");
    }

    auto sg = internal_->isect_to_globals(pos, uv, dir, normal, geom_normal, area);

    auto shader = internal_->shaders_[shader_id];

    internal_->sys_->execute(ctx.ctx, *shader, sg);

    MaterialValue res;
    internal_->process_closure(res, sg.Ci, adjoint);
    return res;
}

void MaterialSystem::add_shader(const std::string& name, const std::string& search_path) {
    // TODO write an actual implementation
    auto sys = internal_->sys_.get();
    sys->attribute("searchpath:shader", search_path);

    auto group = sys->ShaderGroupBegin(name);
    sys->Shader("surface", name);
    // sys->ConnectShaders();
    sys->ShaderGroupEnd();

    internal_->shaders_.push_back(group);
}

MaterialSystem::~MaterialSystem() {
    for (auto& ctx : thread_local_contexts) {
        internal_->sys_->release_context(ctx.ctx);
        internal_->sys_->destroy_thread_info(ctx.tinfo);
    }

    internal_->shaders_.clear();

    internal_->sys_.reset();
}

enum ClosureIDs {
    CLOSURE_EMISSION = 1,
    CLOSURE_DIFFUSE,
    CLOSURE_MICROFACET,
    CLOSURE_PHONG,
    CLOSURE_REFLECTION,
    CLOSURE_REFRACTION,
    CLOSURE_TRANSLUCENT
};

struct EmptyParams       {};
struct DiffuseParams     { float3 normal() const { return make_float3(N); }; Vec3 N; };
struct PhongParams       { float3 normal() const { return make_float3(N); }; Vec3 N; float exponent; };
struct ReflectionParams  { float3 normal() const { return make_float3(N); }; Vec3 N; float eta; float kappa; };
struct RefractionParams  { float3 normal() const { return make_float3(N); }; Vec3 N; float eta; };

void MaterialSystem::MatSysInternal::register_closures() {
    constexpr int MAX_PARAMS = 32;

    struct BuiltinClosures {
        const char* name;
        int id;
        ClosureParam params[MAX_PARAMS];
    };

    BuiltinClosures builtins[] = {
        { "emission",    CLOSURE_EMISSION,    { CLOSURE_FINISH_PARAM(EmptyParams) } },

        { "diffuse",     CLOSURE_DIFFUSE,     { CLOSURE_VECTOR_PARAM(DiffuseParams, N),
                                                CLOSURE_FINISH_PARAM(DiffuseParams) } },

        { "phong",       CLOSURE_PHONG,       { CLOSURE_VECTOR_PARAM(PhongParams, N),
                                                CLOSURE_FLOAT_PARAM(PhongParams, exponent),
                                                CLOSURE_FINISH_PARAM(PhongParams) } },

        { "reflection",  CLOSURE_REFLECTION,  { CLOSURE_VECTOR_PARAM(ReflectionParams, N),
                                                CLOSURE_FLOAT_PARAM(ReflectionParams, eta),
                                                CLOSURE_FLOAT_PARAM(ReflectionParams, kappa),
                                                CLOSURE_FINISH_PARAM(ReflectionParams) } },

        { "refraction",  CLOSURE_REFRACTION,  { CLOSURE_VECTOR_PARAM(RefractionParams, N),
                                                CLOSURE_FLOAT_PARAM(RefractionParams, eta),
                                                CLOSURE_FINISH_PARAM(RefractionParams) } },

        { "translucent", CLOSURE_TRANSLUCENT, { CLOSURE_VECTOR_PARAM(DiffuseParams, N),
                                                CLOSURE_FINISH_PARAM(DiffuseParams) } },
    };

    for (int i = 0; i < sizeof(builtins) / sizeof(BuiltinClosures); i++) {
        sys_->register_closure(builtins[i].name, builtins[i].id, builtins[i].params, nullptr, nullptr);
    }
}

ShaderGlobals MaterialSystem::MatSysInternal::isect_to_globals(const float3& pos, const float2& uv_c, const float3& dir,
                                                               const float3& normal, const float3& geom_normal, float area) {
    ShaderGlobals res;
    memset(&res, 0, sizeof(ShaderGlobals));

    Dual2<OSL::Vec3> point  = OSL::Vec3(pos.x, pos.y, pos.z);
    Dual2<OSL::Vec2> uv     = OSL::Vec2(uv_c.x, uv_c.y);
    Dual2<OSL::Vec3> in_dir = OSL::Vec3(dir.x, dir.y, dir.z);

    res.P    = point.val();
    res.dPdx = point.dx();
    res.dPdy = point.dy();

    res.Ng = Vec3(normal.x, normal.y, normal.z);
    res.N  = Vec3(geom_normal.x, geom_normal.y, geom_normal.z);

    res.u    = uv.val().x;
    res.dudx = uv.dx().x;
    res.dudy = uv.dy().x;

    res.v    = uv.val().y;
    res.dvdx = uv.dx().y;
    res.dvdy = uv.dy().y;

    // instancing / animations may cange the area
    res.surfacearea = area;

    res.I    = in_dir.val();
    res.dIdx = in_dir.dx();
    res.dIdy = in_dir.dy();

    // Flip the normal if backfacing
    res.backfacing = res.N.dot(res.I) > 0;
    if (res.backfacing) {
        res.N  = -res.N;
        res.Ng = -res.Ng;
    }

    res.flipHandedness = false; // TODO account for this

    // TODO add ray type support
    // res.raytype;

    return res;
}

void process_closure(MaterialValue& res, const ClosureColor* closure, const Color3& w, bool adjoint) {
    if (!closure) return;

    switch (closure->id) {
    case ClosureColor::MUL: {
        Color3 cw = w * closure->as_mul()->weight;
        process_closure(res, closure->as_mul()->closure, cw, adjoint);
    } break;
    case ClosureColor::ADD: {
        process_closure(res, closure->as_add()->closureA, w, adjoint);
        process_closure(res, closure->as_add()->closureB, w, adjoint);
    } break;
    default: {
        const ClosureComponent* comp = closure->as_comp();
        Color3 cw = w * comp->w;
        if (comp->id == CLOSURE_EMISSION)
            res.emit += make_float3(cw);
        else {
            bool ok = false;
            switch (comp->id) {
            case CLOSURE_DIFFUSE: {
                auto params = *comp->as<DiffuseParams>();
                ok = res.bsdf.add<Lambertian<false>>(make_float3(cw), params.normal());
            } break;
            case CLOSURE_TRANSLUCENT: {
                auto params = *comp->as<DiffuseParams>();
                ok = res.bsdf.add<Lambertian<true>>(make_float3(cw), params.normal());
            } break;
            case CLOSURE_PHONG: {
                auto params = *comp->as<PhongParams>();
                ok = res.bsdf.add<Phong>(make_float3(cw), params.exponent, params.normal());
            } break;
            case CLOSURE_REFLECTION: {
                auto params = *comp->as<ReflectionParams>();
                ok = res.bsdf.add<SpecularReflection<FresnelConductor>>(make_float3(cw), FresnelConductor(params.eta, params.kappa), params.normal());
            } break;
            case CLOSURE_REFRACTION: {
                auto params = *comp->as<RefractionParams>();
                if (adjoint)
                    ok = res.bsdf.add<SpecularTransmission<true>>(make_float3(cw), params.eta, 1.0f, params.normal());
                else
                    ok = res.bsdf.add<SpecularTransmission<false>>(make_float3(cw), params.eta, 1.0f, params.normal());
            } break;
            }
            ASSERT(ok && "Invalid closure invoked in surface shader");
        }
        break;
    }
    }
}

void MaterialSystem::MatSysInternal::process_closure(MaterialValue& res, const ClosureColor* closure, bool adjoint) {
    res.emit = rgb(0.0f);
    imba::process_closure(res, closure, Color3(1,1,1), adjoint);
}

} // namespace imba
#include "imbatracer/render/materials/material_system.h"

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

    Scene* scene_;

    void register_closures();
    ShaderGlobals isect_to_globals(const Hit& hit, const Ray& ray, int& shader_id);
    void process_closure(MaterialValue& res, const ClosureColor* closure);
};

MaterialSystem::MaterialSystem(Scene* scene, const std::string& search_path) {
    internal_.reset(new MatSysInternal);

    internal_->scene_ = scene;

    internal_->sys_.reset(new ShadingSystem(&internal_->ren_serv_, nullptr, &internal_->err_hand_));

    internal_->register_closures();

    auto sys = internal_->sys_.get();

    // Default all shader parameters to be locked w.r.t geometry (no override by geometry)
    sys->attribute("lockgeom", 1);

    sys->attribute("debug", OSL_DEBUG_LVL);
    sys->attribute("compile_report", OSL_DEBUG_LVL);

    sys->attribute("optimize", OSL_OPTIMIZE_LVL);

    // TODO pass shader search path
    sys->attribute("searchpath:shader", search_path);

    // TODO Do we need this?
    // shadingsys->attribute ("options", extraoptions);
}

void MaterialSystem::eval_material(const Hit& hit, const Ray& ray, MaterialValue& res) {
    auto& ctx = thread_local_contexts.local();
    if (!ctx.ctx) {
        ctx.tinfo = internal_->sys_->create_thread_info();
        ctx.ctx = internal_->sys_->get_context(ctx.tinfo);

        // TODO remove debugging code
        printf("created new context\n");
    }

    int shader_id = -1;
    auto sg = internal_->isect_to_globals(hit, ray, shader_id);

    // TODO TEST CODE REMOVE
    shader_id = 0;

    auto shader = internal_->shaders_[shader_id];

    internal_->sys_->execute(ctx.ctx, *shader, sg);

    internal_->process_closure(res, sg.Ci);
}

void MaterialSystem::add_shader() {
    // TODO write an actual implementation
    auto sys = internal_->sys_.get();

    auto group = sys->ShaderGroupBegin("test_shader");
    sys->Shader("surface", "test_diff");
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
struct DiffuseParams     { Vec3 N; };
struct PhongParams       { Vec3 N; float exponent; };
struct ReflectionParams  { Vec3 N; float eta; };
struct RefractionParams  { Vec3 N; float eta; };

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

ShaderGlobals MaterialSystem::MatSysInternal::isect_to_globals(const Hit& hit, const Ray& ray, int& shader_id) {
    ShaderGlobals res;
    memset(&res, 0, sizeof(ShaderGlobals));

    // TODO: compare with values required in the integrator, make sure to not compute the same stuff twice!
    const Mesh::Instance& inst = scene_->instance(hit.inst_id);
    const Mesh& mesh = scene_->mesh(inst.id);

    const int local_tri_id = scene_->local_tri_id(hit.tri_id, inst.id);

    const int i0 = mesh.indices()[local_tri_id * 4 + 0];
    const int i1 = mesh.indices()[local_tri_id * 4 + 1];
    const int i2 = mesh.indices()[local_tri_id * 4 + 2];
    const int  m = mesh.indices()[local_tri_id * 4 + 3];

    const float3     org(ray.org.x, ray.org.y, ray.org.z);
    const float3 out_dir(ray.dir.x, ray.dir.y, ray.dir.z);
    const auto       pos = org + hit.tmax * out_dir;
    const auto local_pos = inst.inv_mat * float4(pos, 1.0f);

    // Recompute v based on u and local_pos
    const float u = hit.u;
    const auto v0 = float3(mesh.vertices()[i0]);
    const auto e1 = float3(mesh.vertices()[i1]) - v0;
    const auto e2 = float3(mesh.vertices()[i2]) - v0;
    const float v = dot(local_pos - v0 - u * e1, e2) / dot(e2, e2);

    const auto texcoords    = mesh.attribute<float2>(MeshAttributes::TEXCOORDS);
    const auto normals      = mesh.attribute<float3>(MeshAttributes::NORMALS);
    const auto geom_normals = mesh.attribute<float3>(MeshAttributes::GEOM_NORMALS);

    const auto uv_coords    = lerp(texcoords[i0], texcoords[i1], texcoords[i2], u, v);
    const auto local_normal = lerp(normals[i0], normals[i1], normals[i2], u, v);
    const auto normal       = normalize(float3(local_normal * inst.inv_mat));
    const auto geom_normal  = normalize(float3(geom_normals[local_tri_id] * inst.inv_mat));

    Dual2<OSL::Vec3> point  = OSL::Vec3(ray.org.x, ray.org.y, ray.org.z);
    Dual2<OSL::Vec2> uv     = OSL::Vec2(uv_coords.x, uv_coords.y);
    Dual2<OSL::Vec3> in_dir = OSL::Vec3(ray.dir.x, ray.dir.y, ray.dir.z);

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
    res.surfacearea = length(cross(e1, e2)) * 0.5f * inst.det;

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

    shader_id = m;

    return res;
}

float3 make_float3(const Color3& cl) {
    return float3(cl.x, cl.y, cl.z);
}

void process_closure(MaterialValue& res, const ClosureColor* closure, const Color3& w) {
    if (!closure) return;

    switch (closure->id) {
    case ClosureColor::MUL: {
        Color3 cw = w * closure->as_mul()->weight;
        process_closure(res, closure->as_mul()->closure, cw);
        break;
    }
    case ClosureColor::ADD: {
        process_closure(res, closure->as_add()->closureA, w);
        process_closure(res, closure->as_add()->closureB, w);
        break;
    }
    default: {
        const ClosureComponent* comp = closure->as_comp();
        Color3 cw = w * comp->w;
        if (comp->id == CLOSURE_EMISSION)
            res.emit += make_float3(cw);
        else {
            bool ok = false;
            switch (comp->id) {
            case CLOSURE_DIFFUSE:     ok = res.bsdf.add_bsdf<Diffuse<0>, DiffuseParams   >(cw, *comp->as<DiffuseParams>  ()); break;
            case CLOSURE_TRANSLUCENT: ok = res.bsdf.add_bsdf<Diffuse<1>, DiffuseParams   >(cw, *comp->as<DiffuseParams>  ()); break;
            case CLOSURE_PHONG:       ok = res.bsdf.add_bsdf<Phong     , PhongParams     >(cw, *comp->as<PhongParams>    ()); break;
            case CLOSURE_REFLECTION:  ok = res.bsdf.add_bsdf<Reflection, ReflectionParams>(cw, *comp->as<ReflectionParams>()); break;
            case CLOSURE_REFRACTION:  ok = res.bsdf.add_bsdf<Refraction, RefractionParams>(cw, *comp->as<RefractionParams>()); break;
            }
            ASSERT(ok && "Invalid closure invoked in surface shader");
        }
        break;
    }
    }
}

void MaterialSystem::MatSysInternal::process_closure(MaterialValue& res, const ClosureColor* closure) {
    process_closure(res, closure, Color3(1,1,1));
}

} // namespace imba
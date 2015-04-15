#include "render.hpp"
#include "scene.hpp"

namespace imba {

imba::Camera Render::perspective_camera(Vec3 eye, Vec3 center, Vec3 up, float fov, float ratio) {
    auto setv = [] (::Vec3& v, const Vec3& u) {
        v.values[0] = u[0];
        v.values[1] = u[1];
        v.values[2] = u[2];
    };
   
    ::Camera camera;
    setv(camera.eye, eye);
    setv(camera.center, center);

    Vec3 dist = center - eye;
    float l = length(dist);
    Vec3 d = dist * (1.0f / l);

    Vec3 r = normalize(cross(d, up));
    Vec3 u = cross(r, d);

    assert_normalized(d);
    assert_normalized(r);
    assert_normalized(u);    

    float f = l * tanf(to_radians(fov / 2));

    setv(camera.right, r * f * ratio);
    setv(camera.up, u * f);

    return camera;
}

void Render::render_gbuffer(const Scene& scene, const Camera& camera, GBuffer& output) {
    ::GBuffer buf;
    buf.width  = output.width();
    buf.height = output.height();
    buf.stride = output.stride();
    buf.buffer = output.pixels();
    ::render_gbuffer(const_cast<::Scene*>(scene.sync_.scene_data.get()),
                     const_cast<::CompiledScene*>(scene.sync_.comp_scene.get()),
                     const_cast<Camera*>(&camera), &buf);
}

void Render::render_texture(const Scene& scene, const Camera& camera, Texture& output) {
    ::Texture tex;
    tex.width  = output.width();
    tex.height = output.height();
    tex.stride = output.stride();
    tex.pixels = output.pixels();
    ::render_texture(const_cast<::Scene*>(scene.sync_.scene_data.get()),
                     const_cast<::CompiledScene*>(scene.sync_.comp_scene.get()),
                     const_cast<Camera*>(&camera), &tex);
}

} // namespace imba

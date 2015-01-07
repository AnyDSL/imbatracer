#include "render.hpp"
#include "scene.hpp"

namespace imba {

::Camera Render::perspective_camera(Vec3 eye, Vec3 center, Vec3 up, float fov, float ratio) {
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

    const float pi = 3.14159265359f;
    float f = l * tanf(pi * fov / 360);
    setv(camera.right, r * f);
    setv(camera.up, u * (f / ratio));

    return camera;
}

void Render::render_gbuffer(Scene& scene, const ::Camera& camera, GBuffer& output) {
    scene.synchronize();
    ::GBuffer buf;
    buf.width  = output.width();
    buf.height = output.height();
    buf.stride = output.stride();
    buf.buffer = output.pixels();
    ::render_gbuffer(scene.sync_.scene_data.get(), scene.sync_.comp_scene.get(), (Camera*)&camera, &buf);
}

} // namespace imba

#ifndef IMBA_RENDER_HPP
#define IMBA_RENDER_HPP

#include "../impala/impala_interface.h"
#include "image.hpp"

namespace imba {

class Scene;

/// Rendering functions. This part of imbatracer is subject to change.
class Render {
public:
    // TODO : Camera should be controlled by shader.
    static ::Camera perspective_camera(Vec3 eye, Vec3 center, Vec3 up, float fov, float ratio);
    static void render_gbuffer(const Scene& scene, const ::Camera& camera, GBuffer& output);
    static void render_texture(const Scene& scene, const ::Camera& camera, Texture& output);
};

} // namespace imba

#endif // IMBA_RENDER_HPP


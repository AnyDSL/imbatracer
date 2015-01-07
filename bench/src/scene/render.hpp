#ifndef IMBA_RENDER_HPP
#define IMBA_RENDER_HPP

#include "../impala/impala_interface.h"
#include "image.hpp"

namespace imba {

class Scene;

class Render {
public:
    // TODO : Camera should be controlled by shader.
    static ::Camera perspective_camera(Vec3 eye, Vec3 center, Vec3 up, float fov, float ratio);
    static void render_gbuffer(Scene& scene, const ::Camera& camera, GBuffer& output);
};

} // namespace imba

#endif // IMBA_RENDER_HPP


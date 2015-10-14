#include "../shader.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <cfloat>
#include <cassert>

void imba::BPTShader::sample_lights(RayQueue& out) {
    RNG rng;
}

void imba::BPTShader::shade_light_rays(RayQueue& rays, Image& out, RayQueue& ray_out) {
}

void imba::BPTShader::shade_camera_rays(RayQueue& rays, Image& out, RayQueue& ray_out) {
}

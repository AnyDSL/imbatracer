#ifndef IMBA_LIGHT_HPP
#define IMBA_LIGHT_HPP

#include "../common/math.hpp"
#include <cassert>

namespace imba {

/// Light definition. May be replaced completely by shaders in the not-so-distant future.
class Light {
public:
    enum Type {
        POINT_LIGHT = 0,
        SPOT_LIGHT = 1,
        SPHERE_LIGHT = 2
    };

    /// Creates a point light.
    Light(Type type, const Vec3& pos, const Vec3& intensity) {
        assert(type == POINT_LIGHT);
        light_.light_type = POINT_LIGHT;
        set_position(pos);
        set_intensity(intensity);
        set_direction(Vec3(0.0f, 0.0f, 0.0f));
        light_.radius = 0.0f;
        light_.max_cutoff = 0.0f;
        light_.min_cutoff = 0.0f;
        light_.penumbra = 0.0f;
        light_.accum_alpha = false;
    }

    /// Creates a spot light.
    Light(Type type, const Vec3& pos, const Vec3& intensity, const Vec3& dir, float cutoff, float penumbra) {
        assert(type == SPOT_LIGHT);
        light_.light_type = SPOT_LIGHT;
        set_position(pos);
        set_intensity(intensity);
        set_direction(dir);
        light_.radius = 0.0f;
        light_.min_cutoff = cosf(to_radians(cutoff + penumbra));
        light_.max_cutoff = cosf(to_radians(cutoff));
        light_.penumbra = penumbra;
        light_.accum_alpha = false;
    }

    /// Creates a spherical area light.
    Light(Type type, const Vec3& pos, const Vec3& intensity, float radius) {
        assert(type == SPHERE_LIGHT);
        light_.light_type = SPHERE_LIGHT;
        set_position(pos);
        set_intensity(intensity);
        set_direction(Vec3(0.0f, 0.0f, 0.0f));
        light_.radius = radius;
        light_.max_cutoff = 0.0f;
        light_.min_cutoff = 0.0f;
        light_.penumbra = 0.0f;
        light_.accum_alpha = false;
    }

    Type type() const {
        return Type(light_.light_type);
    }

    Vec3 position() const {
        return Vec3(light_.pos.values[0],
                    light_.pos.values[1],
                    light_.pos.values[2]);
    }

    Vec3 direction() const {
        return Vec3(light_.dir.values[0],
                    light_.dir.values[1],
                    light_.dir.values[2]);
    }

    Vec3 intensity() const {
        return Vec3(light_.intensity.values[0],
                    light_.intensity.values[1],
                    light_.intensity.values[2]);
    }

    void set_position(const Vec3& p) {
        light_.pos.values[0] = p[0];
        light_.pos.values[1] = p[1];
        light_.pos.values[2] = p[2];
    }

    void set_direction(const Vec3& d) {
        light_.dir.values[0] = d[0];
        light_.dir.values[1] = d[1];
        light_.dir.values[2] = d[2];
    }

    void set_intensity(const Vec3& i) {
        light_.intensity.values[0] = i[0];
        light_.intensity.values[1] = i[1];
        light_.intensity.values[2] = i[2];
    }

    void set_accum_alpha(bool accum) {
        light_.accum_alpha = accum ? 1 : 0;
    }

private:
    ::Light light_;
};

} // namespace imba

#endif // IMBA_LIGHT_HPP


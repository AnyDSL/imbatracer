#ifndef IMBA_LIGHT_HPP
#define IMBA_LIGHT_HPP

namespace imba {

/// Light definition. May be replaced completely by shaders in the not-so-distant future.
class Light {
private:
    enum Type {
        POINT_LIGHT = 0,
        SPOT_LIGHT = 1,
        SPHERE_LIGHT = 2
    };

public:
    /// Creates a point light.
    Light(const Vec3& pos, const Vec3& intensity) {
        light_.light_type = POINT_LIGHT;
        set_position(pos);
        set_intensity(intensity);
        light_.radius = 0.0f;
        light_.cutoff = 0.0f;
        light_.penumbra = 0.0f;
    }

    /// Creates a spot light.
    Light(const Vec3& pos, const Vec3& intensity, float cutoff, float penumbra) {
        light_.light_type = SPOT_LIGHT;
        set_position(pos);
        set_intensity(intensity);
        light_.radius = 0.0f;
        light_.cutoff = cutoff;
        light_.penumbra = penumbra;
    }

    /// Creates a spherical area light.
    Light(const Vec3& pos, const Vec3& intensity, float radius) {
        light_.light_type = SPHERE_LIGHT;
        set_position(pos);
        set_intensity(intensity);
        light_.radius = radius;
        light_.cutoff = 0.0f;
        light_.penumbra = 0.0f;
    }

    Vec3 position() const {
        return Vec3(light_.pos.values[0],
                    light_.pos.values[1],
                    light_.pos.values[2]);
    }

    Vec3 intensity() const {
        return Vec3(light_.intensity.values[0],
                    light_.intensity.values[1],
                    light_.intensity.values[2]);
    }

    float cutoff() const {
        return light_.cutoff;
    }

    void set_position(const Vec3& p) {
        light_.pos.values[0] = p[0];
        light_.pos.values[1] = p[1];
        light_.pos.values[2] = p[2];
    }

    void set_intensity(const Vec3& i) {
        light_.intensity.values[0] = i[0];
        light_.intensity.values[1] = i[1];
        light_.intensity.values[2] = i[2];
    }

    void set_cutoff(float cutoff) {
        light_.cutoff = cutoff;
    }

private:
    ::Light light_;
};

} // namespace imba

#endif // IMBA_LIGHT_HPP


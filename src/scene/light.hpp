#ifndef IMBA_LIGHT_HPP
#define IMBA_LIGHT_HPP

namespace imba {

/// Light definition. May be replaced completely by shaders in the not-so-distant future.
class Light {
public:
    Light(const Vec4& pos_dir, const Vec3& falloff = Vec3(1.0f, 0.0f, 0.0f), const Vec3& intensity = Vec3(1.0f), float cutoff = 0.0f) {
        set_position(pos_dir);
        set_falloff(falloff);
        set_intensity(intensity);
        light_.cutoff = cutoff;
    }

    Vec4 position() const {
        return Vec4(light_.pos_dir.values[0],
                    light_.pos_dir.values[1],
                    light_.pos_dir.values[2],
                    light_.pos_dir.values[3]);
    }

    Vec3 falloff() const {
        return Vec3(light_.falloff.values[0],
                    light_.falloff.values[1],
                    light_.falloff.values[2]);
    }

    Vec3 intensity() const {
        return Vec3(light_.intensity.values[0],
                    light_.intensity.values[1],
                    light_.intensity.values[2]);
    }

    float cutoff() const {
        return light_.cutoff;
    }

    void set_position(const Vec4& p) {
        light_.pos_dir.values[0] = p[0];
        light_.pos_dir.values[1] = p[1];
        light_.pos_dir.values[2] = p[2];
        light_.pos_dir.values[3] = p[3];
    }

    void set_falloff(const Vec3& f) {
        light_.falloff.values[0] = f[0];
        light_.falloff.values[1] = f[1];
        light_.falloff.values[2] = f[2];
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


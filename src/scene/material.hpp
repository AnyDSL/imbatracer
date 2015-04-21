#ifndef IMBA_MATERIAL_HPP
#define IMBA_MATERIAL_HPP

namespace imba {

/// Material object. Currently fixed function but could be replaced
/// by shaders in the future.
class Material {
public:
    Material(const Vec3& a = Vec3(0.0f),
             const Vec3& d = Vec3(0.0f),
             const Vec3& s = Vec3(0.0f),
             float exp = 0.0f,
             TextureId tex_a = TextureId(),
             TextureId tex_d = TextureId(),
             TextureId tex_s = TextureId()) {
        set_ambient(a);
        set_diffuse(d);
        set_specular(s);
        mat_.exp = exp;
        mat_.tex_a = tex_a.id;
        mat_.tex_d = tex_d.id;
        mat_.tex_s = tex_s.id;
    }

    Vec3 ambient() const {
        return Vec3(mat_.ka.values[0], mat_.ka.values[1], mat_.ka.values[2]);
    }

    Vec3 diffuse() const {
        return Vec3(mat_.kd.values[0], mat_.kd.values[1], mat_.kd.values[2]);
    }

    Vec3 specular() const {
        return Vec3(mat_.ks.values[0], mat_.ks.values[1], mat_.ks.values[2]);
    }

    float exponent() const {
        return mat_.exp;
    }

    int ambient_texture() const {
        return mat_.tex_a;
    }

    int diffuse_texture() const {
        return mat_.tex_d;
    }

    int specular_texture() const {
        return mat_.tex_s;
    }

    void set_ambient(const Vec3& a) {
        mat_.ka.values[0] = a[0];
        mat_.ka.values[1] = a[1];
        mat_.ka.values[2] = a[2];
    }

    void set_diffuse(const Vec3& d) {
        mat_.kd.values[0] = d[0];
        mat_.kd.values[1] = d[1];
        mat_.kd.values[2] = d[2];
    }

    void set_specular(const Vec3& s) {
        mat_.ks.values[0] = s[0];
        mat_.ks.values[1] = s[1];
        mat_.ks.values[2] = s[2];
    }

    void set_exponent(float exp) {
        mat_.exp = exp;
    }

    void set_ambient_texture(TextureId tex_id) {
        mat_.tex_a = tex_id.id;
    }

    void set_diffuse_texture(TextureId tex_id) {
        mat_.tex_d = tex_id.id;
    }

    void set_specular_texture(TextureId tex_id, int levels = -1) {
        mat_.tex_s = tex_id.id;
    }

private:
    ::Material mat_;
};

} // namespace imba

#endif // IMBA_MATERIAL_HPP


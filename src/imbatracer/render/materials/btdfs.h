#ifndef IMBA_BTDFS_H
#define IMBA_BTDFS_H

namespace imba {

template<bool adjoint>
class SpecularTransmission : public BxDF {
public:
    SpecularTransmission(float eta_inside, float eta_outside, const float3& n)
        : BxDF(n),
          fresnel_(eta_outside, eta_inside),
          eta_outside_(eta_outside),
          eta_inside_(eta_inside)
    {}

    rgb eval(const float3& out_dir, const float3& in_dir) const override {
        return rgb(0.0f);
    }

    rgb sample(const float3& out_dir, float3& in_dir, RNG& rng, float& pdf) const override {
        pdf = 1.0f;

        // Compute optical densities depending on whether the ray is coming from the outside or the inside.
        float eta_in = eta_outside_;
        float eta_trans = eta_inside_;
        float c_out = cos_theta(out_dir);
        float3 n = normal;
        if (c_out < 0.0f) {
            n = -normal;
            std::swap(eta_in, eta_trans);
        }

        // Compute the direction of the transmitted ray.
        float eta_frac = eta_in / eta_trans;
        float sin_trans_sqr = sqr(eta_frac) * (1.0f - c_out * c_out);

        if (sin_trans_sqr > 1.0f) {
            in_dir = float3(0,0,0);
            return rgb(0.0f); // Total internal reflection.
        }

        in_dir = eta_frac * -out_dir + n * (eta_frac * fabsf(c_out) - sqrt(1.0f - sin_trans_sqr));

        float fr = fresnel_.eval(c_out);
        float factor = adjoint ? 1.0f : sqr(eta_in / eta_trans);

        return rgb(factor * (1.0f - fr));
    }

    float albedo(const float3& out_dir) const override {
        float fr = fresnel_.eval(cos_theta(out_dir));
        return 1.0f - fr;
    }

    float pdf(const float3& out_dir, const float3& in_dir) const override {
        return 0.0f; // Probability between any two randomly choosen directions is zero due to the delta distribution.
    }

    bool specular() const override { return true; }

private:
    FresnelDielectric fresnel_;
    float eta_outside_, eta_inside_;
};

} // namespace imba

#endif // IMBA_BTDFS_H

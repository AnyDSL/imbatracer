#ifndef IMBA_BTDFS_H
#define IMBA_BTDFS_H

namespace imba {

template<bool adjoint>
class SpecularTransmission : public BxDF {
public:
    SpecularTransmission(float eta_inside, float eta_outside, const rgb& scale)
        : BxDF(BxDFFlags(BSDF_TRANSMISSION | BSDF_SPECULAR)),
          fresnel_(eta_outside, eta_inside),
          scale_(scale),
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
        bool entering = cos_theta(out_dir) > 0.0f;
        if (!entering)
            std::swap(eta_in, eta_trans);

        // Compute the direction of the transmitted ray.
        float sin_in_sqr = sin_theta_sqr(out_dir);
        float eta_frac = eta_in / eta_trans;
        float sin_trans_sqr = sqr(eta_frac) * sin_in_sqr;

        if (sin_trans_sqr >= 1.0f) {
            in_dir = float3(-out_dir.x, -out_dir.y, out_dir.z);
            return rgb(0.0f); // Total internal reflection.
        }

        float cos_trans = sqrtf(std::max(0.0f, 1.0f - sin_trans_sqr));
        if (entering) cos_trans = -cos_trans;

        in_dir = float3(eta_frac * -out_dir.x, eta_frac * -out_dir.y, cos_trans);

        float fr = fresnel_.eval(cos_theta(out_dir));
        float factor = adjoint ? 1.0f : sqr(eta_in / eta_trans);

        return factor * (1.0f - fr) * scale_ / fabsf(cos_theta(in_dir));
    }

    float importance(const float3& out_dir) const override {
        float fr = fresnel_.eval(cos_theta(out_dir));
        return 1.0f - fr;
    }

    float pdf(const float3& out_dir, const float3& in_dir) const override {
        return 0.0f; // Probability between any two randomly choosen directions is zero due to the delta distribution.
    }

private:
    FresnelDielectric fresnel_;
    rgb scale_;
    float eta_outside_, eta_inside_;
};

} // namespace imba

#endif // IMBA_BTDFS_H

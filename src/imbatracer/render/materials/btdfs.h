#ifndef IMBA_BTDFS_H
#define IMBA_BTDFS_H

namespace imba {

class SpecularTransmission : public BxDF {
public:
    SpecularTransmission(float eta_inside, float eta_outside, const float4& scale)
        : BxDF(BxDFFlags(BSDF_TRANSMISSION | BSDF_SPECULAR)),
          fresnel_(eta_outside, eta_inside),
          scale_(scale),
          eta_outside_(eta_outside),
          eta_inside_(eta_inside)
    {}

    virtual float4 eval(const float3& out_dir, const float3& in_dir) const override {
        return float4(0.0f);
    }

    virtual float4 sample(const float3& out_dir, float3& in_dir, float rnd_num_1, float rnd_num_2, float& pdf) const override {
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
            return float4(0.0f); // Total internal reflection.
        }

        float cos_trans = sqrtf(std::max(0.0f, 1.0f - sin_trans_sqr));
        if (entering) cos_trans = -cos_trans;

        in_dir = float3(eta_frac * -out_dir.x, eta_frac * -out_dir.y, cos_trans);

        float fr = fresnel_.eval(cos_theta(out_dir));

        return /*sqr(eta_trans) / sqr(eta_in) **/ (1.0f - fr) * scale_ / fabsf(cos_theta(in_dir));  // TODO we need to consider adjoint here.
    }

    virtual float pdf(const float3& out_dir, const float3& in_dir) const override {
        return 0.0f; // Probability between any two randomly choosen directions is zero due to the delta distribution.
    }

private:
    FresnelDielectric fresnel_;
    float4 scale_;
    float eta_outside_, eta_inside_;
};

}

#endif

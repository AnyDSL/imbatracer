#ifndef IMBA_FRESNEL_H
#define IMBA_FRESNEL_H

namespace imba {

namespace {
    inline float fresnel_conductor(float cosi, float eta, float kappa)
    {
        const float ekc = (eta*eta + kappa*kappa) * cosi*cosi;
        const float par =
            (ekc - (2.f * eta * cosi) + 1) /
            (ekc + (2.f * eta * cosi) + 1);

        const float ek = eta*eta + kappa*kappa;
        const float perp =
            (ek - (2.f * eta * cosi) + cosi*cosi) /
            (ek + (2.f * eta * cosi) + cosi*cosi);

        return (par + perp) / 2.f;
    }

    inline float fresnel_dielectric(float cosi, float coso, float etai, float etao)
    {
        const float par  = (etao * cosi - etai * coso) / (etao * cosi + etai * coso);
        const float perp = (etai * cosi - etao * coso) / (etai * cosi + etao * coso);

        return (par * par + perp * perp) / 2.f;
    }
}

class Fresnel {
public:
    virtual float eval(float cosi) const = 0;
};

class FresnelConductor : public Fresnel {
public:
    FresnelConductor(float eta, float kappa) : eta_(eta), kappa_(kappa) {}

    virtual float eval(float cosi) const override {
        return fresnel_conductor(cosi, eta_, kappa_);
    }

private:
    float eta_;
    float kappa_;
};

class FresnelDielectric : public Fresnel {
public:
    FresnelDielectric(float eta_outside, float eta_inside)
        : eta_outside_(eta_outside), eta_inside_(eta_inside) {}

    virtual float eval(float cosi) const override {
        // Compute indices of refraction according to whether the ray is coming from inside or outside.
        float eta_in = eta_outside_;
        float eta_trans = eta_inside_;
        if (cosi <= 0.0f)
            std::swap(eta_in, eta_trans);

        // Use Snell's law to compute the sine of the transmitted direction.
        float sin_trans = eta_in / eta_trans * sqrtf(std::max(0.0f, 1.0f - sqr(cosi)));

        if (sin_trans >= 1.0f) {
            return 1.0f; // Total internal reflection.
        } else {
            float cos_trans = sqrtf(std::max(0.0f, 1.0f - sqr(sin_trans)));
            return fresnel_dielectric(fabsf(cosi), cos_trans, eta_in, eta_trans);
        }
    }

private:
    float eta_outside_;
    float eta_inside_;
};

} // namespace imba

#endif // IMBA_FRESNEL_H


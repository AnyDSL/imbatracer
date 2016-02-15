#ifndef IMBA_FRESNEL_H
#define IMBA_FRESNEL_H

namespace imba {

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

#endif
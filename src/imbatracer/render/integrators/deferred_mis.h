#ifndef IMBA_DEFERRED_MIS_H
#define IMBA_DEFERRED_MIS_H

#include "imbatracer/core/common.h"

namespace imba {

// TODO: use TMP for enabling / disabling techniques (optimization)

enum MISTechnique : int {
    MIS_CONNECT       = 1 << 1,
    MIS_MERGE         = 1 << 2,
    MIS_HIT           = 1 << 3,
    MIS_NEXTEVT_LIGHT = 1 << 4,
    MIS_NEXTEVT_CAM   = 1 << 5,
    MIS_ALL = MIS_CONNECT | MIS_MERGE | MIS_HIT | MIS_NEXTEVT_LIGHT | MIS_NEXTEVT_CAM
};

static inline float mis_heuristic(float p) {
    return p * p;
}

struct PartialMIS {
    static void setup_iteration(float radius, float lp_count, int techniques) {
        light_path_count = lp_count;
        vcm_weight = pi * sqr(radius) * light_path_count;
        vc_weight = mis_heuristic(1.0f / vcm_weight);
        vm_weight = techniques & MIS_MERGE ? 0.0f : mis_heuristic(vcm_weight);
    }

    inline void init_camera(float pdf) {
        reversible = true; // camera can always be connected to

        connect = 0.0f;
        merge   = 0.0f;
        unidir  = mis_heuristic(light_path_count / pdf);
    }

    inline void init_light(float pdf_emit_w, float pdf_di_a, float pdf_lightpick, float cos_out, bool finite, bool delta) {
        reversible = finite; // lights that are infinitely far away cannot be connected to

        unidir = mis_heuristic(pdf_di_a / pdf_emit_w); // pdf_lightpick cancels out
        connect = delta ? 0.0f : mis_heuristic(cos_out / (pdf_emit_w * pdf_lightpick));
        merge = connect * vc_weight;
    }

    inline void update_hit(float cos_theta_o, float d2) {
        if (reversible)
            unidir *= mis_heuristic(d2);

        // After the first hit, the path is always reversible
        reversible = true;

        unidir  *= 1.0f / mis_heuristic(cos_theta_o);
        connect *= 1.0f / mis_heuristic(cos_theta_o);
        merge   *= 1.0f / mis_heuristic(cos_theta_o);
    }

    inline void update_bounce(float pdf_dir_w, float pdf_rev_w, float cos_theta_i, bool specular) {
        if (specular) {
            unidir = 0.0f;
            connect *= mis_heuristic(cos_theta_i);
            merge   *= mis_heuristic(cos_theta_i);
        } else {
            connect = mis_heuristic(cos_theta_i / pdf_dir_w) *
                    (connect * mis_heuristic(pdf_rev_w) + unidir + vm_weight);

            merge = mis_heuristic(cos_theta_i / pdf_dir_w) *
                    (merge * mis_heuristic(pdf_rev_w) + unidir * vc_weight + 1.0f);

            unidir = mis_heuristic(1.0f / pdf_dir_w);
        }
    }

    static float vcm_weight;
    static float vc_weight;
    static float vm_weight;
    static int light_path_count;
    static int techniques;

    float connect;
    float merge;
    float unidir;

    bool reversible;
};

// TODO: implement special cases for all possible combinations of techniques (e.g. connections only)

inline float mis_weight_connect(PartialMIS cam, PartialMIS light,
                                float pdf_cam_w, float pdf_rev_cam_w, float pdf_light_w, float pdf_rev_light_w,
                                float cos_cam, float cos_light, float d2) {
    const float pdf_cam_a = pdf_cam_w * cos_light / d2;
    const float pdf_light_a = pdf_light_w * cos_cam / d2;

    const float mis_weight_light = mis_heuristic(pdf_cam_a) * (PartialMIS::vm_weight + light.unidir + light.connect * mis_heuristic(pdf_rev_light_w));
    const float mis_weight_camera = mis_heuristic(pdf_light_a) * (PartialMIS::vm_weight + cam.unidir + cam.connect * mis_heuristic(pdf_rev_cam_w));

    if (cam.techniques == MIS_CONNECT)
        return 1.0f;
    else
        return 1.0f / (mis_weight_camera + 1.0f + mis_weight_light);
}

inline float mis_weight_merge(PartialMIS cam, PartialMIS light, float pdf_dir_w, float pdf_rev_w) {
    const float mis_weight_light = light.unidir * PartialMIS::vc_weight + light.merge * mis_heuristic(pdf_dir_w);
    const float mis_weight_camera = cam.unidir * PartialMIS::vc_weight + cam.merge * mis_heuristic(pdf_rev_w);

    if (cam.techniques == MIS_MERGE)
        return 1.0f;
    else
        return 1.0f / (mis_weight_light + 1.0f + mis_weight_camera);
}

inline float mis_weight_hit(PartialMIS cam, float pdf_direct_a, float pdf_emit_w, float pdf_lightpick) {
    const float pdf_di = pdf_direct_a * pdf_lightpick;
    const float pdf_e = pdf_emit_w * pdf_lightpick;

    const float mis_weight_camera = mis_heuristic(pdf_di) * cam.unidir + mis_heuristic(pdf_e) * cam.connect;

    if (cam.techniques == MIS_HIT)
        return 1.0f;
    else
        return 1.0f / (mis_weight_camera + 1.0f);
}

inline float mis_weight_cam_connect(PartialMIS light, float pdf_cam, float cos_theta_surf, float d2, float pdf_light) {
    pdf_cam *= cos_theta_surf / d2;

    const float mis_weight_light = mis_heuristic(pdf_cam / PartialMIS::light_path_count) *
                                   (PartialMIS::vm_weight + light.unidir + light.connect * mis_heuristic(pdf_light));

    if (light.techniques == MIS_NEXTEVT_LIGHT)
        return 1.0f;
    else
        return 1.0f / (mis_weight_light + 1.0f);
}

inline float mis_weight_di(PartialMIS cam, float pdf_cam_w, float pdf_di_w, float pdf_emit_w, float pdf_lightpick_inv,
                           float pdf_rev_w, float cos_theta_i, float cos_theta_o) {
    const float mis_weight_light = mis_heuristic(pdf_cam_w * pdf_lightpick_inv / pdf_di_w);
    const float mis_weight_camera = mis_heuristic(pdf_emit_w * cos_theta_i / (pdf_di_w * cos_theta_o)) *
                                    (PartialMIS::vm_weight + cam.unidir + cam.connect * mis_heuristic(pdf_rev_w));

    if (cam.techniques == MIS_NEXTEVT_CAM)
        return 1.0f;
    else
        return 1.0f / (mis_weight_camera + 1.0f + mis_weight_light);
}

} // namespace imba

#endif // IMBA_DEFERRED_MIS_H
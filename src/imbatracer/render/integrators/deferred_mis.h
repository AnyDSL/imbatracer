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

    MIS_ALL = MIS_CONNECT | MIS_MERGE | MIS_HIT | MIS_NEXTEVT_LIGHT | MIS_NEXTEVT_CAM,
    MIS_ADJOINTS = MIS_CONNECT | MIS_MERGE | MIS_NEXTEVT_LIGHT,
    MIS_PT = MIS_HIT | MIS_NEXTEVT_CAM,
};

static inline float mis_heuristic(float p) {
    return p;
}

struct PartialMIS {
    static void setup_iteration(float radius, float lp_count, int techniques) {
        PartialMIS::techniques = techniques;
        light_path_count = lp_count;
        pdf_merge = techniques & MIS_MERGE ? mis_heuristic(pi * sqr(radius) * light_path_count) : 0.0f;
    }

    inline void init_camera(float pdf) {
        reversible = true; // camera can always be connected to

        partial_mis = 0.0f;
        last_pdf  = mis_heuristic(light_path_count / pdf);
    }

    inline void init_light(float pdf_emit_w, float pdf_di_a, float pdf_lightpick, float cos_out, bool finite, bool delta) {
        reversible = finite; // lights that are infinitely far away cannot be connected to

        if (!(techniques & MIS_NEXTEVT_CAM)) pdf_di_a = 0.0f;

        last_pdf = mis_heuristic(pdf_di_a / pdf_emit_w); // pdf_lightpick cancels out
        partial_mis = delta ? 0.0f : mis_heuristic(cos_out / (pdf_emit_w * pdf_lightpick));
    }

    inline void update_hit(float cos_theta_o, float d2) {
        if (reversible)
            last_pdf *= mis_heuristic(d2);

        // After the first hit, the path is always reversible
        reversible = true;

        last_pdf    *= 1.0f / mis_heuristic(cos_theta_o);
        partial_mis *= 1.0f / mis_heuristic(cos_theta_o);
    }

    inline void update_bounce(float pdf_dir_w, float pdf_rev_w, float cos_theta_i, bool specular) {
        if (specular) {
            last_pdf = 0.0f;
            partial_mis *= mis_heuristic(cos_theta_i);
        } else {
            partial_mis = mis_heuristic(cos_theta_i / pdf_dir_w) *
                          (partial_mis * mis_heuristic(pdf_rev_w) + last_pdf + pdf_merge);

            last_pdf = mis_heuristic(1.0f / pdf_dir_w);
        }
    }

    static float pdf_merge;
    static int light_path_count;
    static int techniques;

    float partial_mis; ///< Sum of all (n*p)^b computed so far
    float last_pdf; ///< 1/p of the last bounce, (partially) converted to area measure

    bool reversible;
};

// TODO: implement special cases for all possible combinations of techniques (e.g. connections only, connections and merging only, ...)

inline float mis_weight_connect(PartialMIS cam, PartialMIS light,
                                float pdf_cam_w, float pdf_rev_cam_w, float pdf_light_w, float pdf_rev_light_w,
                                float cos_cam, float cos_light, float d2) {
    const float pdf_cam_a = pdf_cam_w * cos_light / d2;
    const float pdf_light_a = pdf_light_w * cos_cam / d2;

    const float mis_weight_light = mis_heuristic(pdf_cam_a) * (PartialMIS::pdf_merge + light.last_pdf + light.partial_mis * mis_heuristic(pdf_rev_light_w));
    const float mis_weight_camera = mis_heuristic(pdf_light_a) * (PartialMIS::pdf_merge + cam.last_pdf + cam.partial_mis * mis_heuristic(pdf_rev_cam_w));

    if (PartialMIS::techniques == MIS_CONNECT)
        return 1.0f;
    else
        return 1.0f / (mis_weight_camera + 1.0f + mis_weight_light);
}

inline float mis_weight_merge(PartialMIS cam, PartialMIS light, float pdf_dir_w, float pdf_rev_w) {
    float pdf_merge_inv = 1.0f / PartialMIS::pdf_merge;
    const float mis_weight_light  = pdf_merge_inv * (light.last_pdf + light.partial_mis * mis_heuristic(pdf_dir_w));
    const float mis_weight_camera = pdf_merge_inv * (  cam.last_pdf + cam.partial_mis * mis_heuristic(pdf_rev_w));

    return 1.0f / (mis_weight_light + 1.0f + mis_weight_camera);
}

inline float mis_weight_hit(PartialMIS cam, float pdf_direct_a, float pdf_emit_w, float pdf_lightpick, int path_len) {
    const float pdf_di = PartialMIS::techniques & MIS_NEXTEVT_CAM ? pdf_direct_a * pdf_lightpick : 0.0f;
    const float pdf_e  = PartialMIS::techniques & MIS_ADJOINTS ? pdf_emit_w * pdf_lightpick : 0.0f;

    const float mis_weight_camera = mis_heuristic(pdf_di) * cam.last_pdf + mis_heuristic(pdf_e) * cam.partial_mis;

    if (PartialMIS::techniques == MIS_HIT || path_len == 2)
        return 1.0f;
    else
        return 1.0f / (mis_weight_camera + 1.0f);
}

inline float mis_weight_cam_connect(PartialMIS light, float pdf_cam, float cos_theta_surf, float d2, float pdf_light) {
    if (!(PartialMIS::techniques & MIS_PT)) pdf_cam = 0.0f;

    const float mis_weight_light = mis_heuristic(pdf_cam / PartialMIS::light_path_count)
                                 * (PartialMIS::pdf_merge
                                    + light.last_pdf
                                    + (PartialMIS::techniques & MIS_CONNECT ? light.partial_mis * mis_heuristic(pdf_light) : 0.0f));

    if (PartialMIS::techniques == MIS_NEXTEVT_LIGHT)
        return 1.0f;
    else
        return 1.0f / (mis_weight_light + 1.0f);
}

inline float mis_weight_di(PartialMIS cam, float pdf_dir_w, float pdf_rev_w, float pdf_di_w, float pdf_emit_w, float pdf_lightpick_inv,
                           float cos_theta_i, float cos_theta_o, bool delta_light) {
    const float mis_weight_light  = !delta_light && (PartialMIS::techniques & MIS_HIT)
                                  ? mis_heuristic(pdf_dir_w / pdf_di_w * pdf_lightpick_inv)
                                  : 0.0f;

    if (!(PartialMIS::techniques & MIS_ADJOINTS)) pdf_emit_w = 0.0f;

    const float mis_weight_camera = mis_heuristic(pdf_emit_w * cos_theta_i / (pdf_di_w * cos_theta_o))
                                  * (PartialMIS::pdf_merge
                                    + (PartialMIS::techniques & MIS_HIT ? cam.last_pdf : 0.0f)
                                    + (PartialMIS::techniques & MIS_CONNECT ? cam.partial_mis * mis_heuristic(pdf_rev_w) : 0.0f));

    if (PartialMIS::techniques == MIS_NEXTEVT_CAM)
        return 1.0f;
    else
        return 1.0f / (mis_weight_camera + 1.0f + mis_weight_light);
}

} // namespace imba

#endif // IMBA_DEFERRED_MIS_H
#ifndef IMBA_DEFERRED_MIS_H
#define IMBA_DEFERRED_MIS_H

#include "imbatracer/core/common.h"

#include <cassert>

namespace imba {

// TODO: document each function and parameter her (MisHelper class, the interface for the "outside world")

// TODO: move everything but the MisHelper, mis_weight(), and declarations of supported MisHelper-derived classes to the .cpp file

#define DERIVED_FUNC(N, ...) static_cast<Derived*>(this)->N(__VA_ARGS__)

namespace mis {

template <typename Derived>
struct MisHelper {
    /// Computes and returns the weight of the given technique.
    template <typename... Ts>
    float weight(Ts... args) const { DERIVED_FUNC(weight, args...); }

    /// Initializes the MIS for camera paths.
    /// Calling this function multiple times results in undefined behaviour.
    /// \param pdf_cam_w The pdf of sampling the primary ray from the camera (solid angle)
    void init_camera(int num_light_paths, float pdf_cam_w) {
        DERIVED_FUNC(init_camera, num_light_paths, pdf_cam_w);
    }

    void init_light(float pdf_emit_w, float pdf_di_a, float pdf_lightpick, float cos_in_light, bool finite, bool delta) {
        DERIVED_FUNC(init_light, pdf_emit_w, pdf_di_a, pdf_lightpick, cos_in_light, finite, delta);
    }

    /// Updates the MIS weights after intersecting the ray with the scene.
    /// Conversion between area/solid angle measures requires the cosine at this hit point,
    /// as well as the squared distance between it an the last one.
    /// \param cos_out  The cosine of the angle formed by the normal and the direction of the last ray (outgoing radiance / importance in PT / LT)
    /// \param d2       The squared distance between this vertex and the last one
    void update_hit(float cos_out, float d2) {
        DERIVED_FUNC(update_hit, cos_out, d2);
    }

    /// Updates the MIS weights after bouncing (sampling a direction either from the BRDF or something else)
    /// \param cos_in Cosine of the direction we are bouncing to (incoming radiance / importance in PT / LT)
    void update_bounce(float pdf_dir_w, float pdf_rev_w, float cos_in, bool specular, float merge_weight) {
        DERIVED_FUNC(update_bounce, pdf_dir_w, pdf_rev_w, cos_in, specular, merge_weight);
    }
};

static inline float pow_heuristic(float p) {
    return p;
}

#define TECH_UPDATE_NOOP(NAME) template<typename... Ts> static void NAME(Ts... params, float* last_pdf, float* partial) {}
#define TECH_FINALIZE_LIGHT_NOOP static float finalize_on_emitter(float merge_weight, float pdf_di_a, float last_pdf, float partial) {}
#define TECH_FINALIZE_SURF_NOOP  static float finalize(float merge_weight, float last_pdf, float partial) {}

struct DirectIllum {
    TECH_UPDATE_NOOP(init_camera);
    TECH_UPDATE_NOOP(update_bounce); // Direct illumination is not done in the middle of a path

    static void init_light(float pdf_emit_w, float pdf_di_a, float pdf_lightpick, float cos_in_light, bool finite, bool delta,
                           float* last_pdf, float* partial) {
        *last_pdf = pow_heuristic(pdf_di_a / pdf_emit_w); // pdf_lightpick cancels out
    }

    static float finalize_on_emitter(float merge_weight, float pdf_di_a, float last_pdf, float partial) {
        // There was no emission from the light source -> weight for DI not yet accounted for
        return last_pdf * pdf_di_a;
    }

    TECH_FINALIZE_SURF_NOOP;
};

struct UnidirPT {
    TECH_UPDATE_NOOP(init_camera);
    TECH_UPDATE_NOOP(update_bounce); // Hitting a light source cannot happen at a non-light vertex

    static void init_light(float pdf_emit_w, float pdf_di_a, float pdf_lightpick, float cos_in_light, bool finite, bool delta,
                           float* last_pdf, float* partial) {
        if(!delta)
            pow_heuristic(cos_in_light / (pdf_emit_w * pdf_lightpick));
    }

    TECH_FINALIZE_SURF_NOOP;
    TECH_FINALIZE_LIGHT_NOOP;
};

struct ConnectLT {
    TECH_UPDATE_NOOP(init_light);
    TECH_UPDATE_NOOP(update_bounce); // Next Event Estimation in LT only considers camera paths of length 1

    static void init_camera(int num_light_paths, float pdf_cam_w, float* last_pdf, float* partial) {
        *last_pdf = num_light_paths / pdf_cam_w;
    }

    TECH_FINALIZE_SURF_NOOP;
    TECH_FINALIZE_LIGHT_NOOP;
};

struct Connect {
    TECH_UPDATE_NOOP(init_camera);
    TECH_UPDATE_NOOP(init_light); // No connections to light source vertices

    static void update_bounce(float pdf_dir_w, float pdf_rev_w, float cos_in, bool specular, float merge_weight, float* last_pdf, float* partial) {
        // Assumes that the pdf for a connection is 1
        // The partial weight for a connection at some vertex is 1/p, the last_pdf
        *partial += *last_pdf;
    }

    TECH_FINALIZE_LIGHT_NOOP; // No connections to vertices on the light

    static float finalize(float merge_weight, float last_pdf, float partial) {
        // Include weight for connection instead of the last bounce
        return last_pdf;
    }
};

struct Merge {
    TECH_UPDATE_NOOP(init_camera);
    TECH_UPDATE_NOOP(init_light); // No merging on the light source

    static void update_bounce(float pdf_dir_w, float pdf_rev_w, float cos_in, bool specular, float merge_weight, float* last_pdf, float* partial) {
        *partial += merge_weight;
    }

    TECH_FINALIZE_LIGHT_NOOP; // No merging on the light source

    static float finalize(float merge_weight, float last_pdf, float partial) {
        // Include weight for merging at the last bounce
        return merge_weight;
    }
};

#undef TECH_UPDATE_NOOP

/// Combines multiple MIS techniques.
struct Algorithm {
    Algorithm() : last_pdf(0.0f), partial(0.0f), started_at_infinity(false) {}

    #define MAKE_TECH_FN(NAME)                              \
    template <typename CurTech, typename... Techs>          \
    void NAME(MAKE_FN_PARAMS, CurTech*, Techs*... r) {      \
        CurTech::NAME(MAKE_FN_PNAMES, &partial, &last_pdf); \
        NAME(MAKE_FN_PNAMES, r...);                         \
    }                                                       \
    template <typename CurTech>                             \
    void NAME(MAKE_FN_PARAMS, CurTech*) {                   \
        CurTech::NAME(MAKE_FN_PNAMES, &partial, &last_pdf); \
    }

    //////////////////////////////////////////////////////////////////
    // Camera path creation
    #define MAKE_FN_PARAMS int num_light_paths, float pdf_cam_w
    #define MAKE_FN_PNAMES num_light_paths, pdf_cam_w
    MAKE_TECH_FN(init_camera);
    #undef MAKE_FN_PARAMS
    #undef MAKE_FN_PNAMES

    //////////////////////////////////////////////////////////////////
    // Light path creation
    #define MAKE_FN_PARAMS float pdf_emit_w, float pdf_di_a, float pdf_lightpick, float cos_in_light, bool finite, bool delta
    #define MAKE_FN_PNAMES pdf_emit_w, pdf_di_a, pdf_lightpick, cos_in_light, delta
    MAKE_TECH_FN(init_light_helper);
    template<typename... Techs>
    void init_light(MAKE_FN_PARAMS, Techs*... ts) {
        // Allow all techniques to take action.
        init_light_helper(MAKE_FN_PNAMES, ts...);

        // We need to keep track of lights that are at infinity (these must not be multiplied by sqared distance!)
        started_at_infinity = !finite;
    }
    #undef MAKE_FN_PARAMS
    #undef MAKE_FN_PNAMES

    //////////////////////////////////////////////////////////////////
    // Bouncing
    #define MAKE_FN_PARAMS float pdf_dir_w, float pdf_rev_w, float cos_in, bool specular, float merge_weight
    #define MAKE_FN_PNAMES pdf_dir_w, pdf_rev_w, cos_in, specular, merge_weight
    MAKE_TECH_FN(update_bounce_helper);
    template<typename... Techs>
    void update_bounce(MAKE_FN_PARAMS, Techs*... ts) {
        // Account for the (now known) reverse pdf in the partials.
        partial *= pdf_rev_w;

        // Allow all techniques to take action.
        update_bounce_helper(MAKE_FN_PNAMES, ts...);

        // Divide by the technique used for sampling
        // Include the cosine for solid-angle -> area (converts the reverse PDF at the next vertex)
        partial *= cos_in / pdf_dir_w;

        // Store the pdf for this bounce. It is converted to area at the next hit and (maybe) used for this next vertex.
        last_pdf = pow_heuristic(1.0f / pdf_dir_w);
    }
    #undef MAKE_FN_PARAMS
    #undef MAKE_FN_PNAMES

    #undef MAKE_TECH_FN

    /// Converts pdfs from the last bounce to area measure, independent of the techniques.
    void update_hit(int num_light_paths, float cos_out, float d2) {
        if (!started_at_infinity) {
            last_pdf *= pow_heuristic(d2);
            started_at_infinity = false;
        }

        last_pdf *= 1.0f / pow_heuristic(cos_out);

        // The squared distance cancels out for the partials as they divide each pdf by its corresponding reverse.
        partial  *= 1.0f / pow_heuristic(cos_out);
    }

    /// Computes the final partial sum for this subpath, given the reverse pdf (which is known only when using this subpath)
    template <typename... Techs>
    float finalize(float pdf_rev_w, float merge_weight, Techs*... techs) const {
        float res = partial * pdf_rev_w;
        return res + finalize_helper(merge_weight, techs...);
    }

    /// Computes the final partial sum for the special case of a path hitting an emitter (camera sensor or light)
    template <typename... Techs>
    float finalize_on_emitter(float pdf_rev_w, float merge_weight, float pdf_di_a, Techs*... techs) const {
        float res = partial * pdf_rev_w;
        return res + finalize_emitter_helper(merge_weight, techs...);
    }

    /// The partial MIS sum computed so far.
    float partial;

    /// 1/pdf at the last bounce
    float last_pdf;

    /// True if we are at the first vertex on a light path and the light was infinitely far away.
    /// Required to support environment maps and directional lights.
    bool started_at_infinity;

private:
    template <typename CurTech, typename... Techs>
    float finalize_helper(float merge_weight, CurTech* ctech, Techs*... remainder) const {
        return finalize_helper(merge_weight, ctech)
             + finalize_helper(merge_weight, remainder...);
    }

    template<typename CurTech>
    float finalize_helper(float merge_weight, CurTech* ctech) const {
        return CurTech::template finalize(merge_weight, last_pdf, partial);
    }

    template <typename CurTech, typename... Techs>
    float finalize_emitter_helper(float merge_weight, float pdf_di_a, CurTech* ctech, Techs*... remainder) const {
        return finalize_emitter_helper(merge_weight, pdf_di_a, ctech)
             + finalize_emitter_helper(merge_weight, pdf_di_a, remainder...);
    }

    template<typename CurTech>
    float finalize_emitter_helper(float merge_weight, float pdf_di_a, CurTech* ctech) const {
        return CurTech::template finalize(merge_weight, pdf_di_a, last_pdf, partial);
    }
};

/// Computes the partial weight for merging.
/// This is a function for simplicity. The weight should be precomputed by the caller,
/// storing the same weight in multiple (= millions of) partials is not efficient.
inline float merge_accept_weight(int num_light_paths, float radius) {
    pow_heuristic(pi * sqr(radius) * num_light_paths);
}

template <typename T>
inline float weight_connect(const MisHelper<T>& cam, const MisHelper<T>& light, float merge_weight,
                            float pdf_cam_w, float pdf_rev_cam_w, float pdf_light_w, float pdf_rev_light_w,
                            float cos_cam, float cos_light, float d2) {
    // Convert to area measure
    const float pdf_cam_a   = pdf_cam_w   * cos_light / d2;
    const float pdf_light_a = pdf_light_w * cos_cam   / d2;

    // Add missing unidirectional pdfs
    const float wc = pdf_light_a *   cam.template weight(pdf_rev_cam_w  , merge_weight);
    const float wl = pdf_cam_a   * light.template weight(pdf_rev_light_w, merge_weight);

    return 1.0f / (wc + 1.0f + wl);
}

template <typename T>
inline float weight_merge(const MisHelper<T>& cam, const MisHelper<T>& light, float merge_weight, float pdf_dir_w, float pdf_rev_w) {
    float merge_weight_inv = 1.0f / merge_weight; // TODO could also be precomputed

    // Uses the fact that the pdf for merging is the pdf for connecting times the merge acceptance probability
    const float wl = merge_weight_inv * light.template weight(pdf_dir_w, merge_weight);
    const float wc = merge_weight_inv *   cam.template weight(pdf_rev_w, merge_weight);

    return 1.0f / (wc + 1.0f + wl);
}

template <typename T>
inline float weight_upt(const MisHelper<T>& path, float merge_weight, float pdf_direct_a, float pdf_emit_w, float pdf_lightpick, int path_len) {
    if (path_len == 2) return 1.0f; // Light directly visible

    const float pdf_di = pdf_direct_a * pdf_lightpick;
    const float pdf_e  = pdf_emit_w   * pdf_lightpick;

    return 1.0f / (path.template weight_on_emitter(pdf_e, merge_weight, pdf_di) + 1.0f);
}

template <typename T>
inline float weight_di(const MisHelper<T>& path, float merge_weight,
                    float pdf_dir_w, float pdf_rev_w, float pdf_di_w, float pdf_emit_w, float pdf_lightpick_inv,
                    float cos_in, float cos_out, bool delta_light) {
    // TODO: this is actually accounting for the weight from hitting the light source vs DI
    // -> maybe include this in the UnidirPT technique somehow?
    // Ignoring hitting the light source does not make much sense though, hence this is probably always the same
    const float wl = !delta_light ? pow_heuristic(pdf_dir_w / pdf_di_w * pdf_lightpick_inv) : 0.0f;

    const float wc = pow_heuristic(pdf_emit_w * cos_in / (pdf_di_w * cos_out))
                   * path.template weight(pdf_rev_w, merge_weight);

    return 1.0f / (wc + 1.0f);
}

template <typename T>
inline float weight_lt(const MisHelper<T>& path, float merge_weight, float pdf_cam_a, float pdf_rev_w,
                    float cos_theta_surf, float d2, int num_light_paths) {

    const float wl = pow_heuristic(pdf_cam_a / num_light_paths)
                   * path.template weight(pdf_rev_w, merge_weight);

    return 1.0f / (wl + 1.0f);
}

#define MIS_HANDLER_FNS                                                                 \
    template<typename... Ts>                                                            \
    void init_camera(Ts... params)   { algo_.init_camera  (params..., MY_TPYE_ARGS); }  \
    template<typename... Ts>                                                            \
    void init_light(Ts... params)    { algo_.init_light   (params..., MY_TPYE_ARGS); }  \
    template<typename... Ts>                                                            \
    void update_hit(Ts... params)    { algo_.update_hit   (params..., MY_TPYE_ARGS); }  \
    template<typename... Ts>                                                            \
    void update_bounce(Ts... params) { algo_.update_bounce(params..., MY_TPYE_ARGS); }

/// Partial MIS evaluator for path tracing with next event estimation.
struct MisPT : public MisHelper<MisPT> {
    #define MY_TPYE_ARGS (UnidirPT*)nullptr, (ConnectLT*)nullptr, \
                         (DirectIllum*)nullptr, (Connect*)nullptr, (Merge*)nullptr

    template <typename... Ts>
    float weight_on_emitter(Ts... args) const { return algo_.finalize_on_emitter(args..., MY_TPYE_ARGS); }

    template <typename... Ts>
    float weight(Ts... args) const {
        return algo_.finalize(args..., MY_TPYE_ARGS);
    }

    MIS_HANDLER_FNS;

    #undef MY_TPYE_ARGS

private:
    Algorithm algo_;
};

struct MisLT : public MisHelper<MisLT> {
    #define MY_TPYE_ARGS (UnidirPT*)nullptr, (ConnectLT*)nullptr, \
                         (DirectIllum*)nullptr, (Connect*)nullptr, (Merge*)nullptr

    template <typename... Ts>
    float weight_on_emitter(Ts... args) const { return algo_.finalize_on_emitter(args..., MY_TPYE_ARGS); }

    template <typename... Ts>
    float weight(Ts... args) const {
        return algo_.finalize(args..., MY_TPYE_ARGS);
    }

    MIS_HANDLER_FNS;

    #undef MY_TPYE_ARGS

private:
    Algorithm algo_;
};

struct MisTWPT : public MisHelper<MisTWPT> {
    #define MY_TPYE_ARGS (UnidirPT*)nullptr, (ConnectLT*)nullptr, \
                         (DirectIllum*)nullptr, (Connect*)nullptr, (Merge*)nullptr

    template <typename... Ts>
    float weight_on_emitter(Ts... args) const { return algo_.finalize_on_emitter(args..., MY_TPYE_ARGS); }

    template <typename... Ts>
    float weight(Ts... args) const {
        return algo_.finalize(args..., MY_TPYE_ARGS);
    }

    MIS_HANDLER_FNS;

    #undef MY_TPYE_ARGS

private:
    Algorithm algo_;
};

struct MisBPT : public MisHelper<MisBPT> {
    #define MY_TPYE_ARGS (UnidirPT*)nullptr, (ConnectLT*)nullptr, \
                         (DirectIllum*)nullptr, (Connect*)nullptr, (Merge*)nullptr

    template <typename... Ts>
    float weight_on_emitter(Ts... args) const { return algo_.finalize_on_emitter(args..., MY_TPYE_ARGS); }

    template <typename... Ts>
    float weight(Ts... args) const {
        return algo_.finalize(args..., MY_TPYE_ARGS);
    }

    MIS_HANDLER_FNS;

    #undef MY_TPYE_ARGS

private:
    Algorithm algo_;
};

struct MisPPM : public MisHelper<MisPPM> {
    #define MY_TPYE_ARGS (UnidirPT*)nullptr, (ConnectLT*)nullptr, \
                         (DirectIllum*)nullptr, (Connect*)nullptr, (Merge*)nullptr

    template <typename... Ts>
    float weight_on_emitter(Ts... args) const { return algo_.finalize_on_emitter(args..., MY_TPYE_ARGS); }

    template <typename... Ts>
    float weight(Ts... args) const {
        return algo_.finalize(args..., MY_TPYE_ARGS);
    }

    MIS_HANDLER_FNS;

    #undef MY_TPYE_ARGS

private:
    Algorithm algo_;
};

struct MisVCM : public MisHelper<MisVCM> {
    #define MY_TPYE_ARGS (UnidirPT*)nullptr, (ConnectLT*)nullptr, \
                         (DirectIllum*)nullptr, (Connect*)nullptr, (Merge*)nullptr

    template <typename... Ts>
    float weight_on_emitter(Ts... args) const { return algo_.finalize_on_emitter(args..., MY_TPYE_ARGS); }

    template <typename... Ts>
    float weight(Ts... args) const {
        return algo_.finalize(args..., MY_TPYE_ARGS);
    }

    MIS_HANDLER_FNS;

    #undef MY_TPYE_ARGS

private:
    Algorithm algo_;
};

#undef MIS_HANDLER_FNS

} // namespace mis



///////////////////////////////////////////////////////////////////
// Old implementation (kept for testing purposes).
// Not all special cases are implemented (those being a pain to
// figure out was the main motivation driving the new
// implementation above).
///////////////////////////////////////////////////////////////////





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

struct PartialMIS {
    static void setup_iteration(float radius, float lp_count, int techniques) {
        PartialMIS::techniques = techniques;
        light_path_count = lp_count;
        pdf_merge = techniques & MIS_MERGE ? mis::pow_heuristic(pi * sqr(radius) * light_path_count) : 0.0f;
    }

    inline void init_camera(float pdf) {
        reversible = true; // camera can always be connected to

        partial_mis = 0.0f;
        last_pdf  = mis::pow_heuristic(light_path_count / pdf);
    }

    inline void init_light(float pdf_emit_w, float pdf_di_a, float pdf_lightpick, float cos_out, bool finite, bool delta) {
        reversible = finite; // lights that are infinitely far away cannot be connected to

        if (!(techniques & MIS_NEXTEVT_CAM)) pdf_di_a = 0.0f;

        last_pdf = mis::pow_heuristic(pdf_di_a / pdf_emit_w); // pdf_lightpick cancels out
        partial_mis = delta ? 0.0f : mis::pow_heuristic(cos_out / (pdf_emit_w * pdf_lightpick));
    }

    inline void update_hit(float cos_theta_o, float d2) {
        if (reversible)
            last_pdf *= mis::pow_heuristic(d2);

        // After the first hit, the path is always reversible
        reversible = true;

        last_pdf    *= 1.0f / mis::pow_heuristic(cos_theta_o);
        partial_mis *= 1.0f / mis::pow_heuristic(cos_theta_o);
    }

    inline void update_bounce(float pdf_dir_w, float pdf_rev_w, float cos_theta_i, bool specular) {
        if (specular) {
            last_pdf = 0.0f;
            partial_mis *= mis::pow_heuristic(cos_theta_i);
        } else {
            partial_mis = mis::pow_heuristic(cos_theta_i / pdf_dir_w) *
                          (partial_mis * mis::pow_heuristic(pdf_rev_w) + last_pdf + pdf_merge);

            last_pdf = mis::pow_heuristic(1.0f / pdf_dir_w);
        }
    }

    static float pdf_merge;
    static int light_path_count;
    static int techniques;

    float partial_mis; ///< Sum of all (n*p)^b computed so far
    float last_pdf; ///< 1/p of the last bounce, (partially) converted to area measure

    bool reversible;
};

inline float mis_weight_connect(PartialMIS cam, PartialMIS light,
                                float pdf_cam_w, float pdf_rev_cam_w, float pdf_light_w, float pdf_rev_light_w,
                                float cos_cam, float cos_light, float d2) {
    const float pdf_cam_a = pdf_cam_w * cos_light / d2;
    const float pdf_light_a = pdf_light_w * cos_cam / d2;

    const float mis_weight_light = mis::pow_heuristic(pdf_cam_a) * (PartialMIS::pdf_merge + light.last_pdf + light.partial_mis * mis::pow_heuristic(pdf_rev_light_w));
    const float mis_weight_camera = mis::pow_heuristic(pdf_light_a) * (PartialMIS::pdf_merge + cam.last_pdf + cam.partial_mis * mis::pow_heuristic(pdf_rev_cam_w));

    if (PartialMIS::techniques == MIS_CONNECT)
        return 1.0f;
    else
        return 1.0f / (mis_weight_camera + 1.0f + mis_weight_light);
}

inline float mis_weight_merge(PartialMIS cam, PartialMIS light, float pdf_dir_w, float pdf_rev_w) {
    float pdf_merge_inv = 1.0f / PartialMIS::pdf_merge;
    const float mis_weight_light  = pdf_merge_inv * (light.last_pdf + light.partial_mis * mis::pow_heuristic(pdf_dir_w));
    const float mis_weight_camera = pdf_merge_inv * (  cam.last_pdf + cam.partial_mis * mis::pow_heuristic(pdf_rev_w));

    return 1.0f / (mis_weight_light + 1.0f + mis_weight_camera);
}

inline float mis_weight_hit(PartialMIS cam, float pdf_direct_a, float pdf_emit_w, float pdf_lightpick, int path_len) {
    const float pdf_di = PartialMIS::techniques & MIS_NEXTEVT_CAM ? pdf_direct_a * pdf_lightpick : 0.0f;
    const float pdf_e  = PartialMIS::techniques & MIS_ADJOINTS ? pdf_emit_w * pdf_lightpick : 0.0f;

    const float mis_weight_camera = mis::pow_heuristic(pdf_di) * cam.last_pdf + mis::pow_heuristic(pdf_e) * cam.partial_mis;

    if (PartialMIS::techniques == MIS_HIT || path_len == 2)
        return 1.0f;
    else
        return 1.0f / (mis_weight_camera + 1.0f);
}

inline float mis_weight_cam_connect(PartialMIS light, float pdf_cam, float cos_theta_surf, float d2, float pdf_light) {
    if (!(PartialMIS::techniques & MIS_PT)) pdf_cam = 0.0f;

    const float mis_weight_light = mis::pow_heuristic(pdf_cam / PartialMIS::light_path_count)
                                 * (PartialMIS::pdf_merge
                                    + light.last_pdf
                                    + (PartialMIS::techniques & MIS_CONNECT ? light.partial_mis * mis::pow_heuristic(pdf_light) : 0.0f));

    if (PartialMIS::techniques == MIS_NEXTEVT_LIGHT)
        return 1.0f;
    else
        return 1.0f / (mis_weight_light + 1.0f);
}

inline float mis_weight_di(PartialMIS cam, float pdf_dir_w, float pdf_rev_w, float pdf_di_w, float pdf_emit_w, float pdf_lightpick_inv,
                           float cos_theta_i, float cos_theta_o, bool delta_light) {
    const float mis_weight_light  = !delta_light && (PartialMIS::techniques & MIS_HIT)
                                  ? mis::pow_heuristic(pdf_dir_w / pdf_di_w * pdf_lightpick_inv)
                                  : 0.0f;

    if (!(PartialMIS::techniques & MIS_ADJOINTS)) pdf_emit_w = 0.0f;

    const float mis_weight_camera = mis::pow_heuristic(pdf_emit_w * cos_theta_i / (pdf_di_w * cos_theta_o))
                                  * (PartialMIS::pdf_merge
                                    + (PartialMIS::techniques & MIS_HIT ? cam.last_pdf : 0.0f)
                                    + (PartialMIS::techniques & MIS_CONNECT ? cam.partial_mis * mis::pow_heuristic(pdf_rev_w) : 0.0f));

    if (PartialMIS::techniques == MIS_NEXTEVT_CAM)
        return 1.0f;
    else
        return 1.0f / (mis_weight_camera + 1.0f + mis_weight_light);
}

} // namespace imba

#endif // IMBA_DEFERRED_MIS_H
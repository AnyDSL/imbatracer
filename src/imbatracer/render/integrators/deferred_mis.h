#ifndef IMBA_DEFERRED_MIS_H
#define IMBA_DEFERRED_MIS_H

#include "imbatracer/core/common.h"

#include <cassert>

namespace imba {

namespace mis {

/// Returns the power heuristic value for the given pdf * samples
static inline float pow_heuristic(float p) {
    return p;
}

/// Computes the partial weight for merging.
/// This is a function for simplicity. The weight should be precomputed by the caller,
/// storing the same weight in multiple (= millions of) partials is not efficient.
inline float merge_accept_weight(int num_light_paths, float radius) {
    return pow_heuristic(pi * sqr(radius) * num_light_paths);
}

/// Computes the partial MIS weights for a subpath, using the specified techniques.
/// Each technique provides functions to update the MIS partial sum.
template <typename... Techs>
struct MisHelper {
    MisHelper() : last(0.0f), partial(0.0f), started_at_infinity(false) {}

    /// Computes the final partial weight for this subpath, given the missing reverse pdf.
    float weight(float pdf_rev_w, float merge_weight, int path_len, bool cam_path, int path_len_other) const {
        float res = partial * pdf_rev_w;
        return res + finalizeDispatch<Techs...>::forall(merge_weight, last, partial, path_len, cam_path, path_len_other);
    }

    /// Computes the final partial weight for this subpath, if it ends on an emitter
    float weight_on_emitter(float pdf_rev_w, float merge_weight, float pdf_di_a) const {
        float res = partial * pdf_rev_w;
        return res + finalize_on_emitterDispatch<Techs...>::forall(merge_weight, pdf_di_a, last, partial);
    }

    /// Initializes the MIS for camera paths.
    /// Calling this function multiple times results in undefined behaviour.
    /// \param pdf_cam_w The pdf of sampling the primary ray from the camera (solid angle)
    void init_camera(int num_light_paths, float pdf_cam_w) {
        init_cameraDispatch<Techs...>::forall(&last, &partial, num_light_paths, pdf_cam_w);
    }

    void init_light(float pdf_emit_w, float pdf_di_a, float pdf_lightpick, float cos_in_light, bool finite, bool delta) {
        // We need to keep track of lights that are at infinity (these must not be multiplied by sqared distance!)
        started_at_infinity = !finite;

        init_lightDispatch<Techs...>::forall(&last, &partial, pdf_emit_w, pdf_di_a, pdf_lightpick, cos_in_light, finite, delta);
    }

    /// Updates the MIS weights after intersecting the ray with the scene.
    /// Conversion between area/solid angle measures requires the cosine at this hit point,
    /// as well as the squared distance between it an the last one.
    /// \param cos_out  The cosine of the angle formed by the normal and the direction of the last ray (outgoing radiance / importance in PT / LT)
    /// \param d2       The squared distance between this vertex and the last one
    void update_hit(float cos_out, float d2) {
        if (!started_at_infinity) {
            last *= pow_heuristic(d2);
            started_at_infinity = false;
        }

        last *= 1.0f / pow_heuristic(cos_out);

        // The squared distance cancels out for the partials as they divide each pdf by its corresponding reverse.
        partial  *= 1.0f / pow_heuristic(cos_out);
    }

    /// Updates the MIS weights after bouncing (sampling a direction either from the BRDF or something else)
    /// \param cos_in   Cosine of the direction we are bouncing to (incoming radiance / importance in PT / LT)
    /// \param path_len The lenght of the path so far; number of vertices including the one on the emitter
    /// \param cam_path True if the path being traced started from the camera
    void update_bounce(float pdf_dir_w, float pdf_rev_w, float cos_in, bool specular, float merge_weight, int path_len, bool cam_path) {
        if (specular) {
            // No sensible technique can do anything with a delta distribution.
            // Just finish converting the partials.
            last = 0.0f;
            partial *= pow_heuristic(cos_in);
            return;
        }

        // Account for the (now known) reverse pdf in the partials.
        partial *= pow_heuristic(pdf_rev_w);

        // Allow all techniques to add new strategies occuring at this vertex / path length.
        update_bounceDispatch<Techs...>::forall(&last, &partial, pdf_dir_w, pdf_rev_w, cos_in, merge_weight, path_len, cam_path);

        // Divide by the technique used for sampling
        // Include the cosine for solid-angle -> area (converts the reverse PDF at the next vertex)
        partial *= pow_heuristic(cos_in / pdf_dir_w);

        // Store the pdf for this bounce. It is converted to area at the next hit and (maybe) used for this next vertex.
        last = pow_heuristic(1.0f / pdf_dir_w);
    }

private:
    float last, partial;
    bool started_at_infinity;

    // Generates a template that calls a static member with given name for all types.
    #define MAKE_DISPATCHER(NAME)                       \
    template <typename CurTech, typename... RTech>      \
    struct NAME##Dispatch {                             \
        template <typename... Ts>                       \
        static void forall(Ts... vals) {                \
            CurTech::NAME(vals...);                     \
            NAME##Dispatch<RTech...>::forall(vals...);  \
        }                                               \
    };                                                  \
    template <typename CurTech>                         \
    struct NAME##Dispatch<CurTech> {                    \
        template <typename... Ts>                       \
        static void forall(Ts... vals) {                \
            CurTech::NAME(vals...);                     \
        }                                               \
    }

    // Generates a template that calls a static member with given name for all types, with a float return value.
    #define MAKE_DISPATCHER_RET(NAME)                               \
    template <typename CurTech, typename... RTech>                  \
    struct NAME##Dispatch {                                         \
        template <typename... Ts>                                   \
        static float forall(Ts... vals) {                           \
            float r = CurTech::NAME(vals...);                       \
            return r + NAME##Dispatch<RTech...>::forall(vals...);   \
        }                                                           \
    };                                                              \
    template <typename CurTech>                                     \
    struct NAME##Dispatch<CurTech> {                                \
        template <typename... Ts>                                   \
        static float forall(Ts... vals) {                           \
            return CurTech::NAME(vals...);                          \
        }                                                           \
    }

    // Create the dispatchers for all update functions.
    MAKE_DISPATCHER(init_camera);
    MAKE_DISPATCHER(init_light);
    MAKE_DISPATCHER(update_bounce);
    MAKE_DISPATCHER_RET(finalize);
    MAKE_DISPATCHER_RET(finalize_on_emitter);
};

#define TECH_UPDATE_NOOP(NAME) template<typename... Ts> static void NAME(float* last_pdf, float* partial, Ts... params) {}
#define TECH_FINALIZE_LIGHT_NOOP static float finalize_on_emitter(float merge_weight, float pdf_di_a, float last_pdf, float partial) { return 0.0f; }
#define TECH_FINALIZE_SURF_NOOP  static float finalize(float merge_weight, float last_pdf, float partial, int path_len, bool cam_path, int path_len_other) { return 0.0f; }

struct DirectIllum {
    TECH_UPDATE_NOOP(init_camera);

    static void init_light(float* last_pdf, float* partial,
                           float pdf_emit_w, float pdf_di_a, float pdf_lightpick, float cos_in_light, bool finite, bool delta) {
        *last_pdf += pow_heuristic(pdf_di_a / pdf_emit_w); // pdf_lightpick cancels out
    }

    static void update_bounce(float* last_pdf, float* partial, float pdf_dir_w, float pdf_rev_w, float cos_in,
                              float merge_weight, int path_len, bool cam_path) {
        if (path_len == 2 && !cam_path) {
            // At the first vertex along a light path, we have can account for DI weight now converted to area measure
            *partial += *last_pdf;
        }
    }

    static float finalize_on_emitter(float merge_weight, float pdf_di_a, float last_pdf, float partial) {
        // There was no emission from the light source -> weight for DI not yet accounted for
        return last_pdf * pdf_di_a;
    }

    static float finalize(float merge_weight, float last_pdf, float partial, int path_len, bool cam_path, int path_len_other) {
        if (path_len == 2 && !cam_path)
            return last_pdf; // Include weight for direct illumination if it is not yet part of partial
        else
            return 0.0f;
    }
};

struct UnidirPT {
    TECH_UPDATE_NOOP(init_camera);
    TECH_UPDATE_NOOP(update_bounce); // Hitting a light source cannot happen at a non-light vertex

    static void init_light(float* last_pdf, float* partial,
                           float pdf_emit_w, float pdf_di_a, float pdf_lightpick, float cos_in_light, bool finite, bool delta) {
        if(!delta)
            *partial += pow_heuristic(cos_in_light / (pdf_emit_w * pdf_lightpick));
    }

    TECH_FINALIZE_SURF_NOOP;
    TECH_FINALIZE_LIGHT_NOOP;
};

struct ConnectLT {
    TECH_UPDATE_NOOP(init_light);

    static void init_camera(float* last_pdf, float* partial, int num_light_paths, float pdf_cam_w) {
        *last_pdf += num_light_paths / pdf_cam_w;
    }

    static void update_bounce(float* last_pdf, float* partial, float pdf_dir_w, float pdf_rev_w, float cos_in,
                              float merge_weight, int path_len, bool cam_path) {
        if (path_len == 2 && cam_path) {
            // At the first vertex along a camera path, we have the pdf for connecting to the camera now converted to area measure.
            *partial += *last_pdf;
        }
    }

    static float finalize(float merge_weight, float last_pdf, float partial, int path_len, bool cam_path, int path_len_other) {
        if (path_len == 2 && cam_path)
            return last_pdf; // Include weight for light tracing if it is not yet part of partial
        else
            return 0.0f;
    }

    TECH_FINALIZE_LIGHT_NOOP;
};

struct Connect {
    TECH_UPDATE_NOOP(init_camera);
    TECH_UPDATE_NOOP(init_light); // No connections to light source vertices

    static void update_bounce(float* last_pdf, float* partial, float pdf_dir_w, float pdf_rev_w, float cos_in,
                              float merge_weight, int path_len, bool cam_path) {
        // Assumes that the pdf for a connection is 1
        // The partial weight for a connection at some vertex is 1/p, the last_pdf
        if (path_len > 2) // No connections to the light / camera!
            *partial += *last_pdf;
    }

    TECH_FINALIZE_LIGHT_NOOP; // No connections to vertices on the light

    static float finalize(float merge_weight, float last_pdf, float partial, int path_len, bool cam_path, int path_len_other) {
        // Include weight for connection instead of the last bounce
        if (path_len > 2)
            return last_pdf;
        else
            return 0.0f; // No connections to vertices on the light
    }
};

struct Merge {
    TECH_UPDATE_NOOP(init_camera);
    TECH_UPDATE_NOOP(init_light); // No merging on the light source

    static void update_bounce(float* last_pdf, float* partial, float pdf_dir_w, float pdf_rev_w, float cos_in,
                              float merge_weight, int path_len, bool cam_path) {
        // A merge is possible at every vertex on a path
       if (path_len > 2 || cam_path)
            *partial += merge_weight;
    }

    static float finalize(float merge_weight, float last_pdf, float partial, int path_len, bool cam_path, int path_len_other) {
        // Include weight for merging at the last bounce
        return merge_weight;
    }

    TECH_FINALIZE_LIGHT_NOOP;
};

#undef TECH_UPDATE_NOOP
#undef TECH_FINALIZE_LIGHT_NOOP
#undef TECH_FINALIZE_SURF_NOOP

template <typename T>
inline float weight_connect(const T& cam, const T& light, float merge_weight,
                            float pdf_cam_w, float pdf_rev_cam_w, float pdf_light_w, float pdf_rev_light_w,
                            float cos_cam, float cos_light, float d2, int path_len_c, int path_len_l) {
    // Convert to area measure
    const float pdf_cam_a   = pdf_cam_w   * cos_light / d2;
    const float pdf_light_a = pdf_light_w * cos_cam   / d2;

    // Add missing unidirectional pdfs
    const float wc = pdf_light_a *   cam.template weight(pdf_rev_cam_w  , merge_weight, path_len_c, true , path_len_l);
    const float wl = pdf_cam_a   * light.template weight(pdf_rev_light_w, merge_weight, path_len_l, false, path_len_c);

    return 1.0f / (wc + 1.0f + wl);
}

template <typename T>
inline float weight_merge(const T& cam, const T& light, float merge_weight,
                          float pdf_dir_w, float pdf_rev_w, int path_len_c, int path_len_l) {
    float merge_weight_inv = 1.0f / merge_weight; // TODO could also be precomputed

    // Uses the fact that the pdf for merging is the pdf for connecting times the merge acceptance probability
    const float wc = merge_weight_inv *   cam.template weight(pdf_rev_w, merge_weight, path_len_c, true , path_len_l);
    const float wl = merge_weight_inv * light.template weight(pdf_dir_w, merge_weight, path_len_l, false, path_len_c);

    return 1.0f / (wc + wl - 1.0f); // The 1 in the sum is already included twice, once in each subpath partial sum
}

template <typename T>
inline float weight_upt(const T& path, float merge_weight, float pdf_direct_a, float pdf_emit_w, float pdf_lightpick, int path_len) {
    if (path_len == 2) return 1.0f; // Assumes that the sensor cannot be hit! TODO: this should be done in a more generic way

    const float pdf_di = pdf_direct_a * pdf_lightpick;
    const float pdf_e  = pdf_emit_w   * pdf_lightpick;

    const float w = path.template weight_on_emitter(pdf_e, merge_weight, pdf_di);

    return 1.0f / (w + 1.0f);
}

template <typename T>
inline float weight_di(const T& path, float merge_weight,
                       float pdf_dir_w, float pdf_rev_w, float pdf_di_w, float pdf_emit_w, float pdf_lightpick_inv,
                       float cos_in, float cos_out, bool delta_light, int path_len) {
    // TODO: this is actually accounting for the weight from hitting the light source vs DI
    // -> maybe include this in the UnidirPT technique somehow?
    // Ignoring hitting the light source does not make much sense though, hence this is probably always the same
    const float wl = !delta_light ? pow_heuristic(pdf_dir_w / pdf_di_w * pdf_lightpick_inv) : 0.0f;

    const float wc = pow_heuristic(pdf_emit_w * cos_in / (pdf_di_w * cos_out))
                   * path.template weight(pdf_rev_w, merge_weight, path_len, true, 1);

    return 1.0f / (wc + 1.0f + wl);
}

template <typename T>
inline float weight_lt(const T& path, float merge_weight, float pdf_cam_a, float pdf_rev_w, int num_light_paths, int path_len) {
    const float wl = pow_heuristic(pdf_cam_a / num_light_paths)
                   * path.template weight(pdf_rev_w, merge_weight, path_len, false, 1);

    return 1.0f / (wl + 1.0f);
}

using MisPT   = MisHelper<UnidirPT, DirectIllum>;
using MisLT   = MisHelper<ConnectLT>;
using MisTWPT = MisHelper<UnidirPT, DirectIllum, ConnectLT>;
using MisBPT  = MisHelper<UnidirPT, DirectIllum, ConnectLT, Connect>;
using MisSPPM = MisHelper<Merge, UnidirPT, DirectIllum>;
using MisVCM  = MisHelper<UnidirPT, DirectIllum, ConnectLT, Connect, Merge>;

#undef MIS_HANDLER_FNS

} // namespace mis

} // namespace imba

#endif // IMBA_DEFERRED_MIS_H
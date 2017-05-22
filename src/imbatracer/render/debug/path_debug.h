#ifndef IMBA_PATH_DEBUG_H
#define IMBA_PATH_DEBUG_H

#include "imbatracer/core/float3.h"
#include "imbatracer/render/integrators/light_vertices.h"

#include <fstream>
#include <string>

namespace imba {

/// Stores sampling informations on the light or camera paths for debugging.
template <typename StateType, bool light_paths, bool enabled>
class PathDebugger {
public:
    /// Adds a new vertex along a path. Thread-safe between different paths.
    void add_vertex(const float3& pos, const StateType& state) {
        if (!enabled) return;

        int path_id = light_paths ? state.light_id + num_lights_ * state.ray_id
                                  : state.pixel_id + num_pixels_ * state.sample_id;
        assert(path_id < paths_.size());
        paths_[path_id].push_back(DebugVertex(pos, state.throughput));
    }

    /// Links the LightPathVertex that was stored for PM / VPLs
    /// Thread-safe in-between paths
    void store_vertex(LightPathVertex* vert, const StateType& state) {
        if (!enabled) return;

        int path_id = light_paths ? (state.light_id + num_lights_ * state.ray_id)
                                  : (state.pixel_id + num_pixels_ * state.sample_id);
        assert(path_id < paths_.size());
        int vert_id = state.path_length;
        assert(vert_id < paths_[path_id].size());
        paths_[path_id][vert_id].vert = vert;
    }

    /// Starts a new frame, resetting all paths.
    void start_frame(int frame_id, int num_paths, int num_lights) {
        if (!enabled) return;

        if (light_paths)
            paths_.resize(num_paths * num_lights);
        else
            paths_.resize(num_paths);
        num_lights_ = num_lights;
    }

    /// Ends the current frame and writes the path data to a file.
    void end_frame(int frame_id) {
        if (!enabled) return;

        write_file(frame_id);
        paths_.clear();
    }

private:
    struct DebugVertex {
        float3 pos;
        float3 contrib;
        LightPathVertex* vert;

        DebugVertex() : vert(nullptr) {}

        DebugVertex(const float3& p, const float3& c)
            : pos(p), contrib(c), vert(nullptr)
        {}
    };


    std::vector<std::vector<DebugVertex>> paths_;

    union {
        int num_lights_;
        int num_pixels_;
    };

    /// Writes the path data to a file in a simple binary format (no. paths / vertices followed by data)
    void write_file(int frame_id) {
        std::string filename = "path_debug_" + std::to_string(frame_id) + (light_paths ? ".path" : "cam.path");
        std::ofstream out(filename, std::ios_base::binary);

        auto write_sz = [&out](uint32_t sz) {out.write(reinterpret_cast<const char*>(&sz), sizeof(sz));};
        auto write_f3 = [&out](const float3& v) {out.write(reinterpret_cast<const char*>(&v), sizeof(v));};
        auto write_a3 = [&out](const atomic_rgb& v) {
            for (int i = 0; i < 3; ++i) {
                float c = v[i];
                out.write(reinterpret_cast<const char*>(&c), sizeof(c));
            }
        };
        auto write_sentinel = [&out]() {
            const float sentinel = -1.0f;
            for (int i = 0; i < 3; ++i)
                out.write(reinterpret_cast<const char*>(&sentinel), sizeof(sentinel));
        };

        write_sz(paths_.size());
        for (auto& p : paths_) {
            write_sz(p.size());
            for (auto& v : p) {
                write_f3(v.pos);
                write_f3(v.contrib);
                if (v.vert) {
                    write_a3(v.vert->total_contrib_pm);
                    write_a3(v.vert->total_contrib_vc);
                } else {
                    // Distinguish between photons / VPLs with zero contribution, and vertices that were not used to generate a photon
                    // or VPL because they are on a light source or on a specular surface.
                    write_sentinel();
                    write_sentinel();
                }
            }
        }
    }
};

/// Loads the paths and vertices stored by a \see{PathDebugger} for visualization.
class PathLoader {
public:
    bool read_file(int frame_id, bool light_paths) {
        std::string filename = "path_debug_" + std::to_string(frame_id) + (light_paths ? ".path" : "cam.path");
        std::ifstream in(filename, std::ios_base::binary);

        if (!in) return false;

        max_lum_pm_ = max_lum_vc_ = 0.0f;
        paths_.clear();
        photons_.clear();

        auto read_sz = [&in]() -> uint32_t {
            uint32_t sz;
            in.read(reinterpret_cast<char*>(&sz), sizeof(sz));
            return sz;
        };

        auto read_f3 = [&in]() -> float3 {
            float3 v;
            in.read(reinterpret_cast<char*>(&v), sizeof(v));
            return v;
        };

        uint32_t n_paths = read_sz();
        for (int i = 0; i < n_paths; ++i) {
            uint32_t n_verts = read_sz();
            paths_.emplace_back(n_verts);
            for (int k = 0; k < n_verts; ++k) {
                auto& v = paths_.back()[k];
                v.pos = read_f3();
                v.power = read_f3();
                v.contrib_pm = read_f3();
                v.contrib_vc = read_f3();

                v.used = v.contrib_pm.x != -1.0f || v.contrib_pm.y != -1.0f || v.contrib_pm.z != -1.0f ||
                         v.contrib_vc.x != -1.0f || v.contrib_vc.y != -1.0f || v.contrib_vc.z != -1.0f;

                if (v.used && k > 0) { // do not consider photons on the light source!
                    photons_.emplace_back(v);
                    max_lum_pm_ = std::max(max_lum_pm_, luminance(v.contrib_pm));
                    max_lum_vc_ = std::max(max_lum_vc_, luminance(v.contrib_vc));
                } else if (!light_paths) {
                    v.contrib_pm = rgb(0.0f);
                    v.contrib_vc = rgb(0.0f);
                    photons_.emplace_back(v);
                }
            }
        }

        return true;
    }

    struct DebugVertex {
        float3 pos;
        rgb power;
        bool used;
        rgb contrib_vc;
        rgb contrib_pm;
    };

    struct DebugPhoton {
        float3 pos;
        rgb power;
        rgb contrib_vc;
        rgb contrib_pm;

        DebugPhoton() {}

        DebugPhoton(const DebugVertex& v)
            : pos(v.pos)
            , power(v.power)
            , contrib_vc(v.contrib_vc)
            , contrib_pm(v.contrib_pm)
        {}

        float3& position() { return pos; }
    };

    using PhotonIter = std::vector<DebugPhoton>::iterator;

    PhotonIter photons_begin() { return photons_.begin(); }
    PhotonIter photons_end() { return photons_.end(); }

    int num_paths() const { return paths_.size(); }

    float max_luminance_pm() const { return max_lum_pm_; }
    float max_luminance_vc() const { return max_lum_vc_; }

private:
    std::vector<std::vector<DebugVertex>> paths_;
    std::vector<DebugPhoton> photons_;
    float max_lum_pm_;
    float max_lum_vc_;
};

} // namespace imba

#endif // IMBA_PATH_DEBUG_H

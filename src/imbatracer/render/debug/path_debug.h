#ifndef IMBA_PATH_DEBUG_H
#define IMBA_PATH_DEBUG_H

#include "imbatracer/core/float3.h"

#include <fstream>
#include <string>

namespace imba {

/// Stores sampling informations on the light or camera paths for debugging.
template <typename StateType, bool enabled>
class PathDebugger {
public:
    /// Adds a new vertex along a path. Thread-safe between different paths.
    void add_vertex(const float3& pos, const float3& dir, const StateType& state) {
        if (!enabled) return;

        int path_id = state.sample_id * num_pixels_ + state.ray_id;
        paths_[path_id].emplace_back(pos, dir, state.throughput);
    }

    /// Links the LightPathVertex that was stored for PM / VPLs
    /// Thread-safe in-between paths
    void store_vertex(LightPathVertex* vert, const StateType& state) {
        int path_id = state.sample_id * num_pixels_ + state.ray_id;
        int vert_id = state.path_length;
        paths_[path_id][vert_id].vert = vert;
    }

    /// Starts a new frame, resetting all paths.
    void start_frame(int frame_id, int num_pixels, int num_samples) {
        if (!enabled) return;

        paths_.resize(num_pixels * num_samples);
        num_pixels_  = num_pixels;
        num_samples_ = num_samples;
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
        float3 dir;
        float3 contrib;
        LightPathVertex* vert;

        DebugVertex(const float3& p, const float3& d, const float3& c)
            : pos(p), dir(d), contrib(c), vert(nullptr)
        {}
    };

    std::vector<std::vector<DebugVertex>> paths_;
    int num_pixels_;
    int num_samples_;

    /// Writes the path data to a file in a simple binary format (no. paths / vertices followed by data)
    void write_file(int frame_id) {
        std::string filename = "path_debug_" + std::to_string(frame_id) + ".path";
        std::ofstream out(filename, std::ios_base::binary);
        out << (uint32_t)paths_.size();
        for (auto& p : paths_) {
            out << (uint32_t)p.size();
            for (auto& v : p) {
                out << v.pos.x << v.pos.y << v.pos.z;
                out << v.dir.x << v.dir.y << v.dir.z;
                out << v.contrib.x << v.contrib.y << v.contrib.z;
                out << v.vert->total_contrib_pm[0] << v.vert->total_contrib_pm[1] << v.vert->total_contrib_pm[2];
                out << v.vert->total_contrib_vc[0] << v.vert->total_contrib_vc[1] << v.vert->total_contrib_vc[2];
            }
        }
    }
};

} // namespace imba

#endif // IMBA_PATH_DEBUG_H

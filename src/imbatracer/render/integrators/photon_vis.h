#ifndef IMBA_PHOTON_VIS_H
#define IMBA_PHOTON_VIS_H

#include "imbatracer/render/integrators/deferred_vcm.h"
#include "imbatracer/render/integrators/contrib_grid.h"
#include "imbatracer/render/scheduling/tile_scheduler.h"
#include "imbatracer/render/ray_gen/tile_gen.h"
#include "imbatracer/render/ray_gen/ray_gen.h"

#include "imbatracer/frontend/cmd_line.h"

namespace imba {

struct PhotonVisState : RayState {
    rgb throughput;
    int wnd;
};

/// Visualizes a photon / VPL distribution that was computed and stored by
/// a VCM, BPT, or PPM iteration
class PhotonVis : public Integrator {
    static constexpr int RES = 40;

public:
    PhotonVis(Scene& scene, PerspectiveCamera& cam, const UserSettings& settings)
        : Integrator(scene, cam)
        , settings_(settings)
        , ray_gen_(settings.width, settings.height, settings.concurrent_spp, settings.tile_size)
        , scheduler_(ray_gen_, scene, 1, settings.thread_count,
                     settings.tile_size * settings.tile_size * settings.concurrent_spp,
                     settings.traversal_platform == UserSettings::gpu)
        , depth_(0)
        , eye_light_(false)
    {
    }

    virtual void render(AtomicImage& out) override;

    virtual void reset() override {}

    virtual void preprocess() override {
        Integrator::preprocess();
        load("camera_paths.path", "light_paths.path");
        build();
    }

    void load(const std::string& file_cam, const std::string& file_light) {
        if (!(num_cam_paths_ = read_vertices<DebugVertex>(file_cam, [this](const DebugVertex& v) {
            cam_vertices_.push_back(v);
        }))) {
            std::cout << "Camera path file not found." << std::endl;
            return;
        }

        if (!(num_light_paths_ = read_vertices<DebugVertex>(file_light, [this](const DebugVertex& v) {
            light_vertices_.push_back(v);
        }))) {
            std::cout << "Light path file not found." << std::endl;
            return;
        }

        radius_ = pixel_size() * settings_.radius_factor * 0.25f;
    }

    void build() {
        cam_grid_  .build(cam_vertices_  .begin(), cam_vertices_  .end(), radius_, [this](auto& v){ return !depth_ || v.path_len - 1 <= depth_; });
        light_grid_.build(light_vertices_.begin(), light_vertices_.end(), radius_, [this](auto& v){ return !depth_ || v.path_len - 1 <= depth_; });
    }

    virtual bool key_press(int32_t k) override {
        if (k >= '0' && k <= '9') {
            depth_ = k - '0';

            // // convert from 1,..,9,0 to 0,1,...,9,10
            // if (depth_ == 0) depth_ = 10;
            // depth_--;

            build();

            return true;
        } else if (k == 'e' || k == 'r') {
            eye_light_ = !eye_light_;
        }

        return false;
    }

private:
    UserSettings settings_;

    int depth_; ///< Path length to be visualized. 0 == all
    float radius_;
    bool eye_light_;

    std::vector<DebugVertex> cam_vertices_;
    std::vector<DebugVertex> light_vertices_;
    int num_cam_paths_;
    int num_light_paths_;

    struct VertexHandle {
        VertexHandle() : vert(nullptr) {}
        VertexHandle(const DebugVertex& v) { vert = &v; }
        VertexHandle(const DebugVertex* v) { vert = v; }
        VertexHandle& operator= (const DebugVertex& v) { vert = &v; return *this; }
        VertexHandle& operator= (const VertexHandle* v) { vert = v->vert; return *this; }

        const float3& position() const { return vert->isect.pos; }

        const DebugVertex* vert;
    };

    HashGrid<VertexHandle> cam_grid_;
    HashGrid<VertexHandle> light_grid_;

    DefaultTileGen<PhotonVisState> ray_gen_;
    TileScheduler<PhotonVisState, ShadowState> scheduler_;

    void process_camera_rays(RayQueue<PhotonVisState>& prim_rays, RayQueue<ShadowState>& shadow_rays, AtomicImage& out);
};

} // namespace imba

#endif // IMBA_PHOTON_VIS_H
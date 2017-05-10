#ifndef IMBA_PHOTON_VIS_H
#define IMBA_PHOTON_VIS_H

#include "imbatracer/render/integrators/integrator.h"
#include "imbatracer/render/debug/path_debug.h"
#include "imbatracer/render/scheduling/tile_scheduler.h"
#include "imbatracer/render/ray_gen/tile_gen.h"
#include "imbatracer/render/ray_gen/ray_gen.h"

#include "imbatracer/frontend/cmd_line.h"

namespace imba {

struct PhotonVisState : RayState {
    rgb throughput;
};

/// Visualizes a photon / VPL distribution that was computed and stored by
/// a VCM, BPT, or PPM iteration
class PhotonVis : public Integrator {
public:
    PhotonVis(Scene& scene, PerspectiveCamera& cam, const UserSettings& settings)
        : Integrator(scene, cam)
        , settings_(settings)
        , ray_gen_(settings.width, settings.height, settings.concurrent_spp, settings.tile_size)
        , scheduler_(ray_gen_, scene, 1, settings.thread_count,
                     settings.tile_size * settings.tile_size * settings.concurrent_spp,
                     settings.traversal_platform == UserSettings::gpu)
    {
    }

    virtual void render(AtomicImage& out) override;

    virtual void reset() override {}

    virtual void preprocess() override {
        Integrator::preprocess();
        pl_.read_file(0);
        radius_ = pixel_size() * settings_.radius_factor;
        accel_.build(pl_.photons_begin(), pl_.photons_end(), radius_);
    }

private:
    UserSettings settings_;
    PathLoader pl_;
    HashGrid<PathLoader::PhotonIter, PathLoader::DebugPhoton> accel_;

    DefaultTileGen<PhotonVisState> ray_gen_;
    TileScheduler<PhotonVisState, ShadowState> scheduler_;

    float radius_;

    void process_camera_rays(RayQueue<PhotonVisState>& prim_rays, RayQueue<ShadowState>& shadow_rays, AtomicImage& out);
};

} // namespace imba

#endif // IMBA_PHOTON_VIS_H
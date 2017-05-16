#ifndef IMBA_PHOTON_VIS_H
#define IMBA_PHOTON_VIS_H

#include "imbatracer/render/integrators/integrator.h"
#include "imbatracer/render/integrators/contrib_grid.h"
#include "imbatracer/render/debug/path_debug.h"
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

        const int res = 10;

        pl_light_.read_file(0, true);
        grid_light_.build(pl_light_.photons_begin(), pl_light_.photons_end(),
                          [](const PathLoader::DebugPhoton& p, float c[]){
                              c[0] = luminance(p.contrib_pm);
                              c[1] = luminance(p.contrib_vc);
                              c[2] = 1.0f;
                          },
                          [](const PathLoader::DebugPhoton& p){ return p.pos; },
                          res, res, res, scene_.bounds());

        pl_cam_.read_file(0, false);
        grid_cam_.build(pl_cam_.photons_begin(), pl_cam_.photons_end(),
                        [](const PathLoader::DebugPhoton& p, float c[]){
                            c[0] = luminance(p.power);
                            c[1] = 1.0f;
                        },
                        [](const PathLoader::DebugPhoton& p){ return p.pos; },
                        res, res, res, scene_.bounds());
    }

private:
    UserSettings settings_;
    PathLoader pl_light_;
    PathLoader pl_cam_;

    ContribGrid<PathLoader::DebugPhoton, 3> grid_light_;
    ContribGrid<PathLoader::DebugPhoton, 2> grid_cam_;

    DefaultTileGen<PhotonVisState> ray_gen_;
    TileScheduler<PhotonVisState, ShadowState> scheduler_;

    void process_camera_rays(RayQueue<PhotonVisState>& prim_rays, RayQueue<ShadowState>& shadow_rays, AtomicImage& out);
};

} // namespace imba

#endif // IMBA_PHOTON_VIS_H
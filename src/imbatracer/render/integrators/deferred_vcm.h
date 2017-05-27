#ifndef IMBA_DEFERRED_VCM_H
#define IMBA_DEFERRED_VCM_H

#include "imbatracer/render/scheduling/deferred_scheduler.h"

#include "imbatracer/render/integrators/integrator.h"

#include "imbatracer/render/ray_gen/tile_gen.h"
#include "imbatracer/render/ray_gen/ray_gen.h"

#include "imbatracer/frontend/cmd_line.h"

namespace imba {

class DeferredVCM : public Integrator {
    struct State : public RayState {

    };

    struct ShadowState : public RayState {

    };

    class ConnectTileGen : public TileGen<ShadowState> {
    public:
        using typename TileGen<ShadowState>::TilePtr;

        TilePtr next_tile(uint8_t*) override final {}

        size_t sizeof_ray_gen() const override final {}

        void start_frame() override final {}
    };

    class NextEventTileGen : public TileGen<ShadowState> {
    public:
        using typename TileGen<ShadowState>::TilePtr;

        TilePtr next_tile(uint8_t*) override final {}

        size_t sizeof_ray_gen() const override final {}

        void start_frame() override final {}
    };

    class CamConnectTileGen : public TileGen<ShadowState> {
    public:
        using typename TileGen<ShadowState>::TilePtr;

        TilePtr next_tile(uint8_t*) override final {}

        size_t sizeof_ray_gen() const override final {}

        void start_frame() override final {}
    };

public:
    DeferredVCM(Scene& scene, PerspectiveCamera& cam, const UserSettings& settings)
        : Integrator(scene, cam)
        , settings_(settings)
        , cur_iteration_(0)
        , light_tile_gen_(scene.light_count(), settings.light_path_count, settings.tile_size * settings.tile_size)
        , camera_tile_gen_(settings.width, settings.height, settings.concurrent_spp, settings.tile_size)
        , scheduler_(&scene_, settings.thread_count, settings.q_size,
                     settings.traversal_platform == UserSettings::gpu,
                     std::max(camera_tile_gen_.sizeof_ray_gen(), light_tile_gen_.sizeof_ray_gen()))
        //, shadow_scheduler_() TODO
    {
    }

    virtual void render(AtomicImage& out) override;

    virtual void reset() override {
        pm_radius_ = base_radius_;
        cur_iteration_ = 0;
    }

    virtual void preprocess() override {
        Integrator::preprocess();
        base_radius_ = pixel_size() * settings_.radius_factor;
    }

private:
    UserSettings settings_;

    int cur_iteration_;
    float pm_radius_;
    float base_radius_;

    UniformLightTileGen<State> light_tile_gen_;
    DefaultTileGen<State> camera_tile_gen_;

    DeferredScheduler<State> scheduler_;
    //DeferredScheduler<ShadowState> shadow_scheduler_;
};

} // namespace imba

#endif // IMBA_DEFERRED_VCM_H
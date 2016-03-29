#ifndef RAY_SCHEDULER
#define RAY_SCHEDULER

#include "ray_gen.h"

#include <tbb/tbb.h>
#include <tbb/task_group.h>

#include <array>
#include <atomic>
#include <cassert>
#include <list>

namespace imba {

/// Base class for all types of schedulers.
template<typename Derived, typename StateType>
class RaySchedulerBase {
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;
public:
    RaySchedulerBase(RayGen<StateType>& ray_gen, Scene& scene)
        : ray_gen_(ray_gen)
        , scene_(scene)
    {}

    template<typename Obj>
    void run_iteration(AtomicImage& out, Obj* integrator,
                       void (Obj::*process_shadow_rays)(RayQueue<StateType>&, AtomicImage&),
                       void (Obj::*process_primary_rays)(RayQueue<StateType>&, RayQueue<StateType>&, RayQueue<StateType>&, AtomicImage&),
                       SamplePixelFn sample_fn) {
        static_cast<Derived*>(this)->derived_run_iteration(out, integrator, process_shadow_rays, process_primary_rays, sample_fn);
    }

protected:
    RayGen<StateType>& ray_gen_;
    Scene& scene_;
};

}

#include "queue_scheduler_impl.h"
#include "tile_scheduler_impl.h"

#endif

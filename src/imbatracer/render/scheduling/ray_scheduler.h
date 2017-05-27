#ifndef IMBA_RAY_SCHEDULER_H
#define IMBA_RAY_SCHEDULER_H

#include "imbatracer/render/ray_gen/ray_gen.h"

#include <array>
#include <atomic>
#include <cassert>
#include <list>

namespace imba {

/// Base class for all types of schedulers.
template <typename StateType, typename ShadowStateType>
class RayScheduler {
protected:
    using SampleFn = typename RayGen<StateType>::SampleFn;
    typedef std::function<void (RayQueue<StateType>&, RayQueue<ShadowStateType>&, AtomicImage&)> ProcessPrimaryFn;
    typedef std::function<void (RayQueue<ShadowStateType>&, AtomicImage&)> ProcessShadowFn;

public:
    RayScheduler(Scene& scene, bool gpu_traversal)
        : scene_(scene)
        , gpu_traversal(gpu_traversal)
    {}

    virtual ~RayScheduler() {}

    virtual void run_iteration(AtomicImage& out,
                               ProcessShadowFn process_shadow_rays,
                               ProcessPrimaryFn process_primary_rays,
                               SampleFn sample_fn) = 0;

    const bool gpu_traversal;

protected:
    Scene& scene_;
};

} // namespace imba

#endif // IMBA_RAY_SCHEDULER_H


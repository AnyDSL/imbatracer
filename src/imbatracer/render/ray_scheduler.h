#ifndef IMBA_RAY_SCHEDULER_H
#define IMBA_RAY_SCHEDULER_H

#include "ray_gen.h"

#include <array>
#include <atomic>
#include <cassert>
#include <list>

namespace imba {

/// Base class for all types of schedulers.
template <typename StateType>
class RayScheduler {
protected:
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;
    typedef std::function<void (RayQueue<StateType>&, RayQueue<StateType>&, AtomicImage&)> ProcessPrimaryFn;
    typedef std::function<void (RayQueue<StateType>&, AtomicImage&)> ProcessShadowFn;

public:
    RayScheduler(Scene& scene)
        : scene_(scene)
    {}

    virtual ~RayScheduler() {}

    virtual void run_iteration(AtomicImage& out,
                       ProcessShadowFn process_shadow_rays, ProcessPrimaryFn process_primary_rays,
                       SamplePixelFn sample_fn) = 0;

protected:
    Scene& scene_;
};

} // namespace imba

#endif // IMBA_RAY_SCHEDULER_H


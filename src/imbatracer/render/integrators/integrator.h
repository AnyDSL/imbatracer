#ifndef IMBA_INTEGRATOR_H
#define IMBA_INTEGRATOR_H

#include "imbatracer/render/scheduling/ray_queue.h"
#include "imbatracer/render/ray_gen/camera.h"
#include "imbatracer/render/light.h"
#include "imbatracer/render/random.h"
#include "imbatracer/render/scene.h"

#include "imbatracer/core/mesh.h"
#include "imbatracer/core/image.h"
#include "imbatracer/core/rgb.h"

#include <functional>

namespace imba {

/// Base class for all integrators.
class Integrator {
public:
    Integrator(const Scene& scene, const PerspectiveCamera& cam)
        : scene_(scene), cam_(cam)
    {}

    virtual ~Integrator() {}

    /// Renders a frame, using the resolution and sample count specified in the camera.
    virtual void render(AtomicImage& out) = 0;

    /// Called whenever the camera view is updated.
    virtual void reset() {}

    /// Called once per scene at the beginning, before the other methods.
    virtual void preprocess() { estimate_pixel_size(); }

    /// Estimate of the average distance between hit points of rays from the same pixel.
    /// The value is computed during the preprocessing phase.
    /// The result of calling this function before preprocess() is undefined.
    float pixel_size() const { return pixel_size_; }

    /// Allows integrators to react on user input (e.g. for debugging)
    /// \returns true if the image should be reset.
    virtual bool key_press(int32_t k) { return false; }

protected:
    const Scene& scene_;
    const PerspectiveCamera& cam_;

    inline static void add_contribution(AtomicImage& out, int pixel_id, const rgb& contrib) {
        out.pixels()[pixel_id].apply<std::plus<float> >(contrib);
    }

    inline void process_shadow_rays(RayQueue<ShadowState>& ray_in, AtomicImage& out) {
        ShadowState* states = ray_in.states();
        Hit* hits = ray_in.hits();

        tbb::parallel_for(tbb::blocked_range<int>(0, ray_in.size()),
            [&] (const tbb::blocked_range<int>& range)
        {
            for (auto i = range.begin(); i != range.end(); ++i) {
                if (hits[i].tri_id < 0) {
                    // Nothing was hit, the light source is visible.
                    add_contribution(out, states[i].pixel_id, states[i].throughput);
                }
            }
        });
    }

private:
    float pixel_size_;

    void estimate_pixel_size();
};

template<typename StateType>
void terminate_path(StateType& state) {
    state.pixel_id = -1;
}

} // namespace imba

#endif // IMBA_INTEGRATOR_H

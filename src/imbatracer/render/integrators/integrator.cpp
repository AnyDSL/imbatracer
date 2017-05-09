#include "imbatracer/render/integrators/integrator.h"
#include "imbatracer/render/scheduling/ray_queue.h"

#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>

#include <iostream>
#include <atomic>

namespace imba {

struct EmptyState { };

void Integrator::estimate_pixel_size() {
    const bool use_gpu = scene_.has_gpu_buffers();

    // Compute rays from the corners of every 8th pixel
    const int ray_count = cam_.width() * cam_.height() / 16;
    RayQueue<EmptyState> q(ray_count, use_gpu);
    for (int x = 0; x < cam_.width(); x += 8) {
        for (int y = 0; y < cam_.height(); y += 8) {
            q.push(cam_.generate_ray(x    , y    ), EmptyState());
            q.push(cam_.generate_ray(x + 1, y    ), EmptyState());
            q.push(cam_.generate_ray(x    , y + 1), EmptyState());
            q.push(cam_.generate_ray(x + 1, y + 1), EmptyState());
        }
    }

    // Traverse the rays and compute the hitpoints
    if (use_gpu)
        q.traverse_gpu(scene_.traversal_data_gpu());
    else
        q.traverse_cpu(scene_.traversal_data_cpu());
    auto hits = q.hits();
    auto rays = q.rays();

    std::atomic<int> count;
    count = 0;
    float total = tbb::parallel_reduce(tbb::blocked_range<int>(0, q.size() / 4), 0.0f,
        [&] (const tbb::blocked_range<int>& range, float init) -> float {
            for (auto i = range.begin(); i != range.end(); ++i) {
                if (hits[i * 4 + 0].tri_id < 0 ||
                    hits[i * 4 + 1].tri_id < 0 ||
                    hits[i * 4 + 2].tri_id < 0 ||
                    hits[i * 4 + 3].tri_id < 0 )
                    continue;

                auto p = [&](int idx) -> float3 {
                    auto dir = float3(rays[idx].dir.x, rays[idx].dir.y, rays[idx].dir.z);
                    auto org = float3(rays[idx].org.x, rays[idx].org.y, rays[idx].org.z);
                    return hits[idx].tmax * dir + org; };
                auto d = [](const float3& a, const float3& b) -> float { return length(a - b); };

                init += d(p(i * 4 + 0), p(i * 4 + 1));
                init += d(p(i * 4 + 1), p(i * 4 + 3));
                init += d(p(i * 4 + 3), p(i * 4 + 2));
                init += d(p(i * 4 + 2), p(i * 4 + 0));

                count += 4;
            }
            return init;
        },
        std::plus<float>());

    if (count == 0) {
        std::cout << "Warning: could not estimate pixel size, nothing was hit by the sample rays." << std::endl;
        pixel_size_ = 1.0f;
    }
    else
        pixel_size_ = total / count;
}

} // namespace imba

#ifndef IMBA_MIS_DEBUG_H
#define IMBA_MIS_DEBUG_H

#include "imbatracer/core/image.h"
#include "imbatracer/loaders/loaders.h"

#include <functional>

namespace imba {

template<int tech_count, bool enabled>
class MISDebugger {
public:
    /// Starts a new frame, resetting all images
    void start_frame(int width, int height, int num_samples) {
        if (!enabled) return;

        frames_.resize(num_samples);
        for (auto& f : frames_) {
            for (int i = 0; i < tech_count; ++i) {
                f.techniques_[i].resize(width, height);
                f.techniques_[i].clear();
            }
        }
    }

    /// Records a contribution made by the given technique to the given pixel sample
    void record(int tech_idx, float weight, const float3& unweighted_contrib, int pixel_id, int sample_id) {
        if (!enabled) return;

        auto& img = frames_[sample_id].techniques_[tech_idx];
        auto val = float4(unweighted_contrib, weight);
        img.pixels()[pixel_id].template apply<std::plus<float>>(val);
    }

    /// Ends the current frame and writes the contribution images to files
    void end_frame(int frame_id) {
        if (!enabled) return;

        int j = 0;
        for (auto& f : frames_) {
            for (int i = 0; i < tech_count; ++i)
                store_png("technique_" + std::to_string(i) + "_frame_" + std::to_string(frame_id) + "_sample_" + std::to_string(j) + ".png",
                    f.techniques_[i], 1.0f, 1.0f, true);
            j++;
        }
    }

private:
    struct SampleData {
        /// Stores the unweighted contribution for every technique as RGB
        /// and the corresponding weights in the alpha channel
        std::array<AtomicImageRGBA, tech_count> techniques_;
    };
    std::vector<SampleData> frames_;
};

} // namespace imba

#endif // IMBA_MIS_DEBUG_H

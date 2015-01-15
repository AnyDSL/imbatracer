#ifndef IMBA_SDL_DEVICE_HPP
#define IMBA_SDL_DEVICE_HPP

#include <SDL.h>
#include "device.hpp"
#include "../scene/image.hpp"

namespace imba {

/// A render device that writes images to disk
class SdlDevice : public Device {
public:
    SdlDevice();
    ~SdlDevice();

    bool render(const Scene& scene, int width, int height, Logger& logger);
    void set_perspective(const Vec3& eye, const Vec3& center, const Vec3& up, float fov, float ratio);

private:
    void render_surface(const Scene& scene);
    bool handle_events(bool);

    float speed_;
    float ratio_, fov_, dist_;
    Vec3 forward_;
    Vec3 eye_;
    Vec3 up_;
    Vec3 right_;

    GBuffer gbuffer_;
    SDL_Surface* screen_;
};

} // namespace imba

#endif // IMBA_SDL_DEVICE_HPP


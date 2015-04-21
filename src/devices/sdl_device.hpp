#ifndef IMBA_SDL_DEVICE_HPP
#define IMBA_SDL_DEVICE_HPP

#include "device.hpp"
#include "../scene/image.hpp"

struct SDL_Surface;

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
    bool handle_events(bool, Logger&);

    float rspeed_, tspeed_;
    float ratio_, fov_, dist_;
    Vec3 forward_;
    Vec3 eye_;
    Vec3 up_;
    Vec3 right_;

    Texture texture_;
    SDL_Surface* screen_;
};

} // namespace imba

#endif // IMBA_SDL_DEVICE_HPP


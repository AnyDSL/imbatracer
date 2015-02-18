#ifndef IMBA_PNG_DEVICE_HPP
#define IMBA_PNG_DEVICE_HPP

#include "device.hpp"
#include "../scene/render.hpp"

namespace imba {

/// A render device that writes images to disk
class PngDevice : public Device {
public:
    PngDevice();

    bool render(const Scene& scene, int width, int height, Logger& logger);
    void set_perspective(const Vec3& eye, const Vec3& center, const Vec3& up, float fov, float ratio);

private:
    Camera cam_;
    std::string path_;
    std::string prefix_;
};

} // namespace imba

#endif // IMBA_PNG_DEVICE_HPP


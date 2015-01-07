#ifndef IMBA_PNG_DEVICE_HPP
#define IMBA_PNG_DEVICE_HPP

#include "device.hpp"

namespace imba {

/// A render device that writes images to disk
class PngDevice : public Device {
public:
    PngDevice();

    bool present(const GBuffer& buffer);

private:
    std::string path_;
    std::string prefix_;
};

} // namespace imba

#endif // IMBA_PNG_DEVICE_HPP


#ifndef IMBA_DEVICE_HPP
#define IMBA_DEVICE_HPP

#include <unordered_map>
#include <sstream>
#include <cctype>

#include "../common/options.hpp"
#include "../common/logger.hpp"
#include "../scene/scene.hpp"

namespace imba {

struct DeviceOption {
    virtual ~DeviceOption() {}
    virtual bool read(const std::string& opt) = 0;
    virtual std::string to_string() const = 0;
};

template <typename T>
struct DeviceOptionImpl : public DeviceOption {
    DeviceOptionImpl(T& v) : value(v) {}

    bool read(const std::string& str) {
        return OptionReader<T>::read(str.c_str(), value);
    }

    std::string to_string() const {
        return OptionWriter<T>::write(value);
    }

    T& value;
};

/// A render device. Displays images or writes them to disk,
/// depending on the implementation.
class Device {
public:
    virtual ~Device() {
        for (auto it = options_.begin(); it != options_.end(); it++) {
            delete it->second;
        }
    }

    bool parse_options(const std::string& str, Logger& logger) {
        bool ok = true;

        int i = 0;
        while (i < str.length()) {
            // Strip spaces
            while (std::isspace(str[i])) i++;

            // Read the option name
            int j = i;
            while (std::isalnum(str[j])) j++;

            const std::string& name = str.substr(i, j - i);
            auto it = options_.find(name);

            while (std::isspace(str[j])) j++;
            
            if (str[j] != '=') {
                logger.log("missing \'=\' sign");
                ok = false;
            } else {
                j++;
            }

            while (std::isspace(str[j])) j++;

            // Read the option value
            std::string value;
            if (str[j] == '\"') {
                // loop until we reach the end or a " sign
                j++;
                i = j;
                while (str[j] != '\0' && str[j] != '\"') j++;

                value = str.substr(i, j - i);

                if (str[j] == '\0') {
                    logger.log("end of input reached while parsing device option");
                    ok = false;
                } else {
                    j++;
                }
            } else {
                i = j;
                while (str[j] != '\0' && !std::isspace(str[j])) j++;
                value = str.substr(i, j - i);
            }

            if (it == options_.end()) {
                logger.log("unknown device option \'", name, "\'");
                ok = false;
            } else if (!it->second->read(value)) {
                logger.log("incorrect device option type for \'", it->first, "\'");
                ok = false;
            }

            while (std::isspace(str[j])) j++;
            i = j;
        }

        for (auto it = options_.begin(); it != options_.end(); it++) {
            logger.log("device option \'", it->first, "\' = \'", it->second->to_string(), "\'");
        }

        return ok;
    }

    virtual bool render(const Scene& scene, int width, int height, Logger& logger) = 0;

    // TODO : replace this call by a ray generation shader
    virtual void set_perspective(const Vec3& eye, const Vec3& center, const Vec3& up, float fov, float ratio) = 0;

protected:
    template <typename T>
    void register_option(const std::string& opt, T& value, const T& def = T()) {
        assert(options_.find(opt) == options_.end());
        value = def;
        options_[opt] = new DeviceOptionImpl<T>(value);
    }

private:
    std::unordered_map<std::string, DeviceOption*> options_;
};

} // namespace imba

#endif // IMBA_DEVICE_HPP


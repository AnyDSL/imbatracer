#ifndef IMBA_THORIN_ARRAY
#define IMBA_THORIN_ARRAY

#include <vector>
using std::size_t;
#include <thorin_runtime.hpp>

namespace imba {

template <typename T, thorin::Platform Platform>
class ThorinArray {
public:
    ThorinArray() {}

    ThorinArray(int64_t size, thorin::Device dev = thorin::Device(0))
        : host_array(size)
        , device_array(Platform, dev, size)
    {}

    ThorinArray(const std::vector<T>& rhs, thorin::Device dev = thorin::Device(0))
        : host_array(rhs.size())
        , device_array(Platform, dev, rhs.size())
    {
        std::copy(rhs.begin(), rhs.end(), host_array.begin());
    }

    ThorinArray(ThorinArray&& other) = default;
    ThorinArray& operator = (ThorinArray&& other) = default;

    ThorinArray(const ThorinArray&) = delete;
    ThorinArray& operator = (const ThorinArray&) = delete;

    void upload() { upload(size()); }
    void download() { download(size()); }

    // Uploads the host data to the device.
    void upload(int count) { thorin::copy(host_array, device_array, count); }

    // Downloads the data from the device to the host.
    void download(int count) { thorin::copy(host_array, device_array, count); }

    T* begin() { return host_array.begin(); }
    const T* begin() const { return host_array.begin(); }

    T* end() { return host_array.end(); }
    const T* end() const { return host_array.end(); }

    T* host_data() { return host_array.data(); }
    const T* host_data() const { return host_array.data(); }

    T* device_data() { return device_array.data(); }
    const T* device_data() const { return device_array.data(); }

    int64_t size() const { return host_array.size(); }

    const T& operator [] (int i) const { return host_array[i]; }
    T& operator [] (int i) { return host_array[i]; }

private:
    thorin::Array<T> device_array;
    thorin::Array<T> host_array;
};

template <typename T>
class ThorinArray<T, thorin::Platform::HOST> {
public:
    ThorinArray() {}

    ThorinArray(int64_t size)
        : host_array(size)
    {}

    ThorinArray(const std::vector<T>& rhs)
        : host_array(rhs.size())
    {
        std::copy(rhs.begin(), rhs.end(), host_array.begin());
    }

    ThorinArray(ThorinArray&& other) = default;
    ThorinArray& operator = (ThorinArray&& other) = default;

    ThorinArray(const ThorinArray&) = delete;
    ThorinArray& operator = (const ThorinArray&) = delete;

    void upload() {}
    void download() {}
    void upload(int count) {}
    void download(int count) {}

    T* begin() { return host_array.begin(); }
    const T* begin() const { return host_array.begin(); }

    T* end() { return host_array.end(); }
    const T* end() const { return host_array.end(); }

    T* host_data() { return host_array.data(); }
    const T* host_data() const { return host_array.data(); }

    T* device_data() { return host_array.data(); }
    const T* device_data() const { return host_array.data(); }

    int64_t size() const { return host_array.size(); }

    const T& operator [] (int i) const { return host_array[i]; }
    T& operator [] (int i) { return host_array[i]; }

private:
    thorin::Array<T> host_array;
};

} // namespace imba

#endif // IMBA_THORIN_ARRAY_H

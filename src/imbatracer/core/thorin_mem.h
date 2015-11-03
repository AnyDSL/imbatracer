#ifndef THORIN_MEM
#define THORIN_MEM

#include "thorin_runtime.hpp"
#include <vector>

#define TRAVERSAL_DEVICE 	0
#define TRAVERSAL_PLATFORM 	CUDA

namespace imba {
	
template <typename T>
class ThorinArray {
public:
	ThorinArray() {}
	
	ThorinArray(int64_t size)
		: device_array(thorin::Platform::TRAVERSAL_PLATFORM, thorin::Device(TRAVERSAL_DEVICE), size),
		  host_array(size) 
	{}
	
	ThorinArray(ThorinArray&& other) = default;
	ThorinArray& operator = (ThorinArray&& other) = default;
	
	ThorinArray(const ThorinArray&) = delete;
	ThorinArray& operator = (const ThorinArray&) = delete;

	// Uploads the host data to the device.
	void upload() {
		thorin::copy(host_array, device_array);
	}
	
	// Downloads the data from the device to the host.
	void download() {
		thorin::copy(device_array, host_array);
	}
	
	T* begin() { return host_array.begin(); }
    const T* begin() const { return host_array.begin(); }
    
    T* end() { return host_array.end(); }
    const T* end() const { return host_array.end(); }

    T* data() { return host_array.data(); }
    const T* data() const { return host_array.data(); }
	
    int64_t size() const { return host_array.size(); }

    const T& operator [] (int i) const { return host_array[i]; }
    T& operator [] (int i) { return host_array[i]; }
	
private:
	thorin::Array<T> device_array;
	thorin::Array<T> host_array;
};

}

#endif
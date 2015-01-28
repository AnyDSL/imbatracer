#include <png.h>
#include <chrono>
#include "png_device.hpp"

namespace imba {

static void write_to_stream(png_structp png_ptr, png_bytep data, png_size_t length) {
    png_voidp a = png_get_io_ptr(png_ptr);
    ((std::ostream*)a)->write((const char*)data, length);
}

void flush_stream(png_structp png_ptr) {
    // Nothing to do
}

PngDevice::PngDevice() {
    register_option("path", path_, std::string("."));
    register_option("prefix", prefix_);
}

bool PngDevice::render(const Scene& scene, int width, int height, Logger& logger) {
    GBuffer gbuffer(width, height);

    // Ensure scene is ready so that render time measurements do not include scene update
    scene.compile();

    auto t0 = std::chrono::high_resolution_clock::now();
    imba::Render::render_gbuffer(scene, cam_, gbuffer);
    auto t1 = std::chrono::high_resolution_clock::now();

    logger.log("G-Buffer rendered in ", std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count(), " ms");

    std::ofstream file(path_ + "//" + prefix_ + "gbuffer.png");
    if (!file)
        return false;

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return false;
    }

    png_bytep row = nullptr;
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        delete[] row;
        return false;
    }

    png_set_write_fn(png_ptr, &file, write_to_stream, flush_stream);

    png_set_IHDR(png_ptr, info_ptr, gbuffer.width(), gbuffer.height(),
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);
    
    row = new png_byte[4 * gbuffer.width()];

    for (int y = 0; y < gbuffer.height(); y++) {
        const GBufferPixel* buf_row = gbuffer.row(y);
        for (int x = 0; x < gbuffer.width(); x++) {
            // r is t/tmax
            // g is u
            // b is v
            // a is mat_id != 0
            row[x * 4 + 0] = (png_byte)(255 * buf_row[x].t);
            row[x * 4 + 1] = (png_byte)(255 * buf_row[x].u);
            row[x * 4 + 2] = (png_byte)(255 * buf_row[x].v);
            row[x * 4 + 3] = (png_byte)(buf_row[x].inst_id >= 0 ? 255 : 0);
        }
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    delete[] row;

    return true;
}

void PngDevice::set_perspective(const Vec3& eye, const Vec3& center, const Vec3& up, float fov, float ratio) {
    cam_ = Render::perspective_camera(eye, center, up, fov, ratio);
}

} // namespace imba



#ifndef IMBA_STORE_PNG_H
#define IMBA_STORE_PNG_H

#include <png.h>
#include <fstream>
#include <cstring>
#include <memory>

namespace imba {

namespace {
    inline void write_to_stream(png_structp png_ptr, png_bytep data, png_size_t length) {
        png_voidp a = png_get_io_ptr(png_ptr);
        ((std::ostream*)a)->write((const char*)data, length);
    }

    inline void flush_stream(png_structp png_ptr) {
        // Nothing to do
    }
}

template<typename T>
static bool store_png(const Path& path, const ImageBase<T>& img, float weight, float gamma, bool include_alpha) {
    std::ofstream file(path, std::ofstream::binary);
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

    std::unique_ptr<png_byte[]> row;
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }

    png_set_write_fn(png_ptr, &file, write_to_stream, flush_stream);

    png_set_IHDR(png_ptr, info_ptr, img.width(), img.height(),
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    row.reset(new png_byte[4 * img.width()]);

    for (int y = 0; y < img.height(); y++) {
        const auto* accum_row = img.row(y);
        png_bytep img_row = row.get();
        for (int x = 0; x < img.width(); x++) {
            img_row[x * 4 + 0] = (png_byte)(255.0f * clamp(powf(accum_row[x][0] * weight, gamma), 0.0f, 1.0f));
            img_row[x * 4 + 1] = (png_byte)(255.0f * clamp(powf(accum_row[x][1] * weight, gamma), 0.0f, 1.0f));
            img_row[x * 4 + 2] = (png_byte)(255.0f * clamp(powf(accum_row[x][2] * weight, gamma), 0.0f, 1.0f));
            img_row[x * 4 + 3] = include_alpha ? (png_byte)(255.0f * clamp(powf(accum_row[x][3] * weight, gamma), 0.0f, 1.0f)) : (png_byte)(255.0f);
        }
        png_write_row(png_ptr, row.get());
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return true;
}

}

#endif // IMBA_STORE_PNG_H
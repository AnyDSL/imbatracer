#include <png.h>
#include "png_loader.hpp"

namespace imba {

bool PngLoader::check_format(const Path& path) {
    // Check extension
    if (path.extension() != "png")
        return false;
    
    // Check first 8 bytes of the file
    std::ifstream file(path);
    if (!file)
        return false;

    char sig[8];
    file.read(sig, 8);
    return png_check_sig((unsigned char*)sig, 8) != 0;
}

void read_from_stream(png_structp png_ptr, png_bytep data, png_size_t length) {
    png_voidp a = png_get_io_ptr(png_ptr);
    ((std::istream*)a)->read((char*)data, length);
}

bool PngLoader::load_file(const Path& path, Image& image, Logger* logger) {
    std::ifstream file(path);
    if (!file)
        return false;

    // Read signature
    {
        char sig[8];
        file.read(sig, 8);
        if (!png_check_sig((unsigned char*)sig, 8))
            return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return false;
    }

    png_bytep* row_ptrs = nullptr;
    char* data = nullptr;
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        delete[] row_ptrs;
        delete[] data;
        return false;
    }

    png_set_sig_bytes(png_ptr, 8);
    png_set_read_fn(png_ptr, (png_voidp)&file, read_from_stream);
    png_read_info(png_ptr, info_ptr);

    int width  = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);

    png_uint_32 color_type = png_get_color_type(png_ptr, info_ptr);
    png_uint_32 bit_depth  = png_get_bit_depth(png_ptr, info_ptr);
    png_uint_32 channels   = png_get_channels(png_ptr, info_ptr);

    // Expand paletted and grayscale images to RGB
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    } else if (color_type == PNG_COLOR_TYPE_GRAY ||
               color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    
    // Transform to 8 bit per channel
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    // Get alpha channel when there is one
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    // Otherwise add an opaque alphq channel
    if (color_type == PNG_COLOR_TYPE_RGB)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

    int stride = width * (bit_depth / 8) * channels;
    row_ptrs = new png_bytep[height];
    data = new char[stride * height];

    for (int i = 0; i < height; i++) {
        row_ptrs[i] = (png_bytep)data + stride * (height - 1 - i);
    }

    png_read_image(png_ptr, row_ptrs);
    delete[] row_ptrs;
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

    // Convert to floating point format
    image.resize(width, height, width);

    for (int y = 0; y < height; y++) {
        Image::Pixel* row_out = image.row(y);
        const char* row_in = data + y * stride;

        for (int x = 0; x < width; x++) {
            row_out[x] = Image::Pixel(row_in[x * 4 + 0] / 255.0f,
                                      row_in[x * 4 + 1] / 255.0f,
                                      row_in[x * 4 + 2] / 255.0f,
                                      row_in[x * 4 + 3] / 255.0f);
        }    
    }

    delete[] data;
    
    return true;
}

} // namespace imba



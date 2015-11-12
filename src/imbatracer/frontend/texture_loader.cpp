#include "texture_loader.h"

#include <png.h>
#include <cstring>
#include <cassert>

namespace imba {
	
void read_from_stream(png_structp png_ptr, png_bytep data, png_size_t length) {
    png_voidp a = png_get_io_ptr(png_ptr);
    ((std::istream*)a)->read((char*)data, length);
}

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
    return png_check_sig((unsigned char*)sig, 8);
}

bool PngLoader::load_file(const Path& path, Image& texture, Logger* logger) {
    std::ifstream file(path, std::ifstream::binary);
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
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        delete[] row_ptrs;
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

    // Otherwise add an opaque alpha channel
    if (color_type == PNG_COLOR_TYPE_RGB)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

    texture.resize(width, height);
    std::vector<png_byte> row_bytes(width * 4);
    for (int y = 0; y < height; y++) {
        png_read_row(png_ptr, row_bytes.data(), nullptr);
        float* buf_row = texture.row(y);
        for (int x = 0; x < width; x++) {
            buf_row[x * 4 + 0] = row_bytes[x * 4 + 0] / 255.0f;
            buf_row[x * 4 + 1] = row_bytes[x * 4 + 1] / 255.0f;
            buf_row[x * 4 + 2] = row_bytes[x * 4 + 2] / 255.0f;
            buf_row[x * 4 + 3] = row_bytes[x * 4 + 3] / 255.0f;
        }
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    
    if (logger) {
        logger->log("PNG image (", width, "x", height, " pixels)");
    }

    return true;
}

struct TgaHeader
{
    unsigned short width;
    unsigned short height;
    unsigned char bpp;
    unsigned char desc;
};

enum TgaType {
    TGA_NONE,
    TGA_RAW,
    TGA_COMP
};

inline TgaType check_signature(const char* sig) {
    const char raw_sig[12] = {0,0,2, 0,0,0,0,0,0,0,0,0};
    const char comp_sig[12] = {0,0,10,0,0,0,0,0,0,0,0,0};

    if (!std::memcmp(sig, raw_sig, sizeof(char) * 12))
        return TGA_RAW;

    if (!std::memcmp(sig, comp_sig, sizeof(char) * 12))
        return TGA_COMP;

    return TGA_NONE;
}

bool TgaLoader::check_format(const Path& path) {
    // Check extension
    if (path.extension() != "tga")
        return false;
        
    // Check first 8 bytes of the file
    std::ifstream file(path);
    if (!file)
        return false;
        
    char sig[12];
    file.read(sig, 12);

    return check_signature(sig) != TGA_NONE;
}

inline void copy_pixels24(float* tex, const unsigned char* pixels, int n) {
    for (int i = 0; i < n; i++) {
        tex[i * 4 + 2] = pixels[i * 3 + 0] / 255.0f;
        tex[i * 4 + 1] = pixels[i * 3 + 1] / 255.0f;
        tex[i * 4 + 0] = pixels[i * 3 + 2] / 255.0f;
        tex[i * 4 + 3] = 1.0f;
    }
}

inline void copy_pixels32(float* tex, const unsigned char* pixels, int n) {
    for (int i = 0; i < n; i++) {
        tex[i * 4 + 2] = pixels[i * 4 + 0] / 255.0f;
        tex[i * 4 + 1] = pixels[i * 4 + 1] / 255.0f;
        tex[i * 4 + 0] = pixels[i * 4 + 2] / 255.0f;
        tex[i * 4 + 3] = pixels[i * 4 + 3] / 255.0f;
    }
}

static void load_raw(const TgaHeader& tga, std::istream& stream, Image& texture, Logger* logger) {
    assert(tga.bpp == 24 || tga.bpp == 32);
    texture.resize(tga.width, tga.height);

    if (tga.bpp == 24) {
        std::vector<char> tga_row(3 * tga.width);
        for (int y = 0; y < tga.height; y++) {
            float* row = texture.row(tga.height - y - 1);
            stream.read(tga_row.data(), tga_row.size());
            copy_pixels24(row, (unsigned char*)tga_row.data(), tga.width);
        }
    } else {
        std::vector<char> tga_row(4 * tga.width);
        for (int y = 0; y < tga.height; y++) {
            float* row = texture.row(tga.height - y - 1);
            stream.read(tga_row.data(), tga_row.size());
            copy_pixels32(row, (unsigned char*)tga_row.data(), tga.width);
        }
    }
}

static void load_compressed(const TgaHeader& tga, std::istream& stream, Image& texture, Logger* logger) {
    assert(tga.bpp == 24 || tga.bpp == 32);
    texture.resize(tga.width, tga.height);

    const int pix_count = tga.width * tga.height;
    int cur_pix = 0;
    while (cur_pix < pix_count) {
        unsigned char chunk;
        stream.read((char*)&chunk, 1);

        if (chunk < 128) {
            chunk++;

            char pixels[4 * 128];
            stream.read(pixels, chunk * (tga.bpp / 8));
            if (cur_pix + chunk > pix_count) chunk = pix_count - cur_pix;
            
            if (tga.bpp == 24) {
                copy_pixels24(texture.pixels() + cur_pix, (unsigned char*)pixels, chunk);
            } else {
                copy_pixels32(texture.pixels() + cur_pix, (unsigned char*)pixels, chunk);
            }

            cur_pix += chunk;
        } else {
            chunk -= 127;
            unsigned char tga_pix[4];
            stream.read((char*)tga_pix, (tga.bpp / 8));

            if (cur_pix + chunk > pix_count) chunk = pix_count - cur_pix;

            float* pix = texture.pixels() + cur_pix;
            if (tga.bpp == 24) {
                for (int i = 0; i < chunk; i++) {
                    pix[i * 4 + 2] = tga_pix[0] / 255.0f;
                    pix[i * 4 + 1] = tga_pix[1] / 255.0f;
                    pix[i * 4 + 0] = tga_pix[2] / 255.0f;
                    pix[i * 4 + 3] = 1.0f;
                }
            } else {
                for (int i = 0; i < chunk; i++) {
                    pix[i * 4 + 2] = tga_pix[0] / 255.0f;
                    pix[i * 4 + 1] = tga_pix[1] / 255.0f;
                    pix[i * 4 + 0] = tga_pix[2] / 255.0f;
                    pix[i * 4 + 3] = tga_pix[3] / 255.0f;
                }
            }

            cur_pix += chunk;
        }
    }
}

bool TgaLoader::load_file(const Path& path, Image& texture, Logger* logger) {
    std::ifstream file(path, std::ifstream::binary);
    if (!file)
        return false;

    // Read signature
    char sig[12];
    file.read(sig, 12);
    TgaType type = check_signature(sig);
    if (type == TGA_NONE)
        return false;

    TgaHeader header;
    file.read((char*)&header, sizeof(TgaHeader));
    if (!file) return false;

    if (header.width <= 0 || header.height <= 0 ||
        (header.bpp != 24 && header.bpp !=32)) {
        return false;
    }

    if (type == TGA_RAW) {
        load_raw(header, file, texture, logger);
    } else {
        load_compressed(header, file, texture, logger);
    }

    if (logger) {
        logger->log("TGA image (", header.width, "x", header.height, " pixels)");
    }

    return true;
}
	
} // namespace imba
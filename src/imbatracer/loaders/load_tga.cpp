#include <fstream>
#include <cstring>
#include <cassert>

#include "imbatracer/loaders/loaders.h"

namespace imba {

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

inline void copy_pixels24(float4* img, const unsigned char* pixels, int n) {
    for (int i = 0; i < n; i++) {
        img[i].z = pixels[i * 3 + 0] / 255.0f;
        img[i].y = pixels[i * 3 + 1] / 255.0f;
        img[i].x = pixels[i * 3 + 2] / 255.0f;
        img[i].w = 1.0f;
    }
}

inline void copy_pixels32(float4* img, const unsigned char* pixels, int n) {
    for (int i = 0; i < n; i++) {
        img[i].z = pixels[i * 4 + 0] / 255.0f;
        img[i].y = pixels[i * 4 + 1] / 255.0f;
        img[i].x = pixels[i * 4 + 2] / 255.0f;
        img[i].w = pixels[i * 4 + 3] / 255.0f;
    }
}

static void load_raw(const TgaHeader& tga, std::istream& stream, Image& image) {
    assert(tga.bpp == 24 || tga.bpp == 32);

    if (tga.bpp == 24) {
        std::vector<char> tga_row(3 * tga.width);
        for (int y = 0; y < tga.height; y++) {
            float4* row = image.row(tga.height - y - 1);
            stream.read(tga_row.data(), tga_row.size());
            copy_pixels24(row, (unsigned char*)tga_row.data(), tga.width);
        }
    } else {
        std::vector<char> tga_row(4 * tga.width);
        for (int y = 0; y < tga.height; y++) {
            float4* row = image.row(tga.height - y - 1);
            stream.read(tga_row.data(), tga_row.size());
            copy_pixels32(row, (unsigned char*)tga_row.data(), tga.width);
        }
    }
}

static void load_compressed(const TgaHeader& tga, std::istream& stream, Image& image) {
    assert(tga.bpp == 24 || tga.bpp == 32);

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
                copy_pixels24(image.pixels() + cur_pix, (unsigned char*)pixels, chunk);
            } else {
                copy_pixels32(image.pixels() + cur_pix, (unsigned char*)pixels, chunk);
            }

            cur_pix += chunk;
        } else {
            chunk -= 127;

            unsigned char tga_pix[4];
            tga_pix[3] = 255;
            stream.read((char*)tga_pix, (tga.bpp / 8));

            if (cur_pix + chunk > pix_count) chunk = pix_count - cur_pix;

            float4* pix = image.pixels() + cur_pix;
            const float4 c(tga_pix[2] / 255.0f,
                           tga_pix[1] / 255.0f,
                           tga_pix[0] / 255.0f,
                           tga_pix[3] / 255.0f);
            for (int i = 0; i < chunk; i++)
                pix[i] = c;

            cur_pix += chunk;
        }
    }
}

bool load_tga(const Path& path, Image& image) {
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

    image.resize(header.width, header.height);

    if (type == TGA_RAW) {
        load_raw(header, file, image);
    } else {
        load_compressed(header, file, image);
    }

    return true;
}

} // namespace imba

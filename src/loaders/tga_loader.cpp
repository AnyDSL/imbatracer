#include <png.h>
#include <string.h>
#include <cassert>
#include "tga_loader.hpp"

namespace imba {

struct TgaHeader
{
    unsigned short width;
    unsigned short height;
    unsigned char bpp;
};

enum TgaType {
    TGA_NONE,
    TGA_RAW,
    TGA_COMP
};

inline TgaType check_signature(const char* sig) {
    const char raw_sig[12] = {0,0,2, 0,0,0,0,0,0,0,0,0};
    const char comp_sig[12] = {0,0,10,0,0,0,0,0,0,0,0,0};

    if (!memcmp(sig, raw_sig, sizeof(char) * 12))
        return TGA_RAW;

    if (!memcmp(sig, comp_sig, sizeof(char) * 12))
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

inline void copy_pixels24(TexturePixel* tex, const unsigned char* pixels, int n) {
    for (int i = 0; i < n; i++) {
        tex[i].b = pixels[i * 3 + 0] / 255.0f;
        tex[i].g = pixels[i * 3 + 1] / 255.0f;
        tex[i].r = pixels[i * 3 + 2] / 255.0f;
        tex[i].a = 1.0f;
    }
}

inline void copy_pixels32(TexturePixel* tex, const unsigned char* pixels, int n) {
    for (int i = 0; i < n; i++) {
        tex[i].b = pixels[i * 4 + 0] / 255.0f;
        tex[i].g = pixels[i * 4 + 1] / 255.0f;
        tex[i].r = pixels[i * 4 + 2] / 255.0f;
        tex[i].a = pixels[i * 4 + 3] / 255.0f;
    }
}

static void load_raw(const TgaHeader& tga, std::istream& stream, Texture& texture, Logger* logger) {
    assert(tga.bpp == 24 || tga.bpp == 32);
    texture.resize(tga.width, tga.height);

    if (tga.bpp == 24) {
        std::vector<char> tga_row(3 * tga.width);
        for (int y = 0; y < tga.height; y++) {
            TexturePixel* row = texture.row(tga.height - y - 1);
            stream.read(tga_row.data(), tga_row.size());
            copy_pixels24(row, (unsigned char*)tga_row.data(), tga.width);
        }
    } else {
        std::vector<char> tga_row(4 * tga.width);
        for (int y = 0; y < tga.height; y++) {
            TexturePixel* row = texture.row(tga.height - y - 1);
            stream.read(tga_row.data(), tga_row.size());
            copy_pixels32(row, (unsigned char*)tga_row.data(), tga.width);
        }
    }
}

static void load_compressed(const TgaHeader& tga, std::istream& stream, Texture& texture, Logger* logger) {
    assert(tga.bpp == 24 || tga.bpp == 32);
    texture.resize(tga.width, tga.height);

    const int pix_count = tga.width * tga.height;
    int cur_pix = 0;
    while (cur_pix < pix_count) {
        unsigned char chunk;
        stream.read((char*)&chunk, 1);

        if (chunk < 128) {
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
            char tga_pix[4];
            stream.read(tga_pix, (tga.bpp / 8));

            if (cur_pix + chunk > pix_count) chunk = pix_count - cur_pix;

            TexturePixel* pix = texture.pixels() + cur_pix;
            if (tga.bpp == 24) {
                for (int i = 0; i < chunk; i++) {
                    pix[i].b = tga_pix[0] / 255.0f;
                    pix[i].g = tga_pix[1] / 255.0f;
                    pix[i].r = tga_pix[2] / 255.0f;
                    pix[i].a = 1.0f;
                }
            } else {
                for (int i = 0; i < chunk; i++) {
                    pix[i].b = tga_pix[0] / 255.0f;
                    pix[i].g = tga_pix[1] / 255.0f;
                    pix[i].r = tga_pix[2] / 255.0f;
                    pix[i].a = tga_pix[3] / 255.0f;
                }
            }

            cur_pix += chunk;
        }
    }
}

bool TgaLoader::load_file(const Path& path, Texture& texture, Logger* logger) {
    std::ifstream file(path);
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

    if(header.width <= 0 || header.height <= 0 ||
       (header.bpp != 24 && header.bpp !=32)) {
        return false;    
    }

    if (type == TGA_RAW) {
        load_raw(header, file, texture, logger);
    } else {
        load_compressed(header, file, texture, logger);
    }

    return true;
}

} // namespace imba


#include <fstream>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include "loaders.h"

namespace imba {

struct HDRInfo {
    int width;
    int height;
};

using hdr_byte = unsigned char;

struct HDRPixel {
    hdr_byte r, g, b, e;

    hdr_byte& operator[](int i) {
        if (i == 0) return r;
        else if (i == 1) return g;
        else if (i == 2) return b;
        else /*if (i == 3)*/ return e;
    }

    operator rgb () const {
        float exp = ldexp(1.0, e - (128 + 8));
        return rgb((r + 0.5) * exp,
                   (g + 0.5) * exp,
                   (b + 0.5) * exp);
    }

    operator rgba () const {
        return rgba(*this, 0.0f);
    }
};

std::istream& operator >> (std::istream& str, HDRPixel& out) {
    out.r = str.get();
    out.g = str.get();
    out.b = str.get();
    out.e = str.get();
    return str;
}

bool hdr_check_signature(std::ifstream& file) {
    auto sig = "#?RADIANCE";

    std::string buf;
    if (!std::getline(file, buf))
        return false;

    /*if (buf != sig)
        return false;*/

    return true;
}

bool hdr_parse_command(const std::string& cmd, HDRInfo& info) {
    // Handle commands (Format, Exposure, Color correction, etc.) here.
    return true;
}

bool hdr_parse_resolution(const std::string& resline, HDRInfo& info) {
    std::stringstream str(resline);

    std::string first_axis, second_axis;
    str >> first_axis;
    str >> info.height;
    str >> second_axis;
    str >> info.width;

    // We only support the standard coordinate system at the moment.
    if (first_axis != "-Y" || second_axis != "+X") {
        std::cout << " Unsupported axis configuration in the .hdr file." << std::endl;
        return false;
    }

    return true;
}

bool hdr_rle_decode(std::istream& str, rgba* out, int len) {
    HDRPixel last_valid;
    str >> last_valid;

    out[0] = last_valid;
    int pos = 1;

    for (int i = 1; i < len; ++i) {
        HDRPixel pix;
        str >> pix;

        if (pix.r == 1 && pix.g == 1 && pix.b == 1) {
            int repeat = pix.e;
            for (int k = 0; k < repeat; ++k) {
                out[pos++] = last_valid;
            }
            i += repeat;
        } else
            last_valid = pix;
    }

    return true;
}

bool hdr_adaptive_rle_decode(std::istream& str, rgba* out, int len) {
    // Each component is stored individually.
    std::vector<HDRPixel> pixels(len);
    for (int comp = 0; comp < 4; ++comp) {
        for (int i = 0; i < len; /*i is not incremented*/) {
            hdr_byte code = str.get();

            if (code > 128) { // run
                code &= 127;
                hdr_byte val = str.get();

                if (i + code > len) {
                    std::cout << " ERROR: overrun in hdr scanline (rle run)." << std::endl;
                    return false;
                }

                while (code--) {
                    pixels[i++][comp] = val;
                }
            } else { // dump
                if (i + code > len) {
                    std::cout << " ERROR: overrun in hdr scanline (rle dump)." << std::endl;
                    return false;
                }

                while (code--) {
                    hdr_byte val = str.get();
                    pixels[i++][comp] = val;
                }
            }
        }
    }

    // Convert all HDRPixels to rgba.
    for (auto& p : pixels) {
        // auto x = rgba(p);
        // printf("%f %f %f    ", x.x, x.y, x.z);
        // printf("%d %d %d %d  ", p.r, p.g, p.b, p.e);
        // printf("\n");
        // break;

        *(out++) = rgba(p);
    }

    return true;
}

bool hdr_parse_scanline(std::istream& str, const HDRInfo& info, Image& image, int y) {
    const int min_scanline_len = 8;
    const int max_scanline_len = 0x7fff;

    HDRPixel pix;
    str >> pix;

    if (pix.r == 2 && pix.g == 2 && !(pix.b & 128)) {
        // Adaptive run-length encoding.
        if (((pix.b << 8) | pix.e) != image.width()) {
            std::cout << " ERROR: hdr scanline length mismatch!" << std::endl;
            return false;
        }
        hdr_adaptive_rle_decode(str, image.row(y), image.width());
    } else {
        // Run-length encoding or uncompressed.
        str.seekg(-4, std::ios_base::cur);
        hdr_rle_decode(str, image.row(y), image.width());
    }

    return true;
}

bool load_hdr(const Path& path, Image& image) {
    std::ifstream file(path, std::ifstream::binary);
    if (!file) {
        std::cout << " ERROR: Could not open file " << path.path() << std::endl;
        return false;
    }

    // Check the signature
    if (!hdr_check_signature(file)) {
        std::cout << " ERROR: Not a valid .hdr file " << path.file_name() << std::endl;
        return false;
    }

    HDRInfo info;

    // Everything until the next empty line is a header command.
    std::string cmd;
    while(true) {
        if (!std::getline(file, cmd)) {
            std::cout << " Unexpected EOF while parsing the .hdr header " << path.file_name() << std::endl;
            return false;
        }

        if (cmd.size() == 0)
            break;

        if (!hdr_parse_command(cmd, info))
            return false;
    }

    // Read the resolution string.
    std::string resline;
    if (!std::getline(file, resline)) {
        std::cout << " Unexpected EOF while parsing the .hdr resolution string " << path.file_name() << std::endl;
        return false;
    }

    if (!hdr_parse_resolution(resline, info))
        return false;

    image.resize(info.width, info.height);

    // Parse the actual color values.
    for (int y = 0; y < info.height; ++y) {
        if (!hdr_parse_scanline(file, info, image, y)) {
            std::cout << " Unexpected EOF while reading the .hdr scanlines " << path.file_name() << std::endl;
            return false;
        }
    }

    return true;
}

} // namespace imba

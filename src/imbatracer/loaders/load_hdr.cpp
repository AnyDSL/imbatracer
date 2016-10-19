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

struct HDRPixel {
    char r, g, b, e;

    char& operator[](int i) {
        if (i == 0) return r;
        else if (i == 1) return g;
        else if (i == 2) return b;
        else /*if (i == 3)*/ return e;
    }

    operator rgb () const {
        float exp = powf(2.0f, e);
        return rgb((r / 256.0f) * exp,
                   (g / 256.0f) * exp,
                   (b / 256.0f) * exp);
    }

    operator rgba () const {
        return rgba(*this, 0.0f);
    }
};

std::istream& operator >> (std::istream& str, HDRPixel& out) {
    str >> out.r;
    str >> out.g;
    str >> out.b;
    str >> out.e;
    return str;
}

bool hdr_check_signature(std::ifstream& file) {
    auto sig = "#?RADIANCE";
    char buf[10];
    if (!file.get(buf, 10) || memcmp(sig, buf, 10))
        return false;
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
    if (first_axis != "-Y" || second_axis != "+X")
        return false;

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
        for (int i = 0; i < len; ++i) {
            char code;
            str >> code;
            if (code > 128) { // run
                char val;
                str >> val;
                while (code--) {
                    pixels[i++][comp] = val;
                }
            } else { // dump
                while (code--) {
                    char val;
                    str >> val;
                    pixels[i++][comp] = val;
                }
            }
        }
    }

    // Convert all HDRPixels to rgba.
    for (auto& p : pixels) {
        *(out++) = p;
    }
}

bool hdr_parse_scanline(const std::string& scanline, const HDRInfo& info, Image& image, int y) {
    const int min_scanline_len = 8;
    const int max_scanline_len = 0x7fff;

    std::stringstream str(scanline);

    HDRPixel pix;
    str >> pix;

    if (pix.r == 2 && pix.g == 2) {
        // Adaptive run-length encoding.
        hdr_adaptive_rle_decode(str, image.row(y), image.width());
    } else {
        // Run-length encoding or uncompressed.
        str.seekg(0);
        hdr_rle_decode(str, image.row(y), image.width());
    }

    return true;
}

bool load_hdr(const Path& path, Image& image) {
    std::ifstream file(path, std::ifstream::binary);
    if (!file)
        return false;

    // Check the signature
    if (!hdr_check_signature(file))
        return false;

    HDRInfo info;

    // Everything until the next empty line is a header command.
    std::string cmd;
    while(true) {
        if (!std::getline(file, cmd))
            return false;

        if (cmd.size() == 0)
            break;

        if (!hdr_parse_command(cmd, info))
            return false;
    }

    // Read the resolution string.
    std::string resline;
    if (!std::getline(file, resline))
        return false;
    if (!hdr_parse_resolution(resline, info))
        return false;

    image.resize(info.width, info.height);

    // Parse the actual color values.
    for (int y = 0; y < info.height; ++y) {
        std::string scanline;
        if (!std::getline(file, scanline))
            return false;
        if (!hdr_parse_scanline(scanline, info, image, y))
            return false;
    }

    return true;
}

} // namespace imba
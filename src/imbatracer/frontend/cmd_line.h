#ifndef IMBA_CMD_LINE_H
#define IMBA_CMD_LINE_H

#include <string>
#include <iostream>
#include <sstream>
#include <cfloat>
#include <climits>

namespace imba {

struct UserSettings {
    std::string input_file;
    std::string output_file;

    enum Algorithm {
        PT,
        BPT,
        VCM
    } algorithm;

    int width, height;

    int max_samples;
    float max_time_sec;

    bool background;

    UserSettings()
        : input_file(""), output_file("render.png"), algorithm(PT),
          width(512), height(512), max_samples(INT_MAX), max_time_sec(FLT_MAX),
          background(false)
    {}
};

inline void print_help() {
    std::cout << "Usage: imbatracer <input_file.scene> [-q | -s <max_samples> | -t <max_time_seconds> |" << std::endl
              << "                                    -a <algorithm> | -w <width> | -h <height> | <output_file.png> ]"
              << std::endl << std::endl
              << "    -q  Quiet mode, render in background without SDL preview." << std::endl
              << "    -s  Number of samples per pixel to render (default: unlimited)" << std::endl
              << "    -t  Number of seconds to run the render algorithm (default: unlimited)" << std::endl
              << "    -a  Selects which algorithm to use, 'pt', 'bpt', or 'vcm' (default: pt)" << std::endl
              << "    -w  Sets the horizontal resolution in pixels (default: 512)" << std::endl
              << "    -h  Sets the vertical resolution in pixels (default: 512)" << std::endl
              << "  If time (-t) and number of samples (-s) are both given, time has higher priority." << std::endl;
}

namespace {
    template<typename T>
    inline bool parse_argument(int i, int argc, char* argv[], T& val) {
        if (i >= argc) {
            std::cout << "Too few arguments." << std::endl;
            return false;
        }

        std::istringstream iss(argv[i]);
        iss >> val;

        if (iss.rdstate() & (std::istringstream::failbit | std::istringstream::eofbit))
            return false;

        return true;
    }

    constexpr int MAX_ALLOWED_SAMPLES = 1000000; // at most 1 million samples
    constexpr float MAX_ALLOWED_TIME = 60.0f * 60.0f * 48.0f; // at most two full days of rendering
}

/// Parses the command line arguments. Returns true if the renderer should run, false if not.
inline bool parse_cmd_line(int argc, char* argv[], UserSettings& settings) {
    settings = UserSettings(); // Start with the default settings.

    if (argc < 2) {
        print_help();
        return false;
    }

    settings.input_file = argv[1];

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-q")
            settings.background = true;
        else if (arg == "-s") {
            parse_argument(++i, argc, argv, settings.max_samples);
        } else if (arg == "-t") {
            parse_argument(++i, argc, argv, settings.max_time_sec);
        } else if (arg == "-a") {
            if (++i >= argc) {
                std::cout << "Too few arguments." << std::endl;
                return false;
            }
            std::string algname = argv[i];
            if (algname == "pt")
                settings.algorithm = UserSettings::PT;
            else if (algname == "bpt")
                settings.algorithm = UserSettings::BPT;
            else if (algname == "vcm")
                settings.algorithm = UserSettings::VCM;
            else {
                std::cout << "Invalid algorithm name: " << algname
                          << " Supported algorithms are: 'pt', 'bpt', and 'vcm'. Defaulting to 'pt'..." << std::endl;
                settings.algorithm = UserSettings::PT;
            }
        } else if (arg == "-w") {
            parse_argument(++i, argc, argv, settings.width);
        } else if (arg == "-h") {
            parse_argument(++i, argc, argv, settings.height);
        } else
            settings.output_file = arg;
    }

    if (settings.background && (settings.max_samples > MAX_ALLOWED_SAMPLES && settings.max_time_sec > MAX_ALLOWED_TIME)) {
        std::cout << "You need to specify a valid maximum time (-t) or maximum number of samples (-s) to use background rendering." << std::endl;
        return false;
    }

    return settings.input_file != "" && settings.output_file != "";
}

}

#endif

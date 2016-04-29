#ifndef IMBA_CMD_LINE_H
#define IMBA_CMD_LINE_H

#include <string>
#include <iostream>
#include <sstream>
#include <cfloat>
#include <climits>
#include <unordered_map>

namespace imba {

struct UserSettings {
    std::string input_file;
    std::string output_file;

    // If specified, BVH data will be written to this file.
    std::string accel_output;

    // Camera and canvas
    int width, height;
    float fov;

    // Execution properties
    int max_samples;
    float max_time_sec;
    bool background;

    float intermediate_image_time;
    std::string intermediate_image_name;

    // Algorithm settings
    enum Algorithm {
        PT,
        BPT,
        VCM,
        PPM,
        VCM_PT,
        LT
    } algorithm;

    float base_radius;
    int max_path_len;

    // Scheduler
    int concurrent_spp;
    int tile_size;
    int thread_count;
    int num_connections;

    UserSettings()
        : input_file(""), accel_output(""), output_file("render.png"), algorithm(PT),
          width(512), height(512), max_samples(INT_MAX), max_time_sec(FLT_MAX),
          background(false), fov(60.0f), base_radius(0.03f),
          max_path_len(10), concurrent_spp(1), tile_size(256), thread_count(4),
          intermediate_image_time(10.0f), intermediate_image_name(""), num_connections(1)
    {}
};

inline void print_help() {
    std::cout << "Usage: imbatracer <input_file.scene> [options]"
              << std::endl << std::endl
              << "    -q  Quiet mode, render in background without SDL preview." << std::endl
              << "    -s  Number of samples per pixel to render (default: unlimited)" << std::endl
              << "    -t  Number of seconds to run the render algorithm (default: unlimited)" << std::endl
              << "    -a  Selects which algorithm to use, 'pt', 'bpt', 'ppm', 'lt', 'vcm_pt', or 'vcm' (default: pt)" << std::endl
              << "    -w  Sets the horizontal resolution in pixels (default: 512)" << std::endl
              << "    -h  Sets the vertical resolution in pixels (default: 512)" << std::endl
              << "    -f  Sets the horizontal field of view (default: 60)" << std::endl
              << "    -r  Sets the initial radius for photon mapping as a factor of the scene bounding sphere radius (default: 0.03)" << std::endl
              << "    -c  Sets the number of vertices form the light path that any vertex on a camera path is connected to (default: 1)" << std::endl
              << "    --write-accel <filename>   Writes the acceleration structure to the specified file." << std::endl
              << "    --max-path-len <len>       Specifies the maximum number of vertices within any path. (default: 10)" << std::endl
              << "    --spp <nr>                 Specifies the number of samples per pixel within a single frame. (default: 1)" << std::endl
              << "    --tile-size <size>         Specifies the size of the rectangular tiles. (default: 256)" << std::endl
              << "    --thread-count <nr>        Specifies the number of threads for processing tiles. (default: 4)" << std::endl
              << "    --intermediate-time <sec>  Specifies the rate in seconds at which to store intermediate results. (default: 10)" << std::endl
              << "    --intermediate-path <path> When given, store intermediate results with filename starting with <path>. (default: not given)" << std::endl
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

    std::unordered_map<std::string, UserSettings::Algorithm> supported_algs = {
        {"pt", UserSettings::PT},
        {"bpt", UserSettings::BPT},
        {"vcm", UserSettings::VCM},
        {"lt", UserSettings::LT},
        {"ppm", UserSettings::PPM},
        {"vcm_pt", UserSettings::VCM_PT}
    };

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-q")
            settings.background = true;
        else if (arg == "-a") {
            if (++i >= argc) {
                std::cout << "Too few arguments." << std::endl;
                return false;
            }
            std::string algname = argv[i];

            auto alg_iter = supported_algs.find(algname);
            if (alg_iter == supported_algs.end()) {
                std::cout << "Invalid algorithm name: " << algname
                          << " Supported algorithms are: 'pt', 'bpt', 'ppm', 'lt', 'vcm_pt', and 'vcm'. Defaulting to 'pt'..." << std::endl;
                settings.algorithm = UserSettings::PT;
            } else {
                settings.algorithm = alg_iter->second;
            }
        } else if (arg == "--write-accel"){
            if (++i >= argc) {
                std::cout << "Too few arguments." << std::endl;
                return false;
            }

            settings.accel_output = argv[i];
        }
        else if (arg == "-s")
            parse_argument(++i, argc, argv, settings.max_samples);
        else if (arg == "-t")
            parse_argument(++i, argc, argv, settings.max_time_sec);
        else if (arg == "-w")
            parse_argument(++i, argc, argv, settings.width);
        else if (arg == "-h")
            parse_argument(++i, argc, argv, settings.height);
        else if (arg == "--max-path-len")
            parse_argument(++i, argc, argv, settings.max_path_len);
        else if (arg == "--spp")
            parse_argument(++i, argc, argv, settings.concurrent_spp);
        else if (arg == "--tile-size")
            parse_argument(++i, argc, argv, settings.tile_size);
        else if (arg == "--thread-count")
            parse_argument(++i, argc, argv, settings.thread_count);
        else if (arg == "-f")
            parse_argument(++i, argc, argv, settings.fov);
        else if (arg == "-r")
            parse_argument(++i, argc, argv, settings.base_radius);
        else if (arg == "-c")
            parse_argument(++i, argc, argv, settings.num_connections);
        else if (arg == "--intermediate-time")
            parse_argument(++i, argc, argv, settings.intermediate_image_time);
        else if (arg == "--intermediate-path")
            parse_argument(++i, argc, argv, settings.intermediate_image_name);
        else if (arg[0] == '-')
            std::cout << "Unknown argument ignored: " << arg << std::endl;
        else
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

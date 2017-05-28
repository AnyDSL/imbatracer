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

    enum TraversalPlatform {
        gpu,
        cpu,
        hybrid
    } traversal_platform;

    // If specified, BVH data will be written to this file.
    std::string accel_output;

    // Camera and canvas
    unsigned int width, height;
    float fov;

    float gamma;

    // Execution properties
    unsigned int max_samples;
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
        LT,
        PHOTON_VIS,

        DEF_VCM
    } algorithm;

    float radius_factor;
    unsigned int num_knn;
    unsigned int max_path_len;
    unsigned int light_path_count;

    // Scheduler
    unsigned int concurrent_spp;
    unsigned int tile_size;
    unsigned int thread_count;
    unsigned int num_connections;
    unsigned int q_size;

    UserSettings()
        : input_file("")
        , accel_output("")
        , output_file("render.png")
        , algorithm(PT)
        , width(512), height(512)
        , max_samples(INT_MAX), max_time_sec(FLT_MAX)
        , background(false)
        , fov(60.0f)
        , radius_factor(2.0f)
        , max_path_len(25)
        , light_path_count(512 * 512 / 2)
        , concurrent_spp(1), tile_size(256), thread_count(4)
        , intermediate_image_time(10.0f), intermediate_image_name("")
        , num_connections(1)
        , traversal_platform(cpu)
        , gamma(0.5f)
        , num_knn(10)
        , q_size(256 * 256)
    {}
};

inline void print_help() {
    std::cout << "Usage: imbatracer <input_file.scene> [options]"
              << std::endl << std::endl
              << "    -q  Quiet mode, render in background without SDL preview." << std::endl
              << "    -s  Number of samples per pixel to render (default: unlimited)" << std::endl
              << "    -t  Number of seconds to run the render algorithm (default: unlimited)" << std::endl
              << "    -a  Selects which algorithm to use: 'pt', 'bpt', 'ppm', 'lt', 'vcm_pt', 'vcm', 'photon_vis', 'def_vcm', or 'vcm_dbg' (default: pt)" << std::endl
              << "    -w  Sets the horizontal resolution in pixels (default: 512)" << std::endl
              << "    -h  Sets the vertical resolution in pixels (default: 512)" << std::endl
              << "    -f  Sets the horizontal field of view (default: 60)" << std::endl
              << "    -r  Sets the initial radius for photon mapping as a factor of the approx. pixel size (default: 2)" << std::endl
              << "    -c  Sets the number of vertices form the light path that any vertex on a camera path is connected to (default: 1)" << std::endl
              << "    -k  Sets the number of photons to use for density estimation (default: 10)" << std::endl
              << "    --gamma   Sets the gamma correction value (default: 0.5)"
              << "    --gpu     Enables GPU traversal (default)" << std::endl
              << "    --cpu     Enables CPU traversal" << std::endl
              << "    --hybrid  Enables hybrid traversal (not yet implemented)" << std::endl
              << "    --queue-size <size>        Specifies the maximum number of rays per queue. (default: 256 * 256)" << std::endl
              << "    --write-accel <filename>   Writes the acceleration structure to the specified file." << std::endl
              << "    --max-path-len <len>       Specifies the maximum number of vertices within any path. (default: 25)" << std::endl
              << "    --light-path-count <nr>    Specifies the number of light paths to be traced per frame. (default: width * height * 0.5)" << std::endl
              << "    --spp <nr>                 Specifies the number of samples per pixel within a single frame. (default: 1)" << std::endl
              << "    --tile-size <size>         Specifies the size of the rectangular tiles. (default: 256)" << std::endl
              << "    --thread-count <nr>        Specifies the number of threads for processing tiles. (default: 4)" << std::endl
              << "    --intermediate-time <sec>  Specifies the rate in seconds at which to store intermediate results. (default: 10)" << std::endl
              << "    --intermediate-path <path> When given, store intermediate results with filename starting with <path>. (default: not given)" << std::endl
              << "  If time (-t) and number of samples (-s) are both given, rendering will be stopped once either of the two has been reached." << std::endl;
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
        {"pt",         UserSettings::PT},
        {"bpt",        UserSettings::BPT},
        {"vcm",        UserSettings::VCM},
        {"lt",         UserSettings::LT},
        {"ppm",        UserSettings::PPM},
        {"vcm_pt",     UserSettings::VCM_PT},
        {"photon_vis", UserSettings::PHOTON_VIS},
        {"def_vcm",    UserSettings::DEF_VCM},
    };

    bool lp_count_given = false;

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
                          << " Supported algorithms are: 'pt', 'bpt', 'ppm', 'lt', 'vcm_pt', 'def_vcm', and 'vcm'. Defaulting to 'pt'..." << std::endl;
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
            parse_argument(++i, argc, argv, settings.radius_factor);
        else if (arg == "-c")
            parse_argument(++i, argc, argv, settings.num_connections);
        else if (arg == "-k")
            parse_argument(++i, argc, argv, settings.num_knn);
        else if (arg == "--intermediate-time")
            parse_argument(++i, argc, argv, settings.intermediate_image_time);
        else if (arg == "--intermediate-path")
            parse_argument(++i, argc, argv, settings.intermediate_image_name);
        else if (arg == "--gpu")
            settings.traversal_platform = UserSettings::gpu;
        else if (arg == "--cpu")
            settings.traversal_platform = UserSettings::cpu;
        else if (arg == "--hybrid")
            settings.traversal_platform = UserSettings::hybrid;
        else if (arg == "--gamma")
            parse_argument(++i, argc, argv, settings.gamma);
        else if (arg == "--queue-size")
            parse_argument(++i, argc, argv, settings.q_size);
        else if (arg == "--light-path-count") {
            parse_argument(++i, argc, argv, settings.light_path_count);
            lp_count_given = true;
        }
        else if (arg[0] == '-')
            std::cout << "Unknown argument ignored: " << arg << std::endl;
        else
            settings.output_file = arg;
    }

    if (settings.background && (settings.max_samples > MAX_ALLOWED_SAMPLES && settings.max_time_sec > MAX_ALLOWED_TIME)) {
        std::cout << "You need to specify a valid maximum time (-t) or maximum number of samples (-s) to use background rendering." << std::endl;
        return false;
    }

    if (settings.num_connections < 1 || settings.num_connections > 8) {
        std::cout << "Number of connections has to be in [1,8]. Using default value one." << std::endl;
        settings.num_connections = 1;
    }

    if (!lp_count_given) {
        settings.light_path_count = (settings.width * settings.height) >> 1;
    }

    return settings.input_file != "" && settings.output_file != "";
}

}

#endif

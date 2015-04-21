#include <chrono>
#include <random>
#include "common/options.hpp"
#include "common/logger.hpp"
#include "common/path.hpp"
#include "loaders/scene_loader.hpp"
#include "loaders/image_loader.hpp"
#include "loaders/obj_loader.hpp"
#include "loaders/png_loader.hpp"
#include "loaders/tga_loader.hpp"
#include "scene/scene.hpp"
#include "scene/render.hpp"
#include "devices/png_device.hpp"
#include "devices/sdl_device.hpp"

extern "C" {
    void put_int(int i) {
        printf("%d\n", i);    
    }

    void put_float(float f) {
        printf("%f\n", f);    
    }

    void debug_abort(const char* msg) {
        printf("Impala assertion failed : %s\n", msg);
        exit(1);
    }
}

int main(int argc, char** argv) {
    bool show_help;
    int image_width, image_height;
    std::string output_dev;
    std::string device_opts;

    imba::ArgParser parser(argc, argv);
    parser.add_option("help",     "h",   "Shows this message",           show_help,    false);
    parser.add_option("width",    "sx",  "Sets the output image width",  image_width,  512,                 "pixels");
    parser.add_option("height",   "sy",  "Sets the output image height", image_height, 512,                 "pixels");
    parser.add_option("device",   "dev", "Sets the output device",       output_dev,   std::string("sdl"),  "dev");
    parser.add_option("device-options", "dev-opts", "Sets the device options", device_opts, std::string(""), "opts");

    if (!parser.parse())
        return EXIT_FAILURE;

    if (show_help) {
        parser.usage();
        return EXIT_SUCCESS;
    }

    if (parser.arguments().size() == 0) {
        std::cerr << "No arguments. Exiting." << std::endl;
        return EXIT_SUCCESS;
    }

    imba::Logger logger;

    std::unique_ptr<imba::Device> device;
    if (output_dev == "png") {
        device.reset(new imba::PngDevice());
        logger.log("using png image device");
    } else if (output_dev == "sdl") {
        device.reset(new imba::SdlDevice());
        logger.log("using SDL window device");
    } else if (output_dev == "null") {
        logger.log("using null device");
    } else {
        logger.log("unknown device, using null device instead");
    }

    if (device && !device->parse_options(device_opts, logger)) {
        std::cerr << "Some device options were invalid. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    imba::TextureLoaderManager texture_loaders;
    texture_loaders.add_loader(new imba::PngLoader());
    texture_loaders.add_loader(new imba::TgaLoader());

    imba::SceneLoaderManager scene_loaders;
    scene_loaders.add_loader(new imba::ObjLoader(&texture_loaders));

    imba::Scene scene;

    for (const auto& arg: parser.arguments()) {
        if (!scene_loaders.load_file(imba::Path(arg), scene, &logger)) {
            std::cerr << "Cannot load file : " << arg << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (device) {
        // TODO : use a shader instead of this camera def.
        device->set_perspective(imba::Vec3(0.0f, 5.0f, 20.0f),
                                imba::Vec3(0.0f, 0.0f, 10.0f),
                                imba::Vec3(0.0f, 1.0f, 0.0f),
                                60.0f,
                                (float)image_width / (float)image_height);
        if (!device->render(scene, image_width, image_height, logger))
            std::cerr << "There was a problem when sending the image to the output device." << std::endl;
    }

    return EXIT_SUCCESS;
}


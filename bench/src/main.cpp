#include "common/options.hpp"
#include "common/logger.hpp"
#include "common/path.hpp"
#include "loaders/scene_loader.hpp"
#include "loaders/image_loader.hpp"
#include "loaders/obj_loader.hpp"
#include "loaders/png_loader.hpp"
#include "scene/scene.hpp"

int main(int argc, char** argv) {
    bool show_help;
    int image_width, image_height;
    std::string output_dev;
    std::string device_opts;

    imba::ArgParser parser(argc, argv);
    parser.add_option("help",     "h",   "Shows this message",           show_help,    false);
    parser.add_option("width",    "sx",  "Sets the output image width",  image_width,  512,                 "pixels");
    parser.add_option("height",   "sy",  "Sets the output image height", image_height, 512,                 "pixels");
    parser.add_option("device",   "dev", "Sets the output device",       output_dev,   std::string("png"),  "dev");
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
    
    imba::ImageLoaderManager image_loaders;
    image_loaders.add_loader(new imba::PngLoader());

    imba::SceneLoaderManager scene_loaders;
    scene_loaders.add_loader(new imba::ObjLoader(&image_loaders));

    imba::Scene scene;
    imba::Logger logger;

    for (const auto& arg: parser.arguments()) {
        if (!scene_loaders.load_file(imba::Path(arg), scene, &logger)) {
            std::cerr << "Cannot load file : " << arg << std::endl;
            return EXIT_FAILURE;
        }
    }
    
    /*imba::Image image(image_width, image_height);
    imba::Renderer renderer(scene, image);
    renderer.run();*/

    return EXIT_SUCCESS;
}


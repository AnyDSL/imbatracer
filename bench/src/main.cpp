#include <iostream>
#include <vector>
#include "bench/bench.hpp"
#include "bench/bench_ray_triangle.hpp"
#include "bench/bench_ray_box.hpp"
#include "bench/bench_bvh_builder.hpp"
#include "loaders/obj_loader.hpp"

// Abort function for the impala assert function
extern "C" void* debug_abort(const char* msg)
{
    printf("Impala assertion failed : %s\n", msg);
    exit(1);
}

int main(int argc, char** argv) {
    std::vector<bench::Bench*> benches;

    benches.push_back(new bench::BenchRayTriangleImpala(4000000));
    benches.push_back(new bench::BenchRay4TriangleImpala(1000000));
    benches.push_back(new bench::BenchRayTriangleEmbree(4000000));
    benches.push_back(new bench::BenchRay4TriangleEmbree(1000000));

    benches.push_back(new bench::BenchRayBoxImpala(4000000));
    benches.push_back(new bench::BenchRay4BoxImpala(1000000));

    benches.push_back(new bench::BenchBvhBuildImpala());

    imba::ObjLoader loader;
    imba::Scene scene;
    std::unique_ptr<imba::Logger> logger(new imba::Logger("log.txt"));
    if (!loader.load_scene(".", "teapot.obj", scene, logger.get())) {
        logger->log("cannot load file 'teapot.obj'");
    }

    for (auto b : benches) {
        b->run_verbose();
        std::cout << b->get_name() << " : "
                  << b->get_milliseconds() << " ms"
                  << std::endl;
    }

    for (auto b : benches) {
        delete b;
    }
}


#include <iostream>
#include <vector>

#include <embree2/rtcore.h>

#include "bench/bench.hpp"
#include "bench/bench_ray_triangle.hpp"
#include "bench/bench_ray_box.hpp"
#include "bench/bench_ray_bvh.hpp"
#include "bench/bench_bvh_build.hpp"
#include "loaders/obj_loader.hpp"

// Abort function for the impala assert function
extern "C" void* debug_abort(const char* msg)
{
    printf("Impala assertion failed : %s\n", msg);
    exit(1);
}

extern "C" void put_float(float f) {
    printf("float : %f\n", (double)f);
}

extern "C" void put_int(int i) {
    printf("int : %d\n", i);
}

struct EmbreeInit {
    EmbreeInit() {
        rtcInit("verbose=0,"
                "isa=sse4.2,"
                "tri_accel=bvh4.triangle1,"
                "threads=1");
    }
    ~EmbreeInit() { rtcExit(); }
};

template <typename F>
struct AutoCleanup {
    AutoCleanup(F f) : f_(f) {}
    ~AutoCleanup() { f_(); }
    F f_;
};

template <typename F>
AutoCleanup<F> auto_cleanup(F f) {
    return AutoCleanup<F>(f);
}

int main(int argc, char** argv) {
    std::unique_ptr<imba::Logger> logger(new imba::FileLogger("log.txt"));
    std::vector<bench::Bench*> benches;
    EmbreeInit embree_init;

    // Automatically cleanup the benches on exit
    auto cleanup = auto_cleanup([&] () {
        std::cout << "cleaning up..." << std::endl;
        for (auto b : benches) {
            delete b;
        }
    });

    benches.push_back(new bench::BenchRayTriangleImpala(4000000));
    benches.push_back(new bench::BenchRay4TriangleImpala(1000000));
    benches.push_back(new bench::BenchRayTriangleEmbree(4000000));
    benches.push_back(new bench::BenchRay4TriangleEmbree(1000000));

    benches.push_back(new bench::BenchRayBoxImpala(4000000));
    benches.push_back(new bench::BenchRay4BoxImpala(1000000));

    // Benches that need a simple scene
    const std::string& scene_file = "teapot.obj";
    imba::ObjLoader loader;
    imba::Scene scene;
    
    if (!loader.load_file(imba::Path("teapot.obj"), scene, logger.get())) {
        logger->log("cannot load file ", scene_file);
        return EXIT_FAILURE;
    }

    if (!scene.triangle_mesh_count()) {
        logger->log("file '", scene_file, "' contains no mesh");
    }

    for (int i = 0; i < scene.triangle_mesh_count(); i++) {
        const imba::TriangleMesh* mesh = scene.triangle_meshes()[i];
        benches.push_back(new bench::BenchBvhBuildImpala(mesh));
        benches.push_back(new bench::BenchBvh4BuildEmbree(mesh));

        benches.push_back(new bench::BenchRayBvh4Embree(mesh, 400000));
        benches.push_back(new bench::BenchRay4Bvh4Embree(mesh, 100000));

        benches.push_back(new bench::BenchRayBvhImpala(mesh, 400000));
        benches.push_back(new bench::BenchRay4BvhImpala(mesh, 100000));
    }

    std::cout << "starting benchmarks..." << std::endl;
    for (auto b : benches) {
        b->run_verbose();
        std::cout << b->name() << " : "
                  << b->milliseconds() << " ms"
                  << std::endl;
    }

    return EXIT_SUCCESS;
}


#include <iostream>
#include <vector>
#include "bench.hpp"
#include "bench_ray_triangle.hpp"

// Allocation function for Thorin
extern "C" void* thorin_malloc(size_t size)
{
    return malloc(size);
}

int main(int argc, char** argv) {
    std::vector<bench::Bench*> benches;

    benches.push_back(new bench::BenchRayTriangleImpala(4000000));
    benches.push_back(new bench::BenchRay4TriangleImpala(1000000));
    benches.push_back(new bench::BenchRayTriangleEmbree(4000000));
    benches.push_back(new bench::BenchRay4TriangleEmbree(1000000));

    for (auto b : benches) {
        b->run();
        std::cout << b->get_name() << " : "
                  << b->get_milliseconds() << " ms"
                  << std::endl;
    }

    for (auto b : benches) {
        delete b;
    }
}


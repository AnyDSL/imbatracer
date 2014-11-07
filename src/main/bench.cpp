#include <thorin_runtime.h>
#include <core/util.h>

#include "interface.h"

typedef impala::State *(&InitBenchFun)();

void run_bench(InitBenchFun initBenchFun, const char *name, unsigned n, int w, int h)
{
    std::cout << "Running " << name << std::endl;

    impala::State *state = initBenchFun();
    impala::impala_update(state, 0.0);

    unsigned *buf = thorin_new<unsigned>(w*h);
    for (unsigned i = 0; i < n; ++i) {
        impala::impala_render(buf, w, h, true, state);
    }
    thorin_free(buf);

    impala::impala_finish(state);

    std::cout << std::endl;
}

int main(int /*argc*/, char */*argv*/[])
{
    thorin_init();
    run_bench(impala::impala_init, "Default main, no GUI: 640x480", 10, 640, 480);
    run_bench(impala::impala_init, "Default main, no GUI: 2560x1920", 10, 640, 480);
    //run_bench(impala::impala_init_bench1, "Bench 1, Config 1", 10, 640/2, 480/2);
    //run_bench(impala::impala_init_bench1, "Bench 1, Config 2", 10, 640, 480);
    //run_bench(impala::impala_init_bench1, "Bench 1, Config 3", 10, 640*4, 480*4);

    //run_bench(impala::impala_init_bench2, "Bench 2, Config 1", 10, 640*2, 480*2);
}

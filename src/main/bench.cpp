#include <thorin_runtime.h>
#include <core/util.h>

#include "interface.h"
#include "scene.h"

typedef void (&InitBenchFun)(impala::State*);

void run_bench(InitBenchFun initBenchFun, const char *name, unsigned n, int w, int h)
{
    std::cout << "Running " << name << std::endl;

    impala::State *state = thorin_new<impala::State>(1);
    {
        rt::Scene scene(&state->scene);
        state->sceneMgr = &scene;
        initBenchFun(state);

        unsigned *buf = thorin_new<unsigned>(w*h);
        for (unsigned i = 0; i < n; ++i) {
            impala_render(buf, w, h, true, state);
        }
        thorin_free(buf);
    }
    thorin_free(state);

    std::cout << std::endl;
}

int main(int /*argc*/, char */*argv*/[])
{
    thorin_init();
    //run_bench(impala::impala_init_bench1, "Bench 1, Config 1", 10, 640/2, 480/2);
    run_bench(impala::impala_init_bench1, "Bench 1, Config 2", 10, 640, 480);
    run_bench(impala::impala_init_bench1, "Bench 1, Config 3", 10, 640*4, 480*4);
}

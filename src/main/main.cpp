#include <io/sdlbuffergui.h>
#include <io/image.h>
#include <SDL.h>
#include <thorin_ext_runtime.h>
#include <core/util.h>

#include "interface.h"
#include "scene.h"

using namespace rt;

class ImpalaGui : public SDLBufferGui
{
public:
    ImpalaGui(unsigned w, unsigned h)
        : SDLBufferGui(w, h)
    {
        state.time = 0;
    }
    
    impala::State *getState() { return &state; }

protected:

    virtual void _Render(CountedPtr<Image> img, float time)
    {
        state.time = time;
        state.cam = impala::perspectiveCam(-3.5f*sinf(time), 1.2f*sinf(0.2f*time), 3.7f*cosf(time), 0, 0, 0,
                                           0, 1.0f, 0, M_PI/4, M_PI/3);
        state.integrator.itype = 0;
        //std::cout << "Origin: " << state.cam.view.origin << ", Fwd: " << state.cam.view.forward << ", Up: " << state.cam.view.up << ", Right: " << state.cam.view.right << std::endl;
        //impala::imp_print_stuff(impala::Point(-1, -2, -3), 1.0f, 2.0f, 3.0f, impala::Vec(4.0f, 5.0f, 6.0f), 7.0f, 8.0f, 9.0f);
        {
            Timer timer("Rendering image");
            impala_render(img->getPtr(), img->width(), img->height(), &state);
        }
    }

    impala::State state;
};

int main(int /*argc*/, char */*argv*/[])
{
    thorin_init();
    SDL_Init(0);
    atexit(SDL_Quit);

    ImpalaGui gui(64, 52);
    CubeScene scene(&gui.getState()->scene);
    return gui.main();

    return 0;
}

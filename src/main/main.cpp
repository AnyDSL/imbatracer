#include <io/sdlgui.h>
#include <io/image.h>
#include <SDL.h>
#include <thorin_ext_runtime.h>
#include <core/util.h>

#include "interface.h"
#include "scene.h"

using namespace rt;

class ImpalaGui : public SDLGui
{
public:
    ImpalaGui(unsigned w, unsigned h)
        : _img(new Image(w, h)) // FIXME use w, h for window
    {
        state.time = 0;
    }
    
    impala::State *getState() { return &state; }

protected:

    virtual void _Update(float dt)
    {
        state.time += dt;
        state.cam = perspectiveCam(impala::Point(5, 0, 0), impala::Point(0, 0, 0), 0, 1.0f, 0, M_PI/4, M_PI/3);
        std::cout << "Origin: " << state.cam.view.origin << ", Fwd: " << state.cam.view.forward << ", Up: " << state.cam.view.up << ", Right: " << state.cam.view.right << std::endl;
        impala::imp_print_stuff(impala::Point(-1, -2, -3), 1.0f, 2.0f, 3.0f, impala::Vec(4.0f, 5.0f, 6.0f), 7.0f, 8.0f, 9.0f);
        {
            Timer timer("Rendering image");
            impala_render(_img->getPtr(), _img->width(), _img->height(), &state);
        }
        ShowImage(_img);
    }

    virtual void _OnWindowResize(int w, int h)
    {
        _img = new Image(w, h);
    }

    impala::State state;
    CountedPtr<Image> _img;
};

int main(int /*argc*/, char */*argv*/[])
{
    thorin_init();
    SDL_Init(0);
    atexit(SDL_Quit);

    ImpalaGui gui(64, 52);
    CubeScene scene(&gui.getState()->scene);
    gui.Init();

    gui.SetWindowTitle("ImbaTracer");
    // FIXME this kind of MT doesn't makie any sense, _Update is called in the display thread, the main thread does - nothing...
    gui.WaitForQuit();


    return 0;
}

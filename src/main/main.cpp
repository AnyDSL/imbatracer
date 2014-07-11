#include <io/sdlgui.h>
#include <io/image.h>
#include <SDL.h>
#include <stdio.h>
#include <float.h>
#include <limits.h>
#include <thorin_ext_runtime.h>

#include "interface.h"
#include "scene.h"

using namespace rt;

extern "C"
{
    void callbackTest(int x, int y)
    {
        printf("callback: (%d, %d)\n", x, y);
    }

    unsigned char *HACK_NULL()
    {
        return nullptr;
    }

    float FLT_MAX_fn()
    {
        return FLT_MAX;
    }

    void c_assert(bool cond)
    {
        if(cond)
            return;
        fprintf(stderr, "IMBA ASSERTION FAILED\n");
        debugAbort();
    }
}

class ImpalaGui : public SDLGui
{
public:
    ImpalaGui(unsigned w, unsigned h)
        : _img(new Image(w, h))
    {
        state.time = 0;
    }
    
    impala::State *getState() { return &state; }

protected:

    virtual void _Update(float dt)
    {
        state.time += dt;
        state.cam = perspectiveCam(impala::Point(2*sinf(dt), 2*cosf(1.5*dt), 2*sinf(-0.2*dt)), impala::Point(0, 0, 0));
        impala_render(_img->getPtr(), _img->width(), _img->height(), &state);
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

    ImpalaGui gui(640, 480);
    gui.Init();
    CubeScene(&gui.getState()->scene);

    gui.SetWindowTitle("ImbaTracer");
    gui.WaitForQuit();


    return 0;
}

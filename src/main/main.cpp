#include <io/sdlgui.h>
#include <io/image.h>
#include <SDL.h>
#include <stdio.h>
#include <float.h>
#include <limits.h>
#include <thorin_ext_runtime.h>

#include "interface.h"
#include "sceneload.h"

using namespace rt;
using namespace impala;

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

    void c_assert(bool cond, const char *str)
    {
        if(cond)
            return;
        fprintf(stderr, "IMBA ASSERTION FAILED: %s\n", str);
        debugAbort();
    }
}

class TestGui : public SDLGui
{
public:
    TestGui(unsigned w, unsigned h)
        : _img(new Image(w, h))
    {
        state.time = 0;
    }
    
    State *getState() { return &state; }

protected:

    virtual void _Update(float dt)
    {
        state.time += dt;
        state.cam = perspectiveCam(point(2*sinf(dt), 2*cosf(1.5*dt), 2*sinf(-0.2*dt)), point(0, 0, 0));
        impala_render(_img->getPtr(), _img->width(), _img->height(), &state);
        ShowImage(_img);
    }

    virtual void _OnWindowResize(int w, int h)
    {
        _img = new Image(w, h);
    }

    State state;
    CountedPtr<Image> _img;
};

int main(int /*argc*/, char */*argv*/[])
{
    thorin_init();
    SDL_Init(0);
    atexit(SDL_Quit);

    TestGui gui(640, 480);
    gui.Init();
    loadScene(&gui.getState()->scene);

    gui.SetWindowTitle("ImbaTracer");
    gui.WaitForQuit();


    return 0;
}

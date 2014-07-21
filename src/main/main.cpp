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
        : SDLBufferGui(w, h, "ImbaTracer"), scene(&state.scene)
    {
        // impala_init may call functions that add objects to the scene
        impala_init(&state);
    }
    virtual ~ImpalaGui() {}

    impala::State *getState() { return &state; }

protected:

    virtual void _Render(CountedPtr<Image> img, float dt)
    {
        impala_render(img->getPtr(), img->width(), img->height(), &state, dt);
    }

    impala::State state;
    rt::Scene scene;
};

int main(int /*argc*/, char */*argv*/[])
{
    thorin_init();
    SDL_Init(0);
    atexit(SDL_Quit);

    ImpalaGui gui(640/2, 480/2);
    return gui.main();

    return 0;
}

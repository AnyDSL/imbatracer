#include <io/sdlbuffergui.h>
#include <io/image.h>
#include <SDL.h>
#include <thorin_runtime.h>
#include <core/util.h>

#include "interface.h"

using namespace rt;

enum class SceneKind {
    Main,
    Bench1,
    Bench2,
};

class ImpalaGui : public SDLBufferGui
{
public:
    ImpalaGui(unsigned w, unsigned h, SceneKind sceneKind)
        : SDLBufferGui(w, h, "ImbaTracer"), sceneKind(sceneKind)
    {
        // impala_init may call functions that add objects to the scene
        switch (sceneKind) {
        case SceneKind::Main:
            state = impala::impala_init(); break;
        case SceneKind::Bench1:
            state = impala::impala_init_bench1(); break;
        case SceneKind::Bench2:
            state = impala::impala_init_bench2(); break;
        }

    }
    virtual ~ImpalaGui() {
        impala::impala_finish(state);
    }

protected:

    virtual void _Render(CountedPtr<Image> img, float dt)
    {
        if (sceneKind == SceneKind::Main)
            impala::impala_update(state, dt);
        impala::impala_render(img->getPtr(), img->width(), img->height(), false, state);
    }

    SceneKind sceneKind;
    impala::State *state;
};

int main(int argc, char *argv[])
{
    // which scene to show?
    SceneKind scene = SceneKind::Main;
    if (argc > 1 && std::string(argv[1]) == "bench1") {
        scene = SceneKind::Bench1;
    }
    else if (argc > 1 && std::string(argv[1]) == "bench2") {
        scene = SceneKind::Bench2;
    }

    // global initialisation
    thorin_init();
    SDL_Init(0);
    atexit(SDL_Quit);

    // run the thing
    ImpalaGui gui(640, 480, scene);
    return gui.main();
}

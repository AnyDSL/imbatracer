#include <io/sdlbuffergui.h>
#include <io/image.h>
#include <SDL.h>
#include <thorin_runtime.h>
#include <core/util.h>

#include "interface.h"
#include "scene.h"

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
        : SDLBufferGui(w, h, "ImbaTracer"), sceneKind(sceneKind), scene(&state.scene)
    {
        memset(&state, 0, sizeof(state));
        state.sceneMgr = &scene;
        // impala_init may call functions that add objects to the scene
        switch (sceneKind) {
        case SceneKind::Main:
            impala_init(&state); break;
        case SceneKind::Bench1:
            impala_init_bench1(&state); break;
        case SceneKind::Bench2:
            impala_init_bench2(&state); break;
        }

    }
    virtual ~ImpalaGui() {}

    impala::State *getState() { return &state; }

protected:

    virtual void _Render(CountedPtr<Image> img, float dt)
    {
        if (sceneKind == SceneKind::Main)
            impala_update(&state, dt);
        impala_render(img->getPtr(), img->width(), img->height(), false, &state);
    }

    SceneKind sceneKind;
    impala::State state;
    rt::Scene scene;
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

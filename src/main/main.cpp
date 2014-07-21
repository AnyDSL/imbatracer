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
        state.scene.sceneMgr = &scene;
        // impala_init may call functions that add objects to the scene
        impala_init(&state);
    }
    virtual ~ImpalaGui() {}

    impala::State *getState() { return &state; }

protected:

    virtual void _Render(CountedPtr<Image> img, float dt)
    {
        impala_update(&state, dt);
        impala_render(img->getPtr(), img->width(), img->height(), &state);
    }

    impala::State state;
    rt::Scene scene;
};

int main(int /*argc*/, char */*argv*/[])
{
    static_assert(std::is_pod<impala::Point>::value, "impala::Point must be a POD");
    static_assert(std::is_pod<impala::Vec>::value, "impala::Vec must be a POD");
    static_assert(std::is_pod<impala::TexCoord>::value, "impala::TexCoord must be a POD");
    static_assert(std::is_pod<impala::Object>::value, "impala::Object must be a POD");
    static_assert(std::is_pod<impala::BBox>::value, "impala::BBox must be a POD");
    static_assert(std::is_pod<impala::BVHNode>::value, "impala::BVHNode must be a POD");
    static_assert(std::is_pod<impala::Scene>::value, "impala::Scene must be a POD");
    static_assert(std::is_pod<impala::View>::value, "impala::View must be a POD");
    static_assert(std::is_pod<impala::Cam>::value, "impala::Cam must be a POD");
    static_assert(std::is_pod<impala::Integrator>::value, "impala::Integrator must be a POD");
    static_assert(std::is_pod<impala::State>::value, "impala::State must be a POD");

    thorin_init();
    SDL_Init(0);
    atexit(SDL_Quit);

    ImpalaGui gui(640/2, 480/2);
    return gui.main();

    return 0;
}

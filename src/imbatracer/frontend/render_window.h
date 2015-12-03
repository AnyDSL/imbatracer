#ifndef IMBA_RENDER_WINDOW_H
#define IMBA_RENDER_WINDOW_H

#include <SDL.h>
#include "../render/render.h"
#include "../render/integrators/pt.h"
#include "../render/integrators/bpt.h"

namespace imba {

enum class Key {
    LEFT, RIGHT, UP, DOWN, PLUS, MINUS
};

class InputController {
public:
    virtual ~InputController() {}
    virtual bool key_press(Key) {}
    virtual bool mouse_move(bool, float, float) {}
};

class RenderWindow {
public:
    RenderWindow(int width, int height, int spp, Integrator& r, InputController& ctrl);
    ~RenderWindow();

    void render_loop();

private:
    void clear();
    bool handle_events(bool flush);
    void render();

    bool write_image(const char* filename);

    Image accum_buffer_;
    SDL_Surface* screen_;
    Integrator& integrator_;
    InputController& ctrl_;

    float mouse_speed_;
    int spp_;
    int frames_;
};

} // namespace imba

#endif


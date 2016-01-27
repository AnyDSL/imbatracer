#ifndef IMBA_RENDER_WINDOW_H
#define IMBA_RENDER_WINDOW_H

#include <SDL.h>

#include "cmd_line.h"

#include "../render/render.h"
#include "../render/integrators/pt.h"
#include "../render/integrators/bpt.h"

namespace imba {

enum class Key {
    LEFT, RIGHT, UP, DOWN, PLUS, MINUS, SPACE
};

class InputController {
public:
    virtual ~InputController() {}
    virtual bool key_press(Key) { return false; }
    virtual bool mouse_move(bool, float, float) { return false; }
};

class RenderWindow {
public:
    RenderWindow(const UserSettings& settings, Integrator& r, InputController& ctrl);
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
    int frames_;

    bool use_sdl_;
    int max_samples_;
    float max_time_sec_;

    std::string output_file_;
    std::string algname_;
};

} // namespace imba

#endif


#ifndef IMBA_RENDER_WINDOW_H
#define IMBA_RENDER_WINDOW_H

#include <SDL2/SDL.h>
#include <chrono>

#include "imbatracer/frontend/cmd_line.h"
#include "imbatracer/render/integrators/integrator.h"

namespace imba {

enum class Key {
    LEFT, RIGHT, UP, DOWN, PLUS, MINUS, SPACE, BACKSPACE
};

class InputController {
public:
    virtual ~InputController() {}
    virtual bool key_press(Key) { return false; }
    virtual bool mouse_move(bool, float, float) { return false; }
};

class RenderWindow {
public:
    RenderWindow(const UserSettings& settings, Integrator& r, InputController& ctrl, int spp);
    ~RenderWindow();

    void render_loop();

private:
    void clear();
    bool handle_events();
    void render();

    bool write_image(const char* filename);

    AtomicImage accum_buffer_;
    SDL_Window* window_;
    int window_id_;
    Integrator& integrator_;
    InputController& ctrl_;

    float gamma_;

    float mouse_speed_;

    int frames_;
    std::chrono::high_resolution_clock::time_point start_time_;
    int msg_counter_;
    static constexpr int msg_interval_ms = 10000;

    int max_samples_;
    int spp_;
    float max_time_sec_;

    std::string conv_file_base_;
    float conv_interval_sec_;
    int conv_count_;

    std::string output_file_;
};

} // namespace imba

#endif


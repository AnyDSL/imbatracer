#include <iostream>
#include "SDL_device.h"
#include "thorin_runtime.h"
#include <assert.h>

imba::SDLDevice::SDLDevice(int img_width, int img_height, Render& r) 
    : image_width_(img_width), image_height_(img_height), render_(r), n_samples_(0), img_(img_width, img_height)
{
    srand(452361532);
#pragma omp parallel for
    for (int y = 0; y < screen_->h; y++) {
        float* buf_row_all = img_.pixels() + y * image_width_ * 4;
        for (int x = 0; x < screen_->w; x++) {
            buf_row_all[x * 4] = 0.0f;
            buf_row_all[x * 4 + 1] = 0.0f;
            buf_row_all[x * 4 + 2] = 0.0f;
            buf_row_all[x * 4 + 3] = 0.0f;
        }
    }   

    SDL_Init(SDL_INIT_VIDEO);
}

imba::SDLDevice::~SDLDevice() {
    SDL_Quit();
}

void imba::SDLDevice::render() {
    SDL_WM_SetCaption("Imbatracer", NULL);

    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    //SDL_WM_GrabInput(SDL_GRAB_ON);
    //SDL_ShowCursor(SDL_DISABLE);

    screen_ = SDL_SetVideoMode(image_width_, image_height_, 32, SDL_DOUBLEBUF);

    // Flush input events (discard first mouse move event)
    handle_events(true);

    bool done = false;
    int frames = 0;
    long ticks = SDL_GetTicks();
    long t = ticks;
    long old_t;
    float dir = -1.0f;
    while (!done) {
        old_t = t;
        t = SDL_GetTicks();
        float ftime = (t - old_t) / 1000.0f;
        if (t - ticks > 5000) {
            std::cout << 1000 * frames / (t - ticks) << " frames per second" << std::endl;
            frames = 0;
            ticks = t;
        }
        
        render_surface();

        SDL_Flip(screen_);
        done = handle_events(false);

        frames++;
    }

    //SDL_WM_GrabInput(SDL_GRAB_OFF);
}

inline float clamp(float x, float a, float b)
{
    return x < a ? a : (x > b ? b : x);
}

void imba::SDLDevice::render_surface() {
    Image& tex = render_();
        
    SDL_LockSurface(screen_);
    const int r = screen_->format->Rshift / 8;
    const int g = screen_->format->Gshift / 8;
    const int b = screen_->format->Bshift / 8;

#pragma omp parallel for
    for (int y = 0; y < screen_->h; y++) {
        unsigned char* row = (unsigned char*)screen_->pixels + screen_->pitch * y;
        const float* buf_row_cur = tex.pixels() + y * image_width_ * 4;
        float* buf_row_all = img_.pixels() + y * image_width_ * 4;
        
        for (int x = 0; x < screen_->w; x++) {
            buf_row_all[x * 4] = (buf_row_all[x * 4] * n_samples_ + buf_row_cur[x * 4]) / (n_samples_ + 1);
            buf_row_all[x * 4 + 1] = (buf_row_all[x * 4 + 1] * n_samples_ + buf_row_cur[x * 4 + 1]) / (n_samples_ + 1);
            buf_row_all[x * 4 + 2] = (buf_row_all[x * 4 + 2] * n_samples_ + buf_row_cur[x * 4 + 2]) / (n_samples_ + 1);
        
            row[x * 4 + r] = 255.0f * clamp(buf_row_all[x * 4], 0.0f, 1.0f);
            row[x * 4 + g] = 255.0f * clamp(buf_row_all[x * 4 + 1], 0.0f, 1.0f);
            row[x * 4 + b] = 255.0f * clamp(buf_row_all[x * 4 + 2], 0.0f, 1.0f);
        }
    }
    SDL_UnlockSurface(screen_);
    
    ++n_samples_;
}

bool imba::SDLDevice::handle_events(bool flush) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (flush) continue;
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        return true;
                }
                break;

            case SDL_QUIT:
                return true;
            default:
                break;
        }
    }

    return false;
}

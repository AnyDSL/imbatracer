#include <iostream>
#include "SDL_device.h"
#include "thorin_runtime.h"

imba::SDLDevice::SDLDevice(int img_width, int img_height) 
    : image_width_(img_width), image_height_(img_height) 
{
    tex_.width = image_width_;
    tex_.height = image_height_;
    tex_.pixels = thorin_new<Vec4>(image_width_ * image_height_);
    
    prim_queue_.count = image_width_ * image_height_;
    prim_queue_.data.org_x = thorin_new<float>(prim_queue_.count);
    prim_queue_.data.org_y = thorin_new<float>(prim_queue_.count);
    prim_queue_.data.org_z = thorin_new<float>(prim_queue_.count);
    prim_queue_.data.dir_x = thorin_new<float>(prim_queue_.count);
    prim_queue_.data.dir_y = thorin_new<float>(prim_queue_.count);
    prim_queue_.data.dir_z = thorin_new<float>(prim_queue_.count);
    prim_queue_.data.tri = thorin_new<int>(prim_queue_.count);
    prim_queue_.data.tmin = thorin_new<float>(prim_queue_.count);
    prim_queue_.data.tmax = thorin_new<float>(prim_queue_.count);
    prim_queue_.data.u = thorin_new<float>(prim_queue_.count);
    prim_queue_.data.v = thorin_new<float>(prim_queue_.count);
    
    sec_queue_.count = image_width_ * image_height_;
    sec_queue_.data.org_x = thorin_new<float>(sec_queue_.count);
    sec_queue_.data.org_y = thorin_new<float>(sec_queue_.count);
    sec_queue_.data.org_z = thorin_new<float>(sec_queue_.count);
    sec_queue_.data.dir_x = thorin_new<float>(sec_queue_.count);
    sec_queue_.data.dir_y = thorin_new<float>(sec_queue_.count);
    sec_queue_.data.dir_z = thorin_new<float>(sec_queue_.count);
    sec_queue_.data.tri = thorin_new<int>(sec_queue_.count);
    sec_queue_.data.tmin = thorin_new<float>(sec_queue_.count);
    sec_queue_.data.tmax = thorin_new<float>(sec_queue_.count);
    sec_queue_.data.u = thorin_new<float>(sec_queue_.count);
    sec_queue_.data.v = thorin_new<float>(sec_queue_.count);
    
    SDL_Init(SDL_INIT_VIDEO);
}

imba::SDLDevice::~SDLDevice() {
    SDL_Quit();
}

void imba::SDLDevice::render(Scene& scene, Accel& accel) {
    SDL_WM_SetCaption("Imbatracer", NULL);

    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    SDL_WM_GrabInput(SDL_GRAB_ON);
    SDL_ShowCursor(SDL_DISABLE);

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
        
        // TEST CODE
        scene.hemi_lights[0].pos.values[1] += dir * ftime * 3.0f;
        if (scene.hemi_lights[0].pos.values[1] < -5.0f){
            scene.hemi_lights[0].pos.values[1] = -5.0f;
            dir = 1.0f;
        } else if (scene.hemi_lights[0].pos.values[1] > 5.0f) {
            scene.hemi_lights[0].pos.values[1] = 5.0f;
            dir = -1.0f;
        }
        // END TEST CODE
        
        render_surface(scene, accel);

        SDL_Flip(screen_);
        done = handle_events(false);

        frames++;
    }

    SDL_WM_GrabInput(SDL_GRAB_OFF);
}

inline float clamp(float x, float a, float b)
{
    return x < a ? a : (x > b ? b : x);
}

void imba::SDLDevice::render_surface(Scene& scene, Accel& accel) {
    render_scene(&scene, &accel, &tex_, 1, &prim_queue_, &sec_queue_);
        
    SDL_LockSurface(screen_);
    const int r = screen_->format->Rshift / 8;
    const int g = screen_->format->Gshift / 8;
    const int b = screen_->format->Bshift / 8;

#pragma omp parallel for
    for (int y = 0; y < screen_->h; y++) {
        unsigned char* row = (unsigned char*)screen_->pixels + screen_->pitch * y;
        const Vec4* buf_row = tex_.pixels + y * image_width_;
        
        for (int x = 0; x < screen_->w; x++) {
            row[x * 4 + r] = 255.0f * clamp(buf_row[x].values[0], 0.0f, 1.0f);
            row[x * 4 + g] = 255.0f * clamp(buf_row[x].values[1], 0.0f, 1.0f);
            row[x * 4 + b] = 255.0f * clamp(buf_row[x].values[2], 0.0f, 1.0f);
        }
    }
    SDL_UnlockSurface(screen_);
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

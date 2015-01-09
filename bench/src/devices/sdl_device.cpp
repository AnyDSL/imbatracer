#include "sdl_device.hpp"
#include "../scene/render.hpp"

namespace imba {

SdlDevice::SdlDevice()
    : screen_(nullptr)
{
    SDL_Init(SDL_INIT_VIDEO);
}

SdlDevice::~SdlDevice() {
    SDL_Quit();
}

bool SdlDevice::render(const Scene& scene, int width, int height, Logger& logger) {
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        logger.log("could not initialize SDL");
        return false;
    }

    scene.compile();

    gbuffer_.resize((width % 2 != 0) ? width + 1 : width,
                    (height % 2 != 0) ? height + 1 : height);
    
    SDL_WM_SetCaption("Imbatracer", NULL);

    screen_ = SDL_SetVideoMode(width, height, 32, SDL_DOUBLEBUF);
    if (!screen_) {
        logger.log("unable to set video mode");
        return false;
    }

    bool done = false;
    int frames = 0;
    long ticks = SDL_GetTicks();
    while (!done) {
        long t = SDL_GetTicks();
        if (t - ticks > 5000) {
            logger.log(1000 * frames / (t - ticks), " frames per second");
            frames = 0;
            ticks = t;
        }

        render_surface(scene);
        SDL_Flip(screen_);
        done = handle_events();
        frames++;
    }

    return true;
}

void SdlDevice::render_surface(const Scene& scene) {
    imba::Render::render_gbuffer(scene, cam_, gbuffer_);

    SDL_LockSurface(screen_);
    const int r = screen_->format->Rshift / 8;
    const int g = screen_->format->Gshift / 8;
    const int b = screen_->format->Bshift / 8;
#pragma omp parallel for
    for (int y = 0; y < screen_->h; y++) {
        unsigned char* row = (unsigned char*)screen_->pixels + screen_->pitch * y;
        const GBufferPixel* buf_row = gbuffer_.row(y);
        for (int x = 0; x < screen_->w; x++) {
            row[x * 4 + r] = 255 * buf_row[x].t;
            row[x * 4 + g] = 255 * buf_row[x].u;
            row[x * 4 + b] = 255 * buf_row[x].v;
        }
    }
    SDL_UnlockSurface(screen_);
}

bool SdlDevice::handle_events() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
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


void SdlDevice::set_perspective(const Vec3& eye, const Vec3& center, const Vec3& up, float fov, float ratio) {
    cam_ = Render::perspective_camera(eye, center, up, fov, ratio);
}

} // namespace imba


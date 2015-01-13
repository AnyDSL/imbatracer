#include "sdl_device.hpp"
#include "../scene/render.hpp"

namespace imba {

SdlDevice::SdlDevice()
    : screen_(nullptr), speed_(0.005f)
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

    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    SDL_WM_GrabInput(SDL_GRAB_ON);
    SDL_ShowCursor(SDL_DISABLE);

    screen_ = SDL_SetVideoMode(width, height, 32, SDL_DOUBLEBUF);
    if (!screen_) {
        logger.log("unable to set video mode");
        return false;
    }

    // Flush input events (discard first mouse move event)
    handle_events(true);

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
        done = handle_events(false);
        frames++;
    }

    SDL_WM_GrabInput(SDL_GRAB_OFF);

    return true;
}

void SdlDevice::render_surface(const Scene& scene) {
    ::Camera cam = Render::perspective_camera(eye_, eye_ + dist_ * forward_, up_, fov_, ratio_);
    imba::Render::render_gbuffer(scene, cam, gbuffer_);

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

bool SdlDevice::handle_events(bool flush) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (flush) continue;
        switch (event.type) {
            case SDL_MOUSEMOTION:
                {
                    right_ = cross(forward_, up_);
                    forward_ = rotate(forward_, right_, -event.motion.yrel * speed_);
                    forward_ = rotate(forward_, up_,    -event.motion.xrel * speed_);
                    forward_ = normalize(forward_);
                    up_ = normalize(cross(right_, forward_));
                }
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_UP:    eye_ = eye_ + forward_; break;
                    case SDLK_DOWN:  eye_ = eye_ - forward_; break;
                    case SDLK_LEFT:  eye_ = eye_ - right_;   break;
                    case SDLK_RIGHT: eye_ = eye_ + right_;   break;
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
    fov_ = fov;
    ratio_ = ratio;
    up_ = normalize(up);
    forward_ = center - eye;
    dist_ = length(forward_);
    forward_ = forward_ / dist_;
    eye_ = eye;
}

} // namespace imba


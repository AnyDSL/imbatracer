#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>

#include <unistd.h>
#include <png.h>

#include "render_window.h"

namespace imba {

RenderWindow::RenderWindow(int width, int height, int spp, Integrator& r, InputController& ctrl)
    : accum_buffer_(width, height)
    , integrator_(r)
    , ctrl_(ctrl)
    , spp_(spp)
    , mouse_speed_(0.01f)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_WM_SetCaption("Imbatracer", NULL);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    screen_ = SDL_SetVideoMode(width, height, 32, SDL_DOUBLEBUF);
    clear();
}

RenderWindow::~RenderWindow() {
    SDL_Quit();
}

void RenderWindow::render_loop() {
    // Flush input events (discard first mouse move event)
    handle_events(true);

    bool done = false;
    int frames = 0;
    long ticks = SDL_GetTicks();
    long t = ticks;

    while (!done) {
        t = SDL_GetTicks();

        if (t - ticks > 5000) {
            std::cout << 1000.0 * frames / (t - ticks) << " frames per second"
                      << ", " << spp_ << " samples per pixel, "
                      << (t - ticks) / frames << "ms per frame" << std::endl;
            frames = 0;
            ticks = t;
        }

        render();
        done = handle_events(false);

        frames++;
    }

    std::stringstream file_name;
    file_name << "render_" << spp_ * frames_ << "_samples.png";
    write_image(file_name.str().c_str());
}

void RenderWindow::render() {
    integrator_.render(accum_buffer_);
    frames_++;

    SDL_LockSurface(screen_);

    const int r = screen_->format->Rshift / 8;
    const int g = screen_->format->Gshift / 8;
    const int b = screen_->format->Bshift / 8;
    const float weight = 1.0f / (spp_ * frames_);
    const float gamma = 0.854f;

#pragma omp parallel for
    for (int y = 0; y < screen_->h; y++) {
        unsigned char* row = (unsigned char*)screen_->pixels + screen_->pitch * y;
        const float4* accum_row = accum_buffer_.row(y);

        for (int x = 0; x < screen_->w; x++) {
            row[x * 4 + r] = 255.0f * clamp(powf(accum_row[x].x * weight, gamma), 0.0f, 1.0f);
            row[x * 4 + g] = 255.0f * clamp(powf(accum_row[x].y * weight, gamma), 0.0f, 1.0f);
            row[x * 4 + b] = 255.0f * clamp(powf(accum_row[x].z * weight, gamma), 0.0f, 1.0f);
        }
    }

    SDL_UnlockSurface(screen_);
    SDL_Flip(screen_);
}

bool RenderWindow::handle_events(bool flush) {
    SDL_Event event;
    bool update = false;

    if (flush) {
        while (SDL_PollEvent(&event));
        return false;
    }

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_UP:       update |= ctrl_.key_press(Key::UP);    break;
                    case SDLK_DOWN:     update |= ctrl_.key_press(Key::DOWN);  break;
                    case SDLK_LEFT:     update |= ctrl_.key_press(Key::LEFT);  break;
                    case SDLK_RIGHT:    update |= ctrl_.key_press(Key::RIGHT); break;
                    case SDLK_KP_PLUS:  update |= ctrl_.key_press(Key::PLUS);  break;
                    case SDLK_KP_MINUS: update |= ctrl_.key_press(Key::MINUS); break;
                    case SDLK_SPACE:    update |= ctrl_.key_press(Key::SPACE); break;
                    case SDLK_ESCAPE:   return true;
                    default: break;
                }
                break;

            case SDL_MOUSEMOTION:
                update |= ctrl_.mouse_move(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT),
                                           -event.motion.yrel * mouse_speed_, -event.motion.xrel * mouse_speed_);
                break;

            case SDL_QUIT:
                return true;

            default:
                break;
        }
    }

    if (update)
        clear();

    return false;
}

void RenderWindow::clear() {
    frames_ = 0;
#pragma omp parallel for
    for (int y = 0; y < accum_buffer_.height(); y++) {
        float4* accum_row = accum_buffer_.row(y);
        for (int x = 0; x < accum_buffer_.width(); x++) {
            accum_row[x] = float4(0.0);
        }
    }
}

static void write_to_stream(png_structp png_ptr, png_bytep data, png_size_t length) {
    png_voidp a = png_get_io_ptr(png_ptr);
    ((std::ostream*)a)->write((const char*)data, length);
}

void flush_stream(png_structp png_ptr) {
    // Nothing to do
}

bool RenderWindow::write_image(const char* file_name) {
    std::ofstream file(file_name, std::ofstream::binary);
    if (!file)
        return false;

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return false;
    }

    std::unique_ptr<png_byte[]> row;
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }

    png_set_write_fn(png_ptr, &file, write_to_stream, flush_stream);

    png_set_IHDR(png_ptr, info_ptr, accum_buffer_.width(), accum_buffer_.height(),
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    row.reset(new png_byte[4 * accum_buffer_.width()]);

    const float weight = 1.0f / (spp_ * frames_);
    for (int y = 0; y < accum_buffer_.height(); y++) {
        const float4* accum_row = accum_buffer_.row(y);
        png_bytep img_row = row.get();
        for (int x = 0; x < accum_buffer_.width(); x++) {
            img_row[x * 4 + 0] = (png_byte)(255.0f * clamp(accum_row[x].x * weight, 0.0f, 1.0f));
            img_row[x * 4 + 1] = (png_byte)(255.0f * clamp(accum_row[x].y * weight, 0.0f, 1.0f));
            img_row[x * 4 + 2] = (png_byte)(255.0f * clamp(accum_row[x].z * weight, 0.0f, 1.0f));
            img_row[x * 4 + 3] = (png_byte)(255.0f);
        }
        png_write_row(png_ptr, row.get());
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return true;
}

} // namespace imba

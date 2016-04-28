#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>

#include <unistd.h>
#include <png.h>

#include "render_window.h"

namespace imba {

constexpr float gamma = 1.0f / 2.0f;

struct InitOnce {
    InitOnce() { SDL_Init(SDL_INIT_VIDEO); };
    ~InitOnce() { SDL_Quit(); }
};

static InitOnce init_once;

RenderWindow::RenderWindow(const UserSettings& settings, Integrator& r, InputController& ctrl, int spp)
    : accum_buffer_(settings.width, settings.height)
    , integrator_(r)
    , ctrl_(ctrl)
    , mouse_speed_(0.01f)
    , window_(nullptr)
    , max_samples_(settings.max_samples)
    , max_time_sec_(settings.max_time_sec)
    , output_file_(settings.output_file)
    , spp_(spp)
    , conv_interval_sec_(settings.intermediate_image_time)
    , conv_file_base_(settings.intermediate_image_name)
{
    if (!settings.background) {
        window_ = SDL_CreateWindow("Imbatracer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, settings.width, settings.height, 0);
        SDL_GetWindowSurface(window_); // Creates a surface for the window
    }
    clear();
}

RenderWindow::~RenderWindow() {
    if (window_) SDL_DestroyWindow(window_);
}

void RenderWindow::render_loop() {
    typedef std::chrono::high_resolution_clock clock_type;
    std::chrono::time_point<clock_type> msg_time = clock_type::now();
    std::chrono::time_point<clock_type> cur_time = msg_time;

    while (true) {
        render();

        cur_time = clock_type::now();

        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - start_time_).count();
        const auto msg_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - msg_time).count();
        const auto avg_frame_time = frames_ == 0 ? static_cast<float>(elapsed_ms) : static_cast<float>(elapsed_ms) / frames_;
        if (msg_ms > msg_interval_ms && frames_ > 0) {
            std::cout << frames_ * spp_ << " samples, "
                      << 1000.0f * frames_ / static_cast<float>(elapsed_ms) << " frames per second, "
                      << avg_frame_time << "ms per frame"
                      << std::endl;
            msg_time = cur_time;
        }

        if ((window_ && handle_events()) ||
            (frames_ + 1) * spp_ > max_samples_ ||
            (elapsed_ms + avg_frame_time * 0.5f) / 1000.0f > max_time_sec_) // Allow only 50% average frame time more than specified.
            break;

        if (conv_file_base_ != "" && elapsed_ms / static_cast<int>(1000 * conv_interval_sec_) >= conv_count_) {
            ++conv_count_;
            std::stringstream str;
            str << conv_file_base_ << elapsed_ms << "ms" << ".png";
            write_image(str.str().c_str());
        }
    }

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - start_time_).count();
    std::cout << "Done after " << elapsed_ms / 1000.0f << " seconds, "
              << frames_ * spp_ << " samples @ "
              << 1000.0f * frames_ / static_cast<float>(elapsed_ms) << " frames per second, "
              << static_cast<float>(elapsed_ms) / frames_ << "ms per frame"  << std::endl;

    write_image(output_file_.c_str());
}

void RenderWindow::render() {
    integrator_.render(accum_buffer_);
    frames_++;

    if (!window_) return;

    SDL_Surface* surface = SDL_GetWindowSurface(window_);
    SDL_LockSurface(surface);

    const int r = surface->format->Rshift / 8;
    const int g = surface->format->Gshift / 8;
    const int b = surface->format->Bshift / 8;
    const float weight = 1.0f / (frames_ * spp_);

    #pragma omp parallel for
    for (int y = 0; y < surface->h; y++) {
        unsigned char* row = (unsigned char*)surface->pixels + surface->pitch * y;
        const auto* accum_row = accum_buffer_.row(y);

        for (int x = 0; x < surface->w; x++) {
            row[x * 4 + r] = 255.0f * clamp(powf(accum_row[x].x * weight, gamma), 0.0f, 1.0f);
            row[x * 4 + g] = 255.0f * clamp(powf(accum_row[x].y * weight, gamma), 0.0f, 1.0f);
            row[x * 4 + b] = 255.0f * clamp(powf(accum_row[x].z * weight, gamma), 0.0f, 1.0f);
        }
    }

    SDL_UnlockSurface(surface);
    SDL_UpdateWindowSurface(window_);
}

bool RenderWindow::handle_events() {
    SDL_Event event;
    bool update = false;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_UP:        update |= ctrl_.key_press(Key::UP);        break;
                    case SDLK_DOWN:      update |= ctrl_.key_press(Key::DOWN);      break;
                    case SDLK_LEFT:      update |= ctrl_.key_press(Key::LEFT);      break;
                    case SDLK_RIGHT:     update |= ctrl_.key_press(Key::RIGHT);     break;
                    case SDLK_KP_PLUS:   update |= ctrl_.key_press(Key::PLUS);      break;
                    case SDLK_KP_MINUS:  update |= ctrl_.key_press(Key::MINUS);     break;
                    case SDLK_SPACE:     update |= ctrl_.key_press(Key::SPACE);     break;
                    case SDLK_BACKSPACE: update |= ctrl_.key_press(Key::BACKSPACE); break;
                    case SDLK_ESCAPE:    return true;
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
    accum_buffer_.clear();

    // Reset number of samples and start time
    frames_ = 0;
    std::chrono::high_resolution_clock clock;
    start_time_ = clock.now();
    msg_counter_ = 1;
    conv_count_ = 0;

    integrator_.reset();
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

    const float weight = 1.0f / (frames_ * spp_);
    for (int y = 0; y < accum_buffer_.height(); y++) {
        const auto* accum_row = accum_buffer_.row(y);
        png_bytep img_row = row.get();
        for (int x = 0; x < accum_buffer_.width(); x++) {
            img_row[x * 4 + 0] = (png_byte)(255.0f * clamp(powf(accum_row[x].x * weight, gamma), 0.0f, 1.0f));
            img_row[x * 4 + 1] = (png_byte)(255.0f * clamp(powf(accum_row[x].y * weight, gamma), 0.0f, 1.0f));
            img_row[x * 4 + 2] = (png_byte)(255.0f * clamp(powf(accum_row[x].z * weight, gamma), 0.0f, 1.0f));
            img_row[x * 4 + 3] = (png_byte)(255.0f);
        }
        png_write_row(png_ptr, row.get());
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return true;
}

} // namespace imba

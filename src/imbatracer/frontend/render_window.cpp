#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>

#include <unistd.h>
#include <png.h>

#include "render_window.h"

namespace imba {

RenderWindow::RenderWindow(int img_width, int img_height, int n_samples, Renderer<StateType>& r) 
    : image_width_(img_width), image_height_(img_height), render_(r), n_samples_(n_samples), img_(img_width, img_height), n_sample_frames_(0)
{
    SDL_Init(SDL_INIT_VIDEO);
    
#pragma omp parallel for
    for (int y = 0; y < image_height_; y++) {
        float4* buf_row = img_.row(y);
        for (int x = 0; x < image_width_; x++) {
            buf_row[x] = float4(0.0);
        }
    }   
}

RenderWindow::~RenderWindow() {
    SDL_Quit();
}

void RenderWindow::render() {
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
            std::cout << 1000 * frames / (t - ticks) << " frames per second" << ", " << n_samples_ << " samples per pixel, " << 
                (t-ticks) / frames << "ms per frame" << std::endl;
            frames = 0;
            ticks = t;
        }
        std::cout << t - old_t << "ms" << std::endl;
        
        render_surface();

        SDL_Flip(screen_);
        done = handle_events(false);

        frames++;
    }
    
    std::stringstream file_name;
    file_name << "render_" << n_samples_ * n_sample_frames_ << "_samples.png";
    save_image_file(file_name.str().c_str());

    //SDL_WM_GrabInput(SDL_GRAB_OFF);
}

inline float clamp(float x, float a, float b)
{
    return x < a ? a : (x > b ? b : x);
}

void RenderWindow::render_surface() {
    Image& tex = render_(n_samples_);
    n_sample_frames_++;
        
    SDL_LockSurface(screen_);
    const int r = screen_->format->Rshift / 8;
    const int g = screen_->format->Gshift / 8;
    const int b = screen_->format->Bshift / 8;
    
#pragma omp parallel for
    for (int y = 0; y < screen_->h; y++) {
        unsigned char* row = (unsigned char*)screen_->pixels + screen_->pitch * y;
        const float4* rendered_row = tex.row(y);
        float4* buf_row = img_.row(y);
        
        for (int x = 0; x < screen_->w; x++) {        
            buf_row[x] += rendered_row[x];
        
            row[x * 4 + r] = 255.0f * clamp(buf_row[x].x / (n_samples_ * n_sample_frames_), 0.0f, 1.0f);
            row[x * 4 + g] = 255.0f * clamp(buf_row[x].y / (n_samples_ * n_sample_frames_), 0.0f, 1.0f);
            row[x * 4 + b] = 255.0f * clamp(buf_row[x].z / (n_samples_ * n_sample_frames_), 0.0f, 1.0f);
        }
    }
    SDL_UnlockSurface(screen_);
}

bool RenderWindow::handle_events(bool flush) {
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

static void write_to_stream(png_structp png_ptr, png_bytep data, png_size_t length) {
    png_voidp a = png_get_io_ptr(png_ptr);
    ((std::ostream*)a)->write((const char*)data, length);
}

void flush_stream(png_structp png_ptr) {
    // Nothing to do
}

bool RenderWindow::save_image_file(const char* file_name) {
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

    png_bytep row = nullptr;
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        delete[] row;
        return false;
    }

    png_set_write_fn(png_ptr, &file, write_to_stream, flush_stream);

    png_set_IHDR(png_ptr, info_ptr, image_width_, image_height_,
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);
    
    row = new png_byte[4 * image_width_];

    for (int y = 0; y < image_height_; y++) {
        float4* buf_row = img_.row(y);
        for (int x = 0; x < image_width_; x++) {
            row[x * 4 + 0] = (png_byte)(255.0f * clamp(buf_row[x].x / (n_samples_ * n_sample_frames_), 0.0f, 1.0f));
            row[x * 4 + 1] = (png_byte)(255.0f * clamp(buf_row[x].y / (n_samples_ * n_sample_frames_), 0.0f, 1.0f));
            row[x * 4 + 2] = (png_byte)(255.0f * clamp(buf_row[x].z / (n_samples_ * n_sample_frames_), 0.0f, 1.0f));
            row[x * 4 + 3] = (png_byte)(255.0f);
        }
        png_write_row(png_ptr, row);
    }
    
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    delete[] row;
    
    return true;
}

} // namespace imba

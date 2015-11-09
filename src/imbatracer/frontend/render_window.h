#ifndef IMBA_RENDER_WINDOW_H
#define IMBA_RENDER_WINDOW_H

#include <SDL.h>
#include "../render/render.h"

namespace imba {

    class RenderWindow {
        using StateType = PTState;
    public:
        RenderWindow(int img_width, int img_height, int n_samples, Renderer<StateType>& r);
        ~RenderWindow(); 
        
        void render();
        bool handle_events(bool flush);
        void render_surface(); 
        
    private:    
        int image_width_;
        int image_height_;
        Image img_;
        SDL_Surface* screen_;
        Renderer<StateType>& render_;
        
        int n_samples_;
        int n_sample_frames_;
        
        bool save_image_file(const char* filename);
    };

} // namespace imba

#endif
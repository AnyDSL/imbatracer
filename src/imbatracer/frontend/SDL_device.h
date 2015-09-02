#include <SDL.h>
#include "../render/render.h"

namespace imba {

    class SDLDevice {
    public:
        SDLDevice(int img_width, int img_height, Render& r);
        ~SDLDevice(); 
        
        void render();
        bool handle_events(bool flush);
        void render_surface(); 
        
    private:    
        int image_width_;
        int image_height_;
        Image img_;
        SDL_Surface* screen_;
        Render& render_;
        
        int n_samples_;
    };

} // namespace imba

#include <SDL.h>
#include "render.h"

namespace imba {

    class SDLDevice {
    public:
        SDLDevice(int img_width, int img_height);
        ~SDLDevice(); 
        
        void render(Scene& scene, Accel& accel);
        bool handle_events(bool flush);
        void render_surface(Scene& scene, Accel& accel); 
        
    private:    
        int image_width_;
        int image_height_;
        Texture tex_;
        SDL_Surface* screen_;
        RayQueue prim_queue_;
        RayQueue sec_queue_;
    };

} // namespace imba

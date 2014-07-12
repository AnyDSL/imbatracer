#ifndef SDLBUFFERGUI_H
#define SDLBUFFERGUI_H

#include <io/sdlgui.h>
#include <atomic>

namespace rt {
    const unsigned nBuffers = 2;
    
    // This adds Image-buffers to the GUI that are filled in the main thread, and displayed from the render thread
    class SDLBufferGui : public SDLGui {
    public:
        SDLBufferGui(unsigned w, unsigned h);
        
        virtual int main();
        
    protected:
        // called in the main thread
        virtual void _Render(CountedPtr<Image> img, float time) = 0;
        
        // implemented by us
        virtual CountedPtr<Image> _Update(float dt);
        
        CountedPtr<Image> buffers[nBuffers];
        std::atomic_uint curBuffer; // the latest complete, to-be-displayed buffer. Only main thread may change.
        
        std::atomic<float> time; // the time, as seen by the GUI thread. Only GUI thread may change.
    };
}

#endif
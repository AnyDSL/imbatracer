#ifndef SDLBUFFERGUI_H
#define SDLBUFFERGUI_H

#include <io/sdlgui.h>

namespace rt {
    // This adds Image-buffers to the GUI that are filled in the main thread, and displayed from the render thread
    class SDLBufferGui : public SDLGui {
        
    };
}

#endif
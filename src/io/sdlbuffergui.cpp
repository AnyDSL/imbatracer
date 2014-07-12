#include "image.h"

#include "sdlbuffergui.h"

namespace rt {
    
    SDLBufferGui::SDLBufferGui(unsigned w, unsigned h)
     : SDLGui(w, h), curBuffer(0), time(0)
    {
        for (unsigned i = 0; i < nBuffers; ++i) {
            buffers[i] = nullptr;
        }
    }
     
    // FIXME: do some kind of throttling?
     
    int SDLBufferGui::main()
    {
        Init();
        // now let's render
        while (!WaitingForQuit()) {
            // get next image
            const unsigned next = (curBuffer+1)%nBuffers;
            auto img = buffers[next];
            // does the image have the right size?
            if (!(img->width() == windowW && img->height() == windowH)) {
                img = new Image(windowW, windowH);
                buffers[next] = img;
            }
            // render into it!
            _Render(img);
            // this is the new "now"
            curBuffer = next;
        }
        return 0;
    }
     
    CountedPtr<Image> SDLBufferGui::_Update(float dt)
    {
        // some time passed
        time = time + dt;
        // check the size of the current image
        auto img = buffers[curBuffer];
        if (img->width() == windowW && img->height() == windowH)
            return img;
        // we don't have anything useful
        return nullptr;
    }
    
}

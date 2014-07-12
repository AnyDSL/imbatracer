#include "image.h"
#include <chrono>

#include "sdlbuffergui.h"

namespace rt {
    
    SDLBufferGui::SDLBufferGui(unsigned w, unsigned h)
     : SDLGui(w, h), curBufferIdx(0), displayBufferSerial(0), time(0)
    {
        for (unsigned i = 0; i < nBuffers; ++i) {
            buffers[i] = std::make_tuple(i, CountedPtr<Image>(nullptr));
        }
    }
    
    // FIXME: do some kind of throttling?
    
    unsigned SDLBufferGui::getFreeIdx()
    {
        std::unique_lock<std::mutex> lock(bufMutex);
        const unsigned nextIdx = (curBufferIdx+1)%nBuffers; // we assume nobody will change curBuffer while we are running
        while (std::get<0>(buffers[nextIdx]) == displayBufferSerial) {
            std::cout << "Next index " << nextIdx << " has currently displayed serial " << displayBufferSerial << ", waiting for display thread" << std::endl;
            // this buffer is currently displayed. wait till an update comes in (which will display curBufferIdx).
            bufChanged.wait(lock);
        }
        /* so the currently displayed buffer has anotehr serial. we can use this position, because even if the display is uodated while we are running, it will be
         * updated to curBufferIdx, and not to this position. */
        return nextIdx;
    }
     
    int SDLBufferGui::main()
    {
        Init();
        // now let's render
        unsigned serial = 1;
        while (!WaitingForQuit()) {
            // get a free image
            const unsigned nextIdx = getFreeIdx();
            auto img = std::get<1>(buffers[nextIdx]);
            // does the image have the right size?
            if (!(img && img->width() == windowW && img->height() == windowH)) {
                img = new Image(windowW, windowH);
            }
            // render into it!
            _Render(img, time);
            // this is the new "now"
            buffers[nextIdx] = std::make_tuple(serial, img);
            //std::cout << "Rendered serial " << serial << " into index " << nextIdx << std::endl;
            std::unique_lock<std::mutex> lock(bufMutex);
            curBufferIdx = nextIdx;
            bufChanged.notify_all();
            ++serial;
        }
        WaitForQuit();
        return 0;
    }
     
    CountedPtr<Image> SDLBufferGui::_Update(float dt)
    {
        // some time passed
        time = time + dt;
        // check the next image to display
        CountedPtr<Image> img = nullptr;
        {
            std::unique_lock<std::mutex> lock(bufMutex);
            unsigned nextSerial = std::get<0>(buffers[curBufferIdx]);
            if (nextSerial == displayBufferSerial) {
                // the image did not even change. Wait for this to happen, but not too long.
                bufChanged.wait_for(lock, std::chrono::milliseconds(50));
                nextSerial = std::get<0>(buffers[curBufferIdx]);
                if (nextSerial == displayBufferSerial)
                    return nullptr; // still nothing, go back to event loop
            }
            // there's an image with a new serial available. get it.
            displayBufferSerial = nextSerial;
            bufChanged.notify_all();
            //std::cout << "Displaying serial " << nextSerial << " from index " << curBufferIdx << std::endl;
            img = std::get<1>(buffers[curBufferIdx]);
        }
         // is this image usable?
        if (img && img->width() == windowW && img->height() == windowH)
            return img;
        // we don't have anything useful
        return nullptr;
    }
    
}

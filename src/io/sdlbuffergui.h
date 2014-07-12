#ifndef SDLBUFFERGUI_H
#define SDLBUFFERGUI_H

#include <io/sdlgui.h>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <tuple>

namespace rt {
    const unsigned nBuffers = 2;
    
    // This adds Image-buffers to the GUI that are filled in the main thread, and displayed from the render thread
    class SDLBufferGui : public SDLGui {
    public:
        SDLBufferGui(unsigned w, unsigned h, const char *title);
        
        virtual int main();
        
    protected:
        // called in the main thread
        virtual void _Render(CountedPtr<Image> img, float time) = 0;
        
        // implemented by us
        virtual CountedPtr<Image> _Update(float dt);
        
        std::string title;
    
    private:
        unsigned getFreeIdx(); // return an index we can use to draw into. call in main thread only [so nobody changes curBufferIdx while we are in here]
        
        typedef std::tuple<unsigned, CountedPtr<Image>> Buffer; // the image together with its serial number.
        Buffer buffers[nBuffers]; // current buffers. serial numbers will be ascending, with curBufferIdx being the largest and the next one being the lowest.
        unsigned curBufferIdx; // index of most up-to-date complete buffer. protected by bufMutex.
        unsigned displayBufferSerial; // serial number of the currently diplsayed buffer. Correpsonding image should not be written to! Protected by bufMutex.
        std::mutex bufMutex;  // protects the current buffer
        std::condition_variable bufChanged; // triggered when the current buffer or the displayed buffer changes. Protected by bufMutex.
        
        std::atomic<float> time; // the time, as seen by the GUI thread. Only GUI thread may change.
        float lastDispTime; // time the last image update was done. GUI thread only.
    };
}

#endif

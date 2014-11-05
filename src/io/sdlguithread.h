#ifndef CG_IO_SDLGUI_THREAD_H
#define CG_IO_SDLGUI_THREAD_H

#include <io/image.h>
#include <SDL_video.h>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace rt {

class SDLGui;
class SDLRenderer;

class SDLGuiThread
{
public:
    SDLGuiThread(SDLGui *gui);
    virtual ~SDLGuiThread();

    // start the thread
    void launch();

    // terminates the thread on the next round of the event loop, and wait till it is quit (do NOT call from within thread!)
    void quitThreadNow();

    // terminates the thread when the main thread sees fit
    void quitThreadASAP();

    // tell if the thread wants to quit
    bool waitingForQuit()
    {
        return getState() > READY;
    }

    // wait till the thread was quit
    void waitForQuit();

    // get underlying SDL window - call only from GUI thread!
    SDL_Window *getWindow() { return _window; }

    float getPixelScale();
    void setPixelScale(float s);


protected:
    void run();


    // while the thread is running, the state is only ever increased
    enum ReadyState
    {
        UNDEFINED,
        READY,
        ABOUT_TO_QUIT,
        QUIT,
        FAIL
    };

    std::mutex stateMutex; // protects threadState, aboutToQuitTime
    std::condition_variable stateChanged;
    ReadyState threadState; // protected by stateMutex
    unsigned aboutToQuitTime; // remember when we wanted to exit; protected by stateMutex

    ReadyState getState() {
        std::lock_guard<std::mutex> lock(stateMutex);
        return threadState;
    }
    bool checkQuittingTooLong(unsigned nowTime);

private:

    SDLGui *_gui;
    std::thread _th;

    // SDL stuff
    SDL_Window *_window;
    SDL_GLContext _glctx;
    SDLRenderer *_disp;
    float pixelScale;
    unsigned _lastW, _lastH;

    bool init();
    void threadMain();
    void shutdown();
    void render(float dt);
    void resize(unsigned w, unsigned h);
    void handleEvents();
    bool createWindow(unsigned w, unsigned h, const char *title);
    void _sizeChanged(unsigned w, unsigned h);
};

} // end namespace rt

#endif

#ifndef CG_IO_SDLGUI_THREAD_H
#define CG_IO_SDLGUI_THREAD_H

#include <core/threading.h>
#include <io/image.h>
#include <SDL_video.h>
#include <core/threading.h>

namespace rt {

class SDLGui;
class SDLRenderer;

class SDLGuiThread : public Thread
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
        MTGuard g(waiter);
        return threadState > READY;
    }
    
    // wait till the thread was quit
    void waitForQuit();
    
    // get underlying SDL window - call only from GUI thread!
    SDL_Window *getWindow() { return _window; }

protected:
	virtual void run();


	// while the thread is running, the state is only ever increased
	enum ReadyState
	{
		UNDEFINED,
		READY,
		ABOUT_TO_QUIT,
		QUIT,
		FAIL
	};

	Waitable waiter; // protects threadState, aboutToQuitTime
	ReadyState threadState; // protected by waiter
    unsigned aboutToQuitTime; // remember when we wanted to exit; protected by waiter

	ReadyState getState() {
		MTGuard g(waiter);
		return threadState;
	}
	bool checkQuittingTooLong(unsigned nowTime);

private:

	SDLGui *_gui;

	// SDL stuff
	SDL_Window *_window;
	SDL_GLContext _glctx;
	SDLRenderer *_disp;

	bool init();
	void threadMain();
	void shutdown();
	void render(float dt);
	void resize(unsigned w, unsigned h);
	void handleEvents();
	bool createWindow(unsigned w, unsigned h, const char *title);
};

} // end namespace rt

#endif

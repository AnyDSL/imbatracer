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

	Waitable waiter; // protects the state, and whatever you want it to protect
	ReadyState threadState; // protected by waiter

	ReadyState getStateSafe() {
		MTGuard g(waiter);
		return threadState;
	}

	// the custom events you can send are all relative to this ID
	enum CustomEvent {
		// Resize the window: Expects data1 and data2 in WindowEvent to be width and height
		ResizeRequest,
		// Show an image on screen: Expects a UserEvent with data1 being a pointer to a GLImage. The refcount must be increased when adding it to the event queue.
		ShowImage,
		// Change the window title. Expects a pointer to a std::string in UserEvent.data1.
		// The string is "given" to the event queue, so it'll be deleted when the event is handled.
		ChangeTitle,
		// Keep cursor in window?
		SetGrabMouse,

		// just for bookkeeping
		NumCustomEvents
	};
	// convert a cstom event ID to a "real" SDL event ID. You may only call this with state in (READY, ABOUT_TO_QUIT, QUIT).
	unsigned getEventID(CustomEvent ev) { return customEventBase + ev; }

	// terminates the thread on the next round of the event loop, and wait till it is quit (do NOT call from within thread!)
	void quitThreadNow();

	// terminates the thread when the main thread sees fit
	void quitThreadASAP();

	void setRenderOn(bool on);
	bool isGrabMouse();

private:

	SDLGui *_gui;

	// SDL stuff
	SDL_Window *_window;
	SDL_GLContext _glctx;
	SDLRenderer *_disp;

	AtomicInt _renderOn;

	CountedPtr<Image> imageToUpload;

	// remember when we wanted to exit
	unsigned aboutToQuitTime;

	// the base ID for our events
	unsigned customEventBase;

	bool init();
	void threadMain();
	void shutdown();
	void render();
	void resize(unsigned w, unsigned h);
	void handleEvents();
	bool createWindow(unsigned w, unsigned h, const char *title);
};

} // end namespace rt

#endif

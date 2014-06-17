#include "sdlguithread.h"
#include "sdlgui.h"

#include <SDL.h>
#include "sdlrenderer.h"

namespace rt {

// width and height of initial window - we assume your screen is large enough so this
// comfortably fits
const unsigned baseWidth = 600;
const unsigned baseHeight = 480;

// time to wait till the window is "force-closed" (in milliseconds)
const unsigned forceCloseTime = 500;

SDLGuiThread::SDLGuiThread(SDLGui *gui)
	: threadState(UNDEFINED), _gui(gui), _window(nullptr), _glctx(nullptr), _disp(nullptr), customEventBase(-1)
{
}

SDLGuiThread::~SDLGuiThread()
{
}

void SDLGuiThread::run()
{
	bool success = init();
	{
		MTGuard g(waiter);
		threadState = success ? READY : FAIL;
		waiter.broadcast();
	}
	if(!success)
		return;

	_gui->_OnInit();

	threadMain();

	_gui->_OnShutdown();

	shutdown();
	{
		MTGuard g(waiter);
		threadState = UNDEFINED; // reset back to inital state, thread can be re-launched
		waiter.broadcast();
	}
}

void SDLGuiThread::quitThreadNow()
{
	{
		MTGuard g(waiter);
		if (threadState < QUIT) threadState = QUIT;
		waiter.broadcast();
	}
	join();
}

void SDLGuiThread::quitThreadASAP() {
	MTGuard g(waiter);
	if (threadState < ABOUT_TO_QUIT) {
		std::cout << "SDLGui: About to quit" << std::endl;
		threadState = ABOUT_TO_QUIT;
		aboutToQuitTime = SDL_GetTicks();
	}
	waiter.broadcast();
}

bool SDLGuiThread::init()
{
	assert(getStateSafe() == UNDEFINED, "Attempt to initialize the same GLGuiThread twice");
	aboutToQuitTime = 0;

	std::cout << "GUI thread init..." << std::endl;

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		std::cerr << "SDL_InitSubSystem Error: " << SDL_GetError() << std::endl;
		return false;
	}

	if (customEventBase == static_cast<unsigned>(-1)) {
		// we need some space for our events
		customEventBase = SDL_RegisterEvents(NumCustomEvents);
		if (customEventBase == static_cast<unsigned>(-1)) {
			std::cerr << "SDL_RegisterEvents failed" << std::endl;
			return false;
		}
	}

	/*std::cout << "Loading OpenGL..." << std::endl;

	if (SDL_GL_LoadLibrary(nullptr) == -1)
	{
		std::cerr << "SDL_GL_LoadLibrary Error: " << SDL_GetError() << std::endl;
		return false;
	}*/

	if(!createWindow(baseWidth, baseHeight, "UberTracer")) // this also creates the GL context
		return false;

	_disp = new SDLRenderer(_window);

	// don't render faster than the GPU
	SDL_GL_SetSwapInterval(-1);

	setRenderOn(true);

	if(!_disp->Init())
		return false;

	return true;
}

bool SDLGuiThread::createWindow(unsigned w, unsigned h, const char *title)
{
	assert(!_window, "There's already a window");

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
	_window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, flags);
	if(!_window)
	{
		std::cerr << "Failed to create SDL window" << std::endl;
		return false;
	}
	/*_glctx = SDL_GL_CreateContext(_window);
	if(!_glctx)
	{
		std::cerr << "Failed to create SDL OpenGL context" << std::endl;
		return false;
	}*/

	// Notify initial size
	int aw, ah;
	SDL_GetWindowSize(_window, &aw, &ah);
	_gui->_OnWindowResize(aw, ah);

	return true;
}

void SDLGuiThread::shutdown()
{

	if(_disp)
		_disp->Shutdown();

	if(_window)
	{
		SDL_GL_MakeCurrent(_window, nullptr);
		if(_glctx)
			SDL_GL_DeleteContext(_glctx);
		SDL_DestroyWindow(_window);
	}

	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	_glctx = nullptr;
	_window = nullptr;

	delete _disp;
}


void SDLGuiThread::render()
{
	if(!_renderOn)
		return;

	if(imageToUpload)
	{
		_disp->uploadImage(imageToUpload.content());
		imageToUpload = nullptr;
	}

	_disp->BeginFrame();
	_disp->render();
	_disp->EndFrame();

	//SDL_GL_SwapWindow(_window);
}

void SDLGuiThread::resize(unsigned w, unsigned h)
{
#ifndef _WIN32
	// on Linux, SDL thinks that resizing will always work. However, if the window manager decides our
	// window must not change in size, X11 will NOT send any resize event which may correct
	// SDL's false imagination
	SDL_SetWindowSize(_window, baseWidth, baseHeight);
#endif
	SDL_SetWindowSize(_window, w, h);
	SDL_SetWindowPosition(_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void SDLGuiThread::threadMain()
{
	Uint32 lastUpdateTime = SDL_GetTicks();
	while(getStateSafe() < QUIT)
	{
		handleEvents();

		Uint32 nowTime = SDL_GetTicks();
		Uint32 diffTime = nowTime - lastUpdateTime;
		lastUpdateTime = nowTime;
		_gui->_Update(diffTime / 1000.0f);
		handleEvents(); // make sure we saw the image-update-events
		render();

		// check if we are waiting for quit too long. If yes, terminate.
		if (getStateSafe() >= ABOUT_TO_QUIT && nowTime-aboutToQuitTime > forceCloseTime) {
			std::cerr << "Forcing unclean shutdown of the entire application" << std::endl;
			::exit(1);
			// of course we could just terminate the thread, but that wouldn't gain us anything - it would mean one cannot
			// stop long render jobs with escape or Ctrl-C.
		}
	}
}

void SDLGuiThread::handleEvents()
{
	bool ignoreMouseMove = false;

	SDL_PumpEvents();

	SDL_Event ev;
	while(SDL_PollEvent(&ev))
	{
		switch(ev.type)
		{
		case SDL_KEYDOWN:
			_gui->_OnKey(ev.key.keysym.scancode, ev.key.keysym.sym, ev.key.keysym.mod, true);
			continue; // in the loop

		case SDL_KEYUP:
			_gui->_OnKey(ev.key.keysym.scancode, ev.key.keysym.sym, ev.key.keysym.mod, false);
			continue; // in the loop

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			_gui->_OnMouseButton(ev.button.button, ev.button.state, ev.button.x, ev.button.y);
			continue; // in the loop

		case SDL_MOUSEWHEEL:
			_gui->_OnMouseWheel(ev.wheel.x, ev.wheel.y);
			continue;

		case SDL_MOUSEMOTION:
			if(!ignoreMouseMove)
				_gui->_OnMouseMotion(ev.motion.xrel, ev.motion.yrel);
			continue; // in the loop

		case SDL_QUIT:
			quitThreadASAP();
			continue; // in the loop

		case SDL_WINDOWEVENT:
			{
				switch(ev.window.event)
				{
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					imageToUpload = nullptr;
					_disp->OnWindowResize(ev.window.data1, ev.window.data2);
					_gui->_OnWindowResize(ev.window.data1, ev.window.data2);
					break; // the innermost switch

				default:
					;
				}
			}
			continue; // in the loop
		}

		// ---- CUSTOM EVENTS BELOW ----
		switch (ev.type - customEventBase) {

		case ResizeRequest:
			resize(ev.window.data1, ev.window.data2);
			continue; // in the loop

		case ShowImage:
		{
			Image *img = static_cast<Image*>(ev.user.data1);
			imageToUpload = img;
			img->decref(); // it's out of the SDL event system now
			continue; // in the loop
		}

		case ChangeTitle:
		{
			std::string *title = static_cast<std::string*>(ev.user.data1);
			SDL_SetWindowTitle(_window, title->c_str());
			delete title;
			continue;
		}

		case SetGrabMouse:
		{
			MTGuard g(waiter);
			const bool doGrab = ev.window.data1;
			SDL_SetWindowGrab(_window, (SDL_bool)doGrab);
			SDL_SetRelativeMouseMode((SDL_bool)doGrab);
			continue;
		}

		}
	}
}

bool SDLGuiThread::isGrabMouse()
{
	MTGuard g(waiter);
	return SDL_GetWindowGrab(_window);
}

void SDLGuiThread::setRenderOn(bool on)
{
	_renderOn = (int)on;
}


} // end namespace rt

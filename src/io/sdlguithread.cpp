#include "sdlguithread.h"
#include "sdlgui.h"

#include <SDL.h>
#include "sdlrenderer.h"

namespace rt {

// time to wait till the window is "force-closed" (in milliseconds)
const unsigned forceCloseTime = 500;

SDLGuiThread::SDLGuiThread(SDLGui *gui)
	: threadState(UNDEFINED), _gui(gui), _window(nullptr), _glctx(nullptr), _disp(nullptr)
{
}

SDLGuiThread::~SDLGuiThread()
{
}

void SDLGuiThread::launch()
{
    Thread::launch();
    {
        // Waiting for GUI thread to start
        std::unique_lock<std::mutex> lock(stateMutex);
        while(threadState < SDLGuiThread::READY)
            stateChanged.wait(lock);
    }
    if(getState() == SDLGuiThread::FAIL)
    {
        // something went seriously wrong
        quitThreadNow();
        ::exit(1);
    }
}

void SDLGuiThread::run()
{
	bool success = init();
	{
		std::unique_lock<std::mutex> lock(stateMutex);
		threadState = success ? READY : FAIL;
		stateChanged.notify_all();
	}
	if(!success)
		return;

	_gui->_OnInit();

	threadMain();

	_gui->_OnShutdown();

	shutdown();
	{
		std::unique_lock<std::mutex> lock(stateMutex);
		threadState = UNDEFINED; // reset back to inital state, thread can be re-launched
		stateChanged.notify_all();
	}
}

void SDLGuiThread::quitThreadNow()
{
	{
		std::unique_lock<std::mutex> lock(stateMutex);
		if (threadState < QUIT) threadState = QUIT;
		stateChanged.notify_all();
	}
	join();
}

void SDLGuiThread::quitThreadASAP()
{
	std::unique_lock<std::mutex> lock(stateMutex);
	if (threadState < ABOUT_TO_QUIT) {
		std::cout << "SDLGuiThread: About to quit" << std::endl;
		threadState = ABOUT_TO_QUIT;
		aboutToQuitTime = SDL_GetTicks();
	}
	stateChanged.notify_all();
}

void SDLGuiThread::waitForQuit()
{
    std::cout << "SDLGuiThread::WaitForQuit" << std::endl;
    // wait till the thread does not want to run anymore
    {
        std::unique_lock<std::mutex> lock(stateMutex);
        while(threadState <= SDLGuiThread::READY)
            stateChanged.wait(lock);
    }
    // tell it that it's okay to go
    quitThreadNow();
}

bool SDLGuiThread::init()
{
	assert(getState() == UNDEFINED, "Attempt to initialize the same GLGuiThread twice");
	aboutToQuitTime = 0;

	std::cout << "GUI thread init..." << std::endl;

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		std::cerr << "SDL_InitSubSystem Error: " << SDL_GetError() << std::endl;
		return false;
	}

	if(!createWindow(_gui->windowW, _gui->windowH, "SDL Window"))
		return false;

	_disp = new SDLRenderer(_window);

	// don't render faster than the GPU
	SDL_GL_SetSwapInterval(-1);

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


void SDLGuiThread::render(float dt)
{
    CountedPtr<Image> img = _gui->_Update(dt);
	if(img)
	{
		_disp->uploadImage(img.content());
	}

	_disp->BeginFrame();
	_disp->render();
	_disp->EndFrame();
}

void SDLGuiThread::resize(unsigned w, unsigned h)
{
#ifndef _WIN32
	// on Linux, SDL thinks that resizing will always work. However, if the window manager decides our
	// window must not change in size, X11 will NOT send any resize event which may correct
	// SDL's false imagination. So make the window really small first to get the right resize events.
	SDL_SetWindowSize(_window, 640, 480);
#endif
	SDL_SetWindowSize(_window, w, h);
	SDL_SetWindowPosition(_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

bool SDLGuiThread::checkQuittingTooLong(unsigned nowTime)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return threadState >= ABOUT_TO_QUIT && nowTime-aboutToQuitTime > forceCloseTime;
}

void SDLGuiThread::threadMain()
{
	Uint32 lastUpdateTime = SDL_GetTicks();
	while(getState() < QUIT)
	{
		handleEvents();

		Uint32 nowTime = SDL_GetTicks();
		Uint32 diffTime = nowTime - lastUpdateTime;
		lastUpdateTime = nowTime;
		render(diffTime / 1000.0f);

		// check if we are waiting for quit too long. If yes, terminate.
		if (checkQuittingTooLong(nowTime)) {
			std::cerr << "Forcing unclean shutdown of the entire application" << std::endl;
			::exit(1);
			// of course we could just terminate the thread, but that wouldn't gain us anything - it would mean one cannot
			// stop long render jobs with escape or Ctrl-C.
		}
	}
}

void SDLGuiThread::handleEvents()
{
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
					_disp->OnWindowResize(ev.window.data1, ev.window.data2);
					_gui->_OnWindowResize(ev.window.data1, ev.window.data2);
					break; // the innermost switch

				default:
					;
				}
			}
			continue; // in the loop
		}
	}
}



} // end namespace rt

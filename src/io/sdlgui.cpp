#include "sdlgui.h"
#include "sdlguithread.h"
#include <core/assert.h>
#include <core/util.h>
#include <iostream>

#include <SDL.h>

namespace rt {


SDLGui::SDLGui()
	: mxaccu(0), myaccu(0), mouseWheelChange(0), windowW(0), windowH(0), spaceWasPressed(false), th(nullptr)
{
}

SDLGui::~SDLGui()
{
	th->quitThreadNow();
	delete th;
}

void SDLGui::Init()
{
    th = new SDLGuiThread(this);
    th->launch();
    {
        std::cout << "Waiting for GUI thread to start" << std::endl;
        MTGuard g(th->waiter);
        while(th->threadState < SDLGuiThread::READY)
            th->waiter.wait();
    }
    if(th->getStateSafe() == SDLGuiThread::FAIL)
    {
        // something went seriously wrong
        th->quitThreadNow();
        ::exit(1);
    }
}

void SDLGui::SetWindowTitle(const char *title)
{
	SDL_Event ev;
	SDL_zero(ev);
	ev.type = th->getEventID(SDLGuiThread::ChangeTitle);
	ev.user.data1 = new std::string(title);
	SDL_PushEvent(&ev);
}

void SDLGui::ShowImage(CountedPtr<Image> img)
{
	Image *rawimg = img.content();
	rawimg->incref(); // reference held by SDL event system
	SDL_Event ev;
	SDL_zero(ev);
	ev.type = th->getEventID(SDLGuiThread::ShowImage);
	ev.user.data1 = rawimg;
	SDL_PushEvent(&ev);
}

void SDLGui::Resize(unsigned w, unsigned h)
{
	SDL_Event ev;
	SDL_zero(ev);
	ev.type = th->getEventID(SDLGuiThread::ResizeRequest);
	ev.window.data1 = w;
	ev.window.data2 = h;
	SDL_PushEvent(&ev);
}

void SDLGui::_OnInit()
{
}

void SDLGui::_Update(float /*dt*/)
{
	// don't spin in a too tight loop, better sleep a bit
	SDL_Delay(50);

	resetMouse();
}

bool SDLGui::_OnKey(int /*scancode*/, int key, int /*mod*/, bool state)
{
	if(state)
	{
		switch(key)
		{
			case SDLK_ESCAPE:
			{
				th->quitThreadASAP();
				return true;
			}

			case SDLK_SPACE:
			{
				MTGuard(th->waiter);
				spaceWasPressed = true;
				th->waiter.broadcast();
				return true;
			}
		}
	}

	return false;
}

void SDLGui::_OnMouseButton(int /*button*/, int /*state*/, int /*x*/, int /*y*/)
{
}

void SDLGui::_OnMouseWheel(int /*x*/, int y)
{
	MTGuard g(th->waiter);

	if(y > 0)
		++mouseWheelChange;
	else if(y < 0)
		--mouseWheelChange;
}

void SDLGui::_OnWindowResize(int w, int h)
{
	MTGuard g(th->waiter);
	windowW = w;
	windowH = h;
}

void SDLGui::_OnShutdown()
{

}

void SDLGui::resetMouse()
{
	MTGuard g(th->waiter);
	mxaccu = myaccu = 0;
	mouseWheelChange = 0;
}

void SDLGui::_OnMouseMotion(int xrel, int yrel)
{
	MTGuard g(th->waiter);
	mxaccu += xrel;
	myaccu += yrel;
}

void SDLGui::WaitForSpace()
{
	if(th->getStateSafe() <= SDLGuiThread::READY)
	{
		std::cout << "SDLGui::WaitForSpace" << std::endl;
		MTGuard g(th->waiter);
		spaceWasPressed = false;
		while(th->threadState <= SDLGuiThread::READY && !spaceWasPressed) // can't press space when the thread is dead. fall through fast if exit was requested.
			th->waiter.wait();
	}
	else
		std::cout << "SDLGui::WaitForSpace - GUI is (about to be) gone, going on" << std::endl;
}

void SDLGui::WaitForQuit()
{
	std::cout << "SDLGui::WaitForQuit" << std::endl;
	// wait till the thread does not want to run anymore
	{
		MTGuard g(th->waiter);
		while(th->threadState <= SDLGuiThread::READY)
			th->waiter.wait();
	}
	// tell it that it's okay to go
	th->quitThreadNow();
}

void SDLGui::getMouseMove(float *x, float *y)
{
	MTGuard g(th->waiter);
	*x = mxaccu / float(windowW);
	*y = myaccu / float(windowH);
}

void SDLGui::setGrabMouse(bool on)
{
	SDL_Event ev;
	SDL_zero(ev);
	ev.type = th->getEventID(SDLGuiThread::SetGrabMouse);
	ev.window.data1 = on;
	SDL_PushEvent(&ev);
}

void SDLGui::setCursorVisible(bool on)
{
	SDL_ShowCursor(on);
}

int SDLGui::getMouseWheelChange()
{
	MTGuard g(th->waiter);
	return mouseWheelChange;
}

bool SDLGui::isGrabMouse()
{
	return th->isGrabMouse();
}

void SDLGui::setRenderOn(bool on)
{
	th->setRenderOn(on);
}

} // end namespace rt

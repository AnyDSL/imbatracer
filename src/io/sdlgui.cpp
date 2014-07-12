#include "sdlgui.h"
#include "sdlguithread.h"
#include <core/assert.h>
#include <core/util.h>
#include <iostream>

#include <SDL.h>

namespace rt {


SDLGui::SDLGui(unsigned width, unsigned height)
	: windowW(width), windowH(height), th(nullptr)
{
}

SDLGui::~SDLGui()
{
	th->quitThreadNow();
	delete th;
}

void SDLGui::Init()
{
    assert(th == nullptr, "Thread already set?");
    th = new SDLGuiThread(this);
    th->launch();
}

SDL_Window *SDLGui::GetWindow()
{
    return th->getWindow();
}

void SDLGui::SetWindowTitle(const std::string &title)
{
	SDL_SetWindowTitle(GetWindow(), title.c_str());
}

bool SDLGui::WaitingForQuit()
{
    return th->waitingForQuit();
}

CountedPtr<Image> SDLGui::_Update(float /*dt*/)
{
	// don't spin in a too tight loop, better sleep a bit
	SDL_Delay(50);
	return nullptr;
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
		}
	}

	return false;
}

void SDLGui::_OnWindowResize(int w, int h)
{
	windowW = w;
	windowH = h;
}


} // end namespace rt

#include "sdlgui.h"
#include "sdlguithread.h"
#include <core/assert.h>
#include <core/util.h>
#include <iostream>

#include <SDL.h>

namespace rt {


SDLGui::SDLGui(unsigned width, unsigned height)
    : windowW(width), windowH(height), mouseGrabbed(false), th(nullptr)
{
}

SDLGui::~SDLGui()
{
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

void SDLGui::WaitForQuit()
{
    th->waitForQuit();
}

CountedPtr<Image> SDLGui::_Update(float /*dt*/)
{
    // don't spin in a too tight loop, better sleep a bit
    SDL_Delay(50);
    _HandleEvents();
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

            case SDLK_g:
            {
                mouseGrabbed = !mouseGrabbed;
                SDL_SetWindowGrab(th->getWindow(), (SDL_bool)mouseGrabbed);
                SDL_SetRelativeMouseMode((SDL_bool)mouseGrabbed);
                return true;
            }
        }
    }

    std::lock_guard<std::mutex> g(eventLock);
    eventQ.push_back(EventHolder::Key(!!state, key));

    return false;
}

void SDLGui::_OnWindowResize(int w, int h)
{
    std::cout << "New window size: " << w << "x" << h << std::endl;
    windowW = w;
    windowH = h;
}

void SDLGui::_OnMouseButton(int button, int state, int /*x*/, int /*y*/)
{
    std::lock_guard<std::mutex> g(eventLock);
    eventQ.emplace_back(EventHolder::MouseButton(!!state, button));
}

void SDLGui::_OnMouseMotion(int xrel, int yrel)
{
    std::lock_guard<std::mutex> g(eventLock);
    eventQ.emplace_back(EventHolder::MouseMove((float)xrel / int(windowW), (float)yrel / int(windowH)));
}

void SDLGui::_OnMouseWheel(int x, int y)
{
    std::lock_guard<std::mutex> g(eventLock);
    eventQ.emplace_back(EventHolder::MouseWheel(x, y));
}

void SDLGui::_HandleEvents()
{
    std::lock_guard<std::mutex> g(eventLock);

    if(size_t sz = eventQ.size())
    {
        _DispatchEvents(&eventQ[0], sz);
        eventQ.clear();
    }
}


} // end namespace rt

#include "sdlgui.h"
#include "sdlrenderer.h"
#include <io/image.h>

#include <core/assert.h>
#include <core/util.h>
#include <iostream>

#include <SDL.h>

namespace rt {


SDLGui::SDLGui(unsigned width, unsigned height)
    : windowW(width), windowH(height), realWindowW(width), realWindowH(height), pixelScale(1.0f), mouseGrabbed(false),
      _window(nullptr), _glctx(nullptr), _disp(nullptr), _lastW(width), _lastH(height),
      lastUpdateTime(0), _wantQuit(false)
{
}

SDLGui::~SDLGui()
{
}

bool SDLGui::Init()
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
    {
        std::cerr << "SDL_InitSubSystem Error: " << SDL_GetError() << std::endl;
        return false;
    }

    if(!createWindow(windowW, windowH, "SDL Window"))
        return false;

    _disp = new SDLRenderer(_window);

    // don't render faster than the GPU
    SDL_GL_SetSwapInterval(-1);

    if(!_disp->Init())
        return false;

    _OnInit();

    return true;
}

int SDLGui::main()
{
    if(!Init())
        return 1;
    while(!WaitingForQuit())
        _UpdateTick();
    WaitForQuit();
    shutdown();
    return 0;
}

bool SDLGui::createWindow(unsigned w, unsigned h, const char *title)
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
    _OnWindowResize(aw, ah, aw, ah);

    return true;
}

void SDLGui::shutdown()
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

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    _glctx = nullptr;
    _window = nullptr;

    delete _disp;
}


void SDLGui::render(Image *img)
{
    _disp->uploadImage(img);
    _disp->BeginFrame();
    _disp->render();
    _disp->EndFrame();
}

void SDLGui::resize(unsigned w, unsigned h)
{
#ifndef _WIN32
    // on Linux, SDL thinks that resizing will always work. However, if the window manager decides our
    // window must not change in size, X11 will NOT send any resize event which may correct
    // SDL's false imagination. So make the window really small first to get the right resize events.
    SDL_SetWindowSize(_window, 640, 480);
#endif
    SDL_SetWindowSize(_window, w, h);
    SDL_SetWindowPosition(_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    _lastW = w;
    _lastH = h;
}

CountedPtr<Image> SDLGui::renderOneImage()
{
    CountedPtr<Image> img = new Image(windowW, windowH);
    _Render(img.content());
    return img;
}

SDL_Window *SDLGui::GetWindow()
{
    return _window;
}

void SDLGui::SetWindowTitle(const std::string &title)
{
    SDL_SetWindowTitle(_window, title.c_str());
}

bool SDLGui::WaitingForQuit()
{
    return !!_wantQuit;
}

void SDLGui::WaitForQuit()
{
}

void SDLGui::_UpdateTick()
{
    handleEvents();
    _DispatchEvents();

    Uint32 nowTime = SDL_GetTicks();
    Uint32 diffTime = lastUpdateTime ? nowTime - lastUpdateTime : 0;
    lastUpdateTime = nowTime;
    const float dt = diffTime / 1000.0f;

    _Update(dt);
    CountedPtr<Image> img = renderOneImage();
    render(img.content());
}

void SDLGui::_Update(float /*dt*/)
{
    // nothing to do here
}

bool SDLGui::_OnKey(int /*scancode*/, int key, int /*mod*/, bool state)
{
    if(state)
    {
        switch(key)
        {
            case SDLK_ESCAPE:
            {
                quitThreadASAP();
                _wantQuit = true;
                return true;
            }

            case SDLK_g:
            {
                mouseGrabbed = !mouseGrabbed;
                SDL_SetWindowGrab(_window, (SDL_bool)mouseGrabbed);
                SDL_SetRelativeMouseMode((SDL_bool)mouseGrabbed);
                return true;
            }
        }
    }

    eventQ.push_back(EventHolder::Key(!!state, key));

    return false;
}

void SDLGui::_OnWindowResize(int w, int h, int realw, int realh)
{
    std::cout << "New window size: " << w << "x" << h << ", real " << realw << "x" << realh << std::endl;
    windowW = w;
    windowH = h;
    realWindowW = realw;
    realWindowH = realh;
}

void SDLGui::_OnMouseButton(int button, int state, int /*x*/, int /*y*/)
{
    eventQ.emplace_back(EventHolder::MouseButton(!!state, button));
}

void SDLGui::_OnMouseMotion(int xrel, int yrel)
{
    eventQ.emplace_back(EventHolder::MouseMove((float)xrel / int(realWindowW), (float)yrel / int(realWindowH)));
}

void SDLGui::_OnMouseWheel(int x, int y)
{
    eventQ.emplace_back(EventHolder::MouseWheel(x, y));
}

void SDLGui::_DispatchEvents()
{
    if(size_t sz = eventQ.size())
    {
        _DispatchEvents(&eventQ[0], sz);
        eventQ.clear();
    }
}


void SDLGui::handleEvents()
{
    SDL_PumpEvents();

    SDL_Event ev;
    while(SDL_PollEvent(&ev))
    {
        switch(ev.type)
        {
        case SDL_KEYDOWN:
            _OnKey(ev.key.keysym.scancode, ev.key.keysym.sym, ev.key.keysym.mod, true);
            continue; // in the loop

        case SDL_KEYUP:
            _OnKey(ev.key.keysym.scancode, ev.key.keysym.sym, ev.key.keysym.mod, false);
            continue; // in the loop

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            _OnMouseButton(ev.button.button, ev.button.state, ev.button.x, ev.button.y);
            continue; // in the loop

        case SDL_MOUSEWHEEL:
            _OnMouseWheel(ev.wheel.x, ev.wheel.y);
            continue;

        case SDL_MOUSEMOTION:
            _OnMouseMotion(ev.motion.xrel, ev.motion.yrel);
            continue; // in the loop

        case SDL_QUIT:
            quitThreadASAP();
            continue; // in the loop

        case SDL_WINDOWEVENT:
            {
                switch(ev.window.event)
                {
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        _sizeChanged(ev.window.data1, ev.window.data2);
                        break; // the innermost switch

                    default:
                        ;
                }
            }
            continue; // in the loop
        }
    }
}

void SDLGui::_sizeChanged(unsigned w, unsigned h)
{
    {
        //std::lock_guard<std::mutex> lock(stateMutex);
        _lastW = w;
        _lastH = h;
        w *= pixelScale;
        h *= pixelScale;
    }
    _disp->OnWindowResize(w, h);
    _OnWindowResize(w, h, _lastW, _lastH);
}

void SDLGui::setPixelScale(float s)
{
    //std::lock_guard<std::mutex> lock(stateMutex);
    if(pixelScale == s)
        return;

    assert(_lastW && _lastH, "Last window size is ", _lastW, ", ", _lastH);

    pixelScale = s;
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = SDL_WINDOWEVENT;
    ev.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    ev.window.data1 = _lastW;
    ev.window.data2 = _lastH;
    SDL_PushEvent(&ev);
}

float SDLGui::getPixelScale()
{
    //std::lock_guard<std::mutex> lock(stateMutex);
    return pixelScale;
}

void SDLGui::quitThreadASAP()
{
    _wantQuit = true;
}

} // end namespace rt

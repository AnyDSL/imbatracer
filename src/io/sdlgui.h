#ifndef CG_IO_SDLGUI_H
#define CG_IO_SDLGUI_H

#include <core/refcounted.h>
#include <atomic>
#include <vector>
#include <mutex>

#include <SDL_video.h>


namespace rt {

//class SDLGuiThread;
class Image;
class SDLRenderer;

class SDLGui
{
public:
    SDLGui(unsigned width, unsigned height);
    virtual ~SDLGui();

    int main();

    bool WaitingForQuit(); // do we want to quit?
    void WaitForQuit(); // do the quitting

    // only call these functions from the GUI Thread!
    SDL_Window *GetWindow();
    void SetWindowTitle(const std::string &title);

    bool isMouseGrabbed() const { return mouseGrabbed; }

    float getPixelScale();
    void setPixelScale(float s);

    enum EventType
    {
        EVT_KEY,
        EVT_MOUSEMOVE,
        EVT_MOUSEBUTTON,
        EVT_MOUSEWHEEL,
    };
    struct EventHolder
    {
        EventHolder() = default;
        EventHolder(EventType ev, bool down, int key, float x, float y) : ev(ev), key(key), x(x), y(y), down(down) {}
        EventType ev;
        int key;
        float x, y;
        bool down;
        static EventHolder Key(bool down, int key) { return EventHolder(EVT_KEY, down, key, 0, 0); }
        static EventHolder MouseMove(float x, float y) { return EventHolder(EVT_MOUSEMOVE, false, 0, x, y); }
        static EventHolder MouseButton(bool down, int btn) { return EventHolder(EVT_MOUSEBUTTON, down, btn, 0, 0); }
        static EventHolder MouseWheel(int changeX, int changeY) { return EventHolder(EVT_MOUSEWHEEL, false, 0, (float)changeX, (float)changeY); }
    };
    static_assert(std::is_pod<EventHolder>::value, "SDLGui::EventHolder must be POD");


protected:

    void render(Image *img);
    void resize(unsigned w, unsigned h);
    void handleEvents();
    bool Init();
    void shutdown();
    bool createWindow(unsigned w, unsigned h, const char *title);
    void _sizeChanged(unsigned w, unsigned h);
    void quitThreadASAP();

    virtual void _OnInit() {}
    virtual void _Update(float dt);
    virtual bool _OnKey(int scancode, int key, int mod, bool down); //!< returns whether the keypress was handled
    virtual void _OnMouseButton(int button, int state, int x, int y);
    virtual void _OnMouseMotion(int xrel, int yrel);
    virtual void _OnMouseWheel(int x, int y);
    virtual void _OnWindowResize(int w, int h, int realw, int realh);
    virtual void _Render(Image *img) = 0;
    virtual void _UpdateTick();

    void _DispatchEvents();
    virtual void _DispatchEvents(const EventHolder * /*ep*/, size_t /*num*/) {}

    CountedPtr<Image> renderOneImage();

    // write only by GUI thread!
    std::atomic_uint windowW, windowH, realWindowW, realWindowH;
    float pixelScale;
    bool mouseGrabbed;

private:

    // SDL stuff
    SDL_Window *_window;
    SDL_GLContext _glctx;
    SDLRenderer *_disp;
    unsigned _lastW, _lastH;
    Uint32 lastUpdateTime;

    bool _wantQuit;

    //SDLGuiThread *th;

    //friend class SDLGuiThread; // it must trigger above events

    // set by gui thread, r/w access protected by eventLock
    //std::mutex eventLock;
    std::vector<EventHolder> eventQ; // queues events until passed to impala
};


}



#endif

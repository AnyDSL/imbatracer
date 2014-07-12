#ifndef CG_IO_SDLGUI_H
#define CG_IO_SDLGUI_H

#include <core/refcounted.h>

struct SDL_Window;

namespace rt {

class SDLGuiThread;
class Image;

class SDLGui
{
public:
	SDLGui(unsigned width, unsigned height);
	virtual ~SDLGui();

	void Init();
    
	bool WaitingForQuit();

    // only call these functions from the GUI Thread!
    SDL_Window *GetWindow();
	void SetWindowTitle(const std::string &title);

protected:
	// called asynchronously, from GUI thread. Overload in children to react.
	virtual void _OnInit() {}
	virtual CountedPtr<Image> _Update(float dt);
	virtual bool _OnKey(int scancode, int key, int mod, bool down); //!< returns whether the keypress was handled
	virtual void _OnMouseButton(int /*button*/, int /*state*/, int /*x*/, int /*y*/) {}
	virtual void _OnMouseMotion(int /*xrel*/, int /*yrel*/) {}
	virtual void _OnMouseWheel(int /*x*/, int /*y*/) {}
	virtual void _OnWindowResize(int w, int h);
	virtual void _OnShutdown() {}

	// only available in GUI thread
	unsigned windowW, windowH;

private:
	SDLGuiThread *th;

	friend class SDLGuiThread; // it must trigger above events
};


}



#endif

#ifndef CG_IO_SDLGUI_H
#define CG_IO_SDLGUI_H

#include <core/refcounted.h>

class Waitable;

namespace rt {

class SDLGuiThread;
class Image;

class SDLGui
{
public:
	SDLGui();
	virtual ~SDLGui();

	virtual void SetWindowTitle(const char *title);

	virtual void Init();
	virtual void Resize(unsigned w, unsigned h);

	virtual void ShowImage(CountedPtr<Image> img);

	virtual void WaitForSpace();
	virtual void WaitForQuit();

	void getMouseMove(float *x, float *y);
	void setCursorVisible(bool on);
	void setGrabMouse(bool on);
	void setRenderOn(bool on);
	int getMouseWheelChange();
	bool isGrabMouse();

protected:
	// called asynchronously, from GUI thread. Overload in children to react.
	virtual void _OnInit();
	virtual void _Update(float dt);
	virtual bool _OnKey(int scancode, int key, int mod, bool down); //!< returns whether the keypress was handled
	virtual void _OnMouseButton(int button, int state,int x, int y);
	virtual void _OnMouseMotion(int xrel, int yrel);
	virtual void _OnMouseWheel(int x, int y);
	virtual void _OnWindowResize(int w, int h);
	virtual void _OnShutdown();

	// protected by th->waiter
	int mxaccu, myaccu; // accumulated mouse movement, over a frame
	int mouseWheelChange;
	int windowW, windowH;
	void resetMouse();

private:
	bool spaceWasPressed; // protected by th->waiter
	SDLGuiThread *th;

	friend class SDLGuiThread; // it must trigger above events
};


}



#endif

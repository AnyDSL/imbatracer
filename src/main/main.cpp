#include <io/sdlgui.h>
#include <io/image.h>
#include <SDL.h>
#include <stdio.h>


extern "C" void impala_render(unsigned *buf, int w, int h, void *statep);

using namespace rt;

// testing, POD struct
struct RenderState
{
    float dtaccu;
};

static void fillImageBuf(unsigned *buf, unsigned w, unsigned h, void *statep)
{
    /*
    RenderState *state = (RenderState*)statep;
	int t = (int)state->dtaccu;
	// TODO: call into impala instead
	for(unsigned y = 0; y < h; ++y)
		for(unsigned x = 0; x < w; ++x)
			buf[y * w + x] =
				(0xff << 24) // alpha
				| (t+(x^y^(rand()&0x40)) << 16) // blue
				| (y << 8) // green
				| x; // red
    */


    impala_render(buf, w, h, statep);
}

extern "C" void callbackTest(int x, int y)
{
    printf("callback: (%d, %d)\n", x, y);
}

class TestGui : public SDLGui
{
public:
	TestGui(unsigned w, unsigned h)
		: _img(new Image(w, h))
	{
		state.dtaccu = 0;
	}

protected:

	virtual void _Update(float dt)
	{
		// TODO: adjust to screen size (will crash on resize currently)
		fillImageBuf(_img->getPtr(), _img->width(), _img->height(), &state);
		ShowImage(_img);
		state.dtaccu += dt*20;
	}

	virtual void _OnWindowResize(int w, int h)
	{
		_img = new Image(w, h);
	}

	RenderState state;
	CountedPtr<Image> _img;
};

int main(int argc, char *argv[])
{
	SDL_Init(0);
	atexit(SDL_Quit);

	TestGui gui(640, 480);
	gui.Init();

	gui.SetWindowTitle("ImbaTracer");
	gui.WaitForQuit();


	return 0;
}

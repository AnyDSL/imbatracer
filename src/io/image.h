#ifndef CG1RAYTRACER_GLIMAGE_HEADER
#define CG1RAYTRACER_GLIMAGE_HEADER

#include <string>
#include <SDL.h>
#include <core/refcounted.h>

namespace rt {

class Image : public Refcounted
{
public:
	Image() {}
	Image(unsigned width, unsigned height);
	~Image();

	unsigned width() const { return (unsigned)surface->w; }
	unsigned height() const { return (unsigned)surface->h; }

	// pointer to raw 32 bit RGBA pixel data
	unsigned *getPtr() { return (unsigned*)surface->pixels; }
	const unsigned *getPtr() const { return (unsigned*)surface->pixels; }

	// for debugging
	void setPixel(unsigned x, unsigned y, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 0xFF)
	{
		((unsigned*)surface->pixels)[y * surface->w + x] = (a << 24) | (b << 16) | (g << 8) | r;
	}

private:
	SDL_Surface *surface;
};

} // end namespace rt

#endif

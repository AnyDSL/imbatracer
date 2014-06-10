#include "image.h"

namespace rt {

Image::Image(unsigned w, unsigned h)
{
	surface = SDL_CreateRGBSurface(0, w, h, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
}

Image::~Image()
{
	SDL_FreeSurface(surface);
}

} // end namespace rt

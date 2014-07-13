#include "image.h"

namespace rt {

Image::Image(unsigned w, unsigned h)
{
    surface = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0); // seems safe to use just 0 everywhere
}

Image::~Image()
{
    SDL_FreeSurface(surface);
}

} // end namespace rt

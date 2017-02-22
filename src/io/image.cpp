#include "image.h"
#include <runtime.h>

namespace rt {

Image::Image(unsigned w, unsigned h)
{
    //surface = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0); // seems safe to use just 0 everywhere
    rawmem = (void*)new char[w*h*4];
    surface = SDL_CreateRGBSurfaceFrom(rawmem, w, h, 32, w*4, 0, 0, 0, 0);
}

Image::~Image()
{
    SDL_FreeSurface(surface);
    delete[] (char*)rawmem;
}

} // end namespace rt

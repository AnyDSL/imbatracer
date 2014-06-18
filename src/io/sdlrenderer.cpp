#include "sdlrenderer.h"
#include <core/macros.h>
#include "image.h"

namespace rt {

SDLRenderer::SDLRenderer(SDL_Window *window)
: _renderer(nullptr)
, _tex(nullptr)
, _window(window)
{
}

SDLRenderer::~SDLRenderer()
{
}

bool SDLRenderer::Init()
{
	_renderer = SDL_CreateRenderer(_window, -1, 0);
	int w, h;
	SDL_GetWindowSize(_window, &w, &h);
	OnWindowResize(w, h);
	return _renderer && _tex;
}

void SDLRenderer::Shutdown()
{
	if(_tex)
		SDL_DestroyTexture(_tex);
	if(_renderer)
		SDL_DestroyRenderer(_renderer);

	_renderer = nullptr;
	_tex = nullptr;
}

void SDLRenderer::OnWindowResize(unsigned w, unsigned h)
{
	if(_tex)
		SDL_DestroyTexture(_tex);
    _tex = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
}

void SDLRenderer::uploadImage(const Image *img)
{
	SDL_UpdateTexture(_tex, NULL, img->getPtr(), img->width() * sizeof (unsigned));
}

void SDLRenderer::BeginFrame()
{
	SDL_RenderClear(_renderer);
}

void SDLRenderer::render()
{
	SDL_RenderCopy(_renderer, _tex, NULL, NULL);
}

void SDLRenderer::EndFrame()
{
	SDL_RenderPresent(_renderer);
}




} // end namespace rt

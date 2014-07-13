#ifndef _SDL_RENDERER_H
#define _SDL_RENDERER_H

#include <SDL.h>

namespace rt {

class Image;

class SDLRenderer
{
public:

    SDLRenderer(SDL_Window *window);
    ~SDLRenderer();

    bool Init();
    void Shutdown();

    void OnWindowResize(unsigned w, unsigned h);

    void BeginFrame();
    void EndFrame();

    void uploadImage(const Image *img);
    void render();

private:

    SDL_Renderer *_renderer;
    SDL_Texture *_tex;
    SDL_Window *_window;

};


} // end namespace rt

#endif

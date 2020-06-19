#include <SDL.h>

#include <stdio.h>

#include <stdexcept>

// TODO: Change this so throwing an exception is not necessary
class SdlContext {
public:
    SdlContext() {
        if (SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error("SDL could not be initialised.");
        }
    }
    ~SdlContext() {
        SDL_Quit();
    }
};

int main() {
    SdlContext sdl_context;

    // FIXME: Check these function calls for errors
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    SDL_Window* window =
        SDL_CreateWindow("4dtest", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_OPENGL);
    if (!window) {
        return 1;
    }

    if (!SDL_GL_CreateContext(window)) {
        return 1;
    }

    getchar();

    return 0;
}

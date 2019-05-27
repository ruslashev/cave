#include "gfx.h"
#include <SDL2/SDL.h>

static unsigned int window_width, window_height;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t *pixels;

void gfx_init(unsigned int _window_width, unsigned int _window_height)
{
    window_width = _window_width;
    window_height = _window_height;

    if (SDL_Init(SDL_INIT_VIDEO) == -1)
        die("failed to init SDL: %s", SDL_GetError());

    window = SDL_CreateWindow("cave", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            window_width, window_height, SDL_WINDOW_SHOWN);
    if (!window)
        die("failed to create window: %s", SDL_GetError());

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
        die("failed to create renderer: %s", SDL_GetError());

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING,
            window_width, window_height);

    pixels = malloc(window_width * window_height * sizeof(uint32_t));
    if (pixels == NULL)
        die("failed to malloc pixels");
}

void gfx_draw()
{
    SDL_UpdateTexture(texture, NULL, pixels, window_width * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

int gfx_update()
{
    SDL_Event event;

    while (SDL_PollEvent(&event) != 0)
        if (event.type == SDL_QUIT)
            return 0;

    return 1;
}

void gfx_clear()
{
    memset(pixels, 0, window_width * window_height * sizeof(pixels[0]));
}

void gfx_set_pixel(int x, int y, uint32_t color)
{
    pixels[y * window_width + x] = color;
}

void gfx_destroy()
{
    free(pixels);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}


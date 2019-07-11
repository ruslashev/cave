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

static char sdlkey_to_char(SDL_Keycode kc)
{
    switch (kc) {
    case SDLK_a:      return 'a';
    case SDLK_d:      return 'd';
    case SDLK_e:      return 'e';
    case SDLK_q:      return 'q';
    case SDLK_r:      return 'r';
    case SDLK_s:      return 's';
    case SDLK_w:      return 'w';
    case SDLK_z:      return 'z';
    case SDLK_COMMA:  return ',';
    case SDLK_PERIOD: return '.';
    case SDLK_SPACE:  return ' ';
    case SDLK_LCTRL:  return GFX_CTRL;
    case SDLK_LSHIFT: return GFX_SHIFT;
    case SDLK_UP:     return GFX_UP;
    case SDLK_DOWN:   return GFX_DOWN;
    case SDLK_LEFT:   return GFX_LEFT;
    case SDLK_RIGHT:  return GFX_RIGHT;
    case SDLK_MINUS:  return '-';
    case SDLK_EQUALS: return '=';
    default:          return -1;
    }
}

int gfx_update(key_event_t key_event_cb)
{
    SDL_Event event;
    char key;

    while (SDL_PollEvent(&event) != 0)
        if (event.type == SDL_QUIT)
            return 0;
        else if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && event.key.repeat == 0) {
            key = sdlkey_to_char(event.key.keysym.sym);
            if (key != -1)
                key_event_cb(key, event.type == SDL_KEYDOWN);
        }

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


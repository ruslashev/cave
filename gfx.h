#pragma once

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#define log(...) do { printf(__VA_ARGS__); puts(""); } while (0)
#define die(...) do { log(__VA_ARGS__); exit(1); } while (0)

typedef void (*key_event_t)(char, int);

enum {
    GFX_CTRL  = 1,
    GFX_SHIFT = 2,
    GFX_UP    = 3,
    GFX_DOWN  = 4,
    GFX_LEFT  = 5,
    GFX_RIGHT = 6,
};

void gfx_init(unsigned int _window_width, unsigned int _window_height);
void gfx_draw();
int gfx_update(key_event_t key_event_cb);
void gfx_clear();
void gfx_set_pixel(int x, int y, uint32_t color);
void gfx_destroy();


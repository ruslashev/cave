#pragma once

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#define log(...) do { printf(__VA_ARGS__); puts(""); } while (0)
#define die(...) do { log(__VA_ARGS__); exit(1); } while (0)

void gfx_init(unsigned int _window_width, unsigned int _window_height);
void gfx_draw();
int gfx_update();
void gfx_clear();
void gfx_set_pixel(int x, int y, uint32_t color);
void gfx_destroy();


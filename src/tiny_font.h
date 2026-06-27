#ifndef TINY_FONT_H
#define TINY_FONT_H

#include <stdint.h>

#define TINY_FONT_TILE_COUNT 57
#define TINY_FONT_CHARS " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/:.-!?@ab+<>^vx*se()"

extern const uint8_t tiny_font_tiles[];

/*
 * Returns the tile ID for c, assuming tiny_font_tiles was loaded at tile_base.
 * Unknown characters resolve to the space tile.
 */
uint8_t tiny_font_tile_for_char(uint8_t tile_base, char c);

#endif

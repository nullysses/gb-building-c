#ifndef WINDOW_TEXT_H
#define WINDOW_TEXT_H

#include <stdint.h>

#define WIN_W 20u
#define WIN_H 2u

void window_clear(uint8_t font_base);
void window_draw_row(uint8_t font_base, uint8_t y, const char *text);
void window_draw_scroller(uint8_t font_base, uint8_t y, const char *msg, uint8_t pos);

#endif

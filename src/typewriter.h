#ifndef TYPEWRITER_H
#define TYPEWRITER_H

#include <stdint.h>

typedef struct typewriter_t {
    const char *text;

    uint8_t font_base;

    uint8_t box_x;
    uint8_t box_y;
    uint8_t box_w;
    uint8_t box_h;

    uint8_t cursor_x;
    uint8_t cursor_y;

    uint8_t ticks_per_char;
    uint8_t tick;

    uint8_t done;
} typewriter_t;

void typewriter_clear_box(uint8_t font_base, uint8_t x, uint8_t y, uint8_t w, uint8_t h);

void typewriter_start(
    typewriter_t *tw,
    uint8_t font_base,
    const char *text,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h,
    uint8_t ticks_per_char
);

void typewriter_update(typewriter_t *tw);
void typewriter_reveal_all(typewriter_t *tw);

#endif
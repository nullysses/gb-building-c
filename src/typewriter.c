#include <gb/gb.h>
#include <stdint.h>

#include "tiny_font.h"
#include "typewriter.h"

void typewriter_clear_box(uint8_t font_base, uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    uint8_t row[20];
    uint8_t ix;
    uint8_t iy;
    uint8_t space = tiny_font_tile_for_char(font_base, ' ');

    if (w > 20u) {
        w = 20u;
    }

    for (ix = 0u; ix < w; ++ix) {
        row[ix] = space;
    }

    for (iy = 0u; iy < h; ++iy) {
        set_win_tiles(x, (uint8_t)(y + iy), w, 1u, row);
    }
}

void typewriter_start(
    typewriter_t *tw,
    uint8_t font_base,
    const char *text,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h,
    uint8_t ticks_per_char
) {
    tw->text = text;
    tw->font_base = font_base;

    tw->box_x = x;
    tw->box_y = y;
    tw->box_w = w;
    tw->box_h = h;

    tw->cursor_x = 0u;
    tw->cursor_y = 0u;

    tw->ticks_per_char = ticks_per_char;
    tw->tick = 0u;
    tw->done = 0u;

    typewriter_clear_box(font_base, x, y, w, h);
}

static void typewriter_newline(typewriter_t *tw) {
    tw->cursor_x = 0u;
    tw->cursor_y++;

    if (tw->cursor_y >= tw->box_h) {
        tw->done = 1u;
    }
}

static void typewriter_put_char(typewriter_t *tw, char c) {
    uint8_t tile;

    if (tw->done) {
        return;
    }

    if (c == '\0') {
        tw->done = 1u;
        return;
    }

    if (c == '\n') {
        typewriter_newline(tw);
        return;
    }

    tile = tiny_font_tile_for_char(tw->font_base, c);

    set_win_tiles(
        (uint8_t)(tw->box_x + tw->cursor_x),
        (uint8_t)(tw->box_y + tw->cursor_y),
        1u,
        1u,
        &tile
    );

    tw->cursor_x++;

    if (tw->cursor_x >= tw->box_w) {
        typewriter_newline(tw);
    }
}

void typewriter_update(typewriter_t *tw) {
    if (tw->done) {
        return;
    }

    tw->tick++;

    if (tw->tick < tw->ticks_per_char) {
        return;
    }

    tw->tick = 0u;

    typewriter_put_char(tw, *(tw->text));

    if (!tw->done) {
        tw->text++;
    }
}

void typewriter_reveal_all(typewriter_t *tw) {
    while (!tw->done) {
        typewriter_put_char(tw, *(tw->text));

        if (!tw->done) {
            tw->text++;
        }
    }
}
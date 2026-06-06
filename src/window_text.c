#include <gb/gb.h>
#include <stdint.h>

#include "tiny_font.h"
#include "window_text.h"

void window_clear(uint8_t font_base) {
    uint8_t tiles[WIN_W * WIN_H];
    uint8_t i;

    for (i = 0u; i < (WIN_W * WIN_H); ++i) {
        tiles[i] = tiny_font_tile_for_char(font_base, ' ');
    }

    set_win_tiles(0u, 0u, WIN_W, WIN_H, tiles);
}

void window_draw_row(uint8_t font_base, uint8_t y, const char *text) {
    uint8_t row[WIN_W];
    uint8_t i;
    uint8_t ended = 0u;

    for (i = 0u; i < WIN_W; ++i) {
        char c;

        if (ended) {
            c = ' ';
        } else {
            c = text[i];

            if (c == '\0') {
                c = ' ';
                ended = 1u;
            }
        }

        row[i] = tiny_font_tile_for_char(font_base, c);
    }

    set_win_tiles(0u, y, WIN_W, 1u, row);
}

void window_draw_scroller(uint8_t font_base, uint8_t y, const char *msg, uint8_t pos) {
    uint8_t row[WIN_W];
    uint8_t i;
    uint8_t len = 0u;

    while (msg[len]) {
        ++len;
    }

    if (len == 0u) {
        window_draw_row(font_base, y, "");
        return;
    }

    for (i = 0u; i < WIN_W; ++i) {
        row[i] = tiny_font_tile_for_char(
            font_base,
            msg[(uint8_t)(pos + i) % len]
        );
    }

    set_win_tiles(0u, y, WIN_W, 1u, row);
}
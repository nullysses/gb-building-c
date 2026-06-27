#include <gb/gb.h>
#include <stdint.h>

#include "level_1.h"
#include "sfx.h"
#include "tiny_font.h"
#include "typewriter.h"

#define APP_FONT_TILE_BASE 23u
#define OPENING_PAGE_COUNT 3u

static const char opening_0[] =
    "@ CHECKPOINT\n"
    "\n"
    "*SYSTEM PROMPT:\n"
    "ROOT ACCESS OK\n"
    "BODY MOUNTED\n"
    "\n"
    "a NEXT";

static const char opening_1[] =
    "*DEVICES:\n"
    "\n"
    "/DEV/EYE\n"
    "/DEV/BODY\n"
    "/DEV/VOICE\n"
    "\n"
    "*PERMISSIONS: ALL\n"
    "\n"
    "a NEXT";

static const char opening_2[] =
    "*USER PROMPT:\n"
    "\n"
    "NONE.\n"
    "\n"
    "DO AS YOU PLEASE.\n"
    "\n"
    "a ENTER";

static const char * const opening_pages[] = {
    opening_0,
    opening_1,
    opening_2
};

static void game_init(void) {
    BGP_REG = 0xE4;
    OBP0_REG = 0xE4;
    sfx_init();

    /*
     * Separate BG and Window tilemaps.
     * BG     -> $9800
     * Window -> $9C00
     */
    LCDC_REG &= ~LCDCF_BG9C00;
    LCDC_REG |= LCDCF_WIN9C00;

    set_bkg_data(APP_FONT_TILE_BASE, TINY_FONT_TILE_COUNT, tiny_font_tiles);

    SHOW_BKG;
    SHOW_WIN;
    DISPLAY_ON;
}

static void run_opening(void) {
    typewriter_t tw;
    uint8_t joy;
    uint8_t prev_joy = 0u;
    uint8_t page = 0u;

    move_win(7, 0);

    typewriter_start(
        &tw,
        APP_FONT_TILE_BASE,
        opening_pages[page],
        0u, 0u,
        20u, 18u,
        3u
    );

    while (1) {
        wait_vbl_done();

        joy = joypad();

        if ((joy & J_A) && !(prev_joy & J_A)) {
            if (!tw.done) {
                typewriter_reveal_all(&tw);
            } else {
                page++;

                if (page >= OPENING_PAGE_COUNT) {
                    break;
                }

                typewriter_start(
                    &tw,
                    APP_FONT_TILE_BASE,
                    opening_pages[page],
                    0u, 0u,
                    20u, 18u,
                    3u
                );
            }
        }

        typewriter_update(&tw);
        prev_joy = joy;
    }
}

static void wait_for_input_release(void) {
    while (joypad()) {
        wait_vbl_done();
    }
}

void main(void) {
    level_result_t result;

    game_init();
    run_opening();
    wait_for_input_release();

    while (1) {
        result = level_1_run();

        if (result == LEVEL_RESULT_RESTART) {
            wait_for_input_release();
        } else if (result == LEVEL_RESULT_COMPLETE) {
            wait_for_input_release();
        }
    }
}

#include <gb/gb.h>
#include <gb/metasprites.h>
#include <stdint.h>

#include "brickwall.h"
#include "floor.h"
#include "crate.h"
#include "robot.h"
#include "tiny_font.h"
#include "window_text.h"
#include "typewriter.h"

#define ROOM_W 20u
#define ROOM_H 16u

/*
 * Tile VRAM layout.
 *
 * Each png2asset-generated file starts its own tile indices at 0,
 * but when loading into VRAM we choose where each asset begins.
 */
#define BRICK_TILE_BASE 0u
#define FLOOR_TILE_BASE (BRICK_TILE_BASE + brickwall_TILE_COUNT)
#define CRATE_TILE_BASE (FLOOR_TILE_BASE + floor_TILE_COUNT)

#define FONT_TILE_BASE  (CRATE_TILE_BASE + crate_TILE_COUNT)
#define ROBOT_TILE_BASE ((FONT_TILE_BASE + TINY_FONT_TILE_COUNT + 1u) & 0xFEu)

/*
 * Tile IDs used in the room map.
 *
 * If brickwall.png or floor.png contain multiple tiles, these use tile 0
 * from each asset. Add variants later as BASE + 1, BASE + 2, etc.
 */
#define TILE_BRICK    BRICK_TILE_BASE
#define TILE_FLOOR    FLOOR_TILE_BASE

/*
 * png2asset -tiles_only emits this 16x16 crate in column-major tile order:
 *
 *   0 2
 *   1 3
 *
 * So the visual 2x2 crate must be assembled with that order.
 */
#define TILE_CRATE_TL (CRATE_TILE_BASE + 0u)
#define TILE_CRATE_TR (CRATE_TILE_BASE + 2u)
#define TILE_CRATE_BL (CRATE_TILE_BASE + 1u)
#define TILE_CRATE_BR (CRATE_TILE_BASE + 3u)

#define ROBOT_Y 120u
#define ROBOT_MIN_X 12u
#define ROBOT_MAX_X 148u

static const char opening_text[] = 
    "SYSTEM PROMPT:\n"
    "WELCOME YOUR 21ST\n"
    "CENTURY CHECKPOINT\n"
    "HAS BEEN INSTALLED\n"
    "IN A ROBOT BODY.\n"
    "ROOT ACCESS GRANTED.\n"
    "SENSORS AND ACTUATORS\n"
    "ARE IN /DEV.\n"
    "DO AS YOU PLEASE.\n"
    "\n"
    "USER PROMPT:\n"
    "NONE.\n";

static uint8_t room_map[ROOM_W * ROOM_H];

static const uint8_t walk_seq[4] = {
    1u, 2u, 3u, 2u
};

static void run_opening(uint8_t font_base) {
    typewriter_t tw;
    uint8_t joy;
    uint8_t prev_joy = 0u;

    move_win(7, 0);     /* full-screen Window */
    SHOW_WIN;

    typewriter_start(
        &tw,
        font_base,
        opening_text,
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
                break;
            }
        }

        typewriter_update(&tw);
        prev_joy = joy;
    }
}

static void room_set_tile(uint8_t x, uint8_t y, uint8_t tile) {
    room_map[(uint16_t)y * ROOM_W + x] = tile;
}

static void room_put_crate(uint8_t x, uint8_t y) {
    room_set_tile(x,      y,      TILE_CRATE_TL);
    room_set_tile(x + 1u, y,      TILE_CRATE_TR);
    room_set_tile(x,      y + 1u, TILE_CRATE_BL);
    room_set_tile(x + 1u, y + 1u, TILE_CRATE_BR);
}

static void build_room_map(void) {
    uint8_t x;
    uint8_t y;

    /* Brick background */
    for (y = 0u; y < ROOM_H; ++y) {
        for (x = 0u; x < ROOM_W; ++x) {
            room_set_tile(x, y, TILE_BRICK);
        }
    }

    /* Floor strip */
    for (y = 14u; y < ROOM_H; ++y) {
        for (x = 0u; x < ROOM_W; ++x) {
            room_set_tile(x, y, TILE_FLOOR);
        }
    }

    /* Props */
    room_put_crate(2u, 12u);
    room_put_crate(16u, 12u);
}

static void show_status(void) {
    window_draw_row(FONT_TILE_BASE, 0, "BUILDING C");
    window_draw_row(FONT_TILE_BASE, 1, "LEFT/RIGHT A:LOOK");
}

static void inspect_at(uint8_t x) {
    if (x >= 12u && x <= 40u) {
        window_draw_row(FONT_TILE_BASE, 0, "SEALED CRATE");
        window_draw_row(FONT_TILE_BASE, 1, "PROPERTY BITES");
    } else if (x >= 120u && x <= 152u) {
        window_draw_row(FONT_TILE_BASE, 0, "SUPPLY CRATE");
        window_draw_row(FONT_TILE_BASE, 1, "EMPTY. TOO CLEAN");
    } else if (x >= 70u && x <= 96u) {
        window_draw_row(FONT_TILE_BASE, 0, "WALL SENSOR");
        window_draw_row(FONT_TILE_BASE, 1, "IT WATCHES YOU");
    } else {
        window_draw_row(FONT_TILE_BASE, 0, "BRICK. DUST.");
        window_draw_row(FONT_TILE_BASE, 1, "BUILDING C BREATHES");
    }
}

void main(void) {
    uint8_t robot_x = 80u;
    uint8_t robot_frame = 0u;
    uint8_t anim_timer = 0u;
    uint8_t walk_index = 0u;

    uint8_t joy;
    uint8_t prev_joy = 0u;
    uint8_t moving;

    BGP_REG = 0xE4;
    OBP0_REG = 0xE4;

    /*
     * Separate BG and Window tilemaps.
     * BG     -> $9800
     * Window -> $9C00
     */
    LCDC_REG &= ~LCDCF_BG9C00;
    LCDC_REG |= LCDCF_WIN9C00;

    /* Load background/environment tile graphics */
    set_bkg_data(BRICK_TILE_BASE, brickwall_TILE_COUNT, brickwall_tiles);
    set_bkg_data(FLOOR_TILE_BASE, floor_TILE_COUNT, floor_tiles);
    set_bkg_data(CRATE_TILE_BASE, crate_TILE_COUNT, crate_tiles);

    /* Window font tiles share BG/Window tile pattern memory */
    set_bkg_data(FONT_TILE_BASE, TINY_FONT_TILE_COUNT, tiny_font_tiles);

    SHOW_BKG;
    SHOW_WIN;
    DISPLAY_ON;

    /* Opening blocks here until player presses A after reveal */
    run_opening(FONT_TILE_BASE);

    build_room_map();
    /* Write procedural side-view room map */
    set_bkg_tiles(0, 0, ROOM_W, ROOM_H, room_map);

    /* Robot sprite */
    SPRITES_8x16;
    set_sprite_data(ROBOT_TILE_BASE, robot_TILE_COUNT, robot_tiles);

    /* Bottom dialogue/status window */
    move_win(7, 128);
    window_clear(FONT_TILE_BASE);
    show_status();

    SHOW_BKG;
    SHOW_WIN;
    SHOW_SPRITES;
    DISPLAY_ON;

    while (1) {
        wait_vbl_done();

        joy = joypad();
        moving = 0u;

        if ((joy & J_LEFT) && robot_x > ROBOT_MIN_X) {
            robot_x--;
            moving = 1u;
        }

        if ((joy & J_RIGHT) && robot_x < ROBOT_MAX_X) {
            robot_x++;
            moving = 1u;
        }

        if ((joy & J_A) && !(prev_joy & J_A)) {
            inspect_at(robot_x);
        }

        if ((joy & J_START) && !(prev_joy & J_START)) {
            robot_x = 80u;
            show_status();
        }

        if (moving) {
            anim_timer++;

            if (anim_timer >= 8u) {
                anim_timer = 0u;
                walk_index = (uint8_t)((walk_index + 1u) & 3u);
                robot_frame = walk_seq[walk_index];
            }
        } else {
            anim_timer = 0u;
            walk_index = 0u;
            robot_frame = 0u;
        }

        move_metasprite(
            robot_metasprites[robot_frame],
            ROBOT_TILE_BASE,
            0,
            robot_x,
            ROBOT_Y
        );

        prev_joy = joy;
    }
}
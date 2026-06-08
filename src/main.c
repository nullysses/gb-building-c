#include <gb/gb.h>
#include <gb/metasprites.h>
#include <stdint.h>

#include "brickwall.h"
#include "floor.h"
#include "crate.h"
#include "door.h"
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
#define DOOR_TILE_BASE  (CRATE_TILE_BASE + crate_TILE_COUNT)

#define FONT_TILE_BASE  (DOOR_TILE_BASE + door_TILE_COUNT)
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

#define DOOR_W 3u
#define DOOR_H 4u
#define DOOR_X 16u
#define DOOR_Y 10u

#define ROBOT_Y 120u
#define ROBOT_MIN_X 8u
#define ROBOT_MAX_X 160u
#define OPENING_PAGE_COUNT 3u

#define ROOM_WEST 0u
#define ROOM_EAST 1u

#define ROBOT_FACE_RIGHT 0u
#define ROBOT_FACE_LEFT  1u

#define HOTSPOT_COUNT ((uint8_t)(sizeof(hotspots) / sizeof(hotspots[0])))

typedef struct hotspot_t {
    uint8_t room;
    uint8_t x_min;
    uint8_t x_max;
    const char *line_0;
    const char *line_1;
} hotspot_t;

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

static const hotspot_t hotspots[] = {
    { ROOM_WEST, 12u, 40u,   "SEALED CRATE",  "???" },
    { ROOM_WEST, 70u, 96u,   "WALL SENSOR",   "IT WATCHES YOU" },
    { ROOM_WEST, 120u, 152u, "SUPPLY CRATE",  "EMPTY. TOO CLEAN" },
    { ROOM_EAST, 70u, 96u,   "WALL SENSOR",   "IT WATCHES YOU" },
    { ROOM_EAST, 128u, 160u, "DOOR LOCKED",   "NO PROMPT FOUND" }
};

static uint8_t room_map[ROOM_W * ROOM_H];

static const uint8_t walk_seq[2] = {
    0u, 1u
};

static void run_opening(uint8_t font_base) {
    typewriter_t tw;
    uint8_t joy;
    uint8_t prev_joy = 0u;
    uint8_t page = 0u;

    move_win(7, 0);     /* full-screen Window */
    SHOW_BKG;
    SHOW_WIN;
    DISPLAY_ON;

    typewriter_start(
        &tw,
        font_base,
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
                    prev_joy = joy;
                    break;
                }

                typewriter_start(
                    &tw,
                    font_base,
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

static void room_set_tile(uint8_t x, uint8_t y, uint8_t tile) {
    room_map[(uint16_t)y * ROOM_W + x] = tile;
}

static void room_put_crate(uint8_t x, uint8_t y) {
    room_set_tile(x,      y,      TILE_CRATE_TL);
    room_set_tile(x + 1u, y,      TILE_CRATE_TR);
    room_set_tile(x,      y + 1u, TILE_CRATE_BL);
    room_set_tile(x + 1u, y + 1u, TILE_CRATE_BR);
}

static void room_put_door(uint8_t x, uint8_t y) {
    uint8_t door_x;
    uint8_t door_y;

    for (door_y = 0u; door_y < DOOR_H; ++door_y) {
        for (door_x = 0u; door_x < DOOR_W; ++door_x) {
            room_set_tile(
                x + door_x,
                y + door_y,
                DOOR_TILE_BASE + door_map[(uint16_t)door_y * DOOR_W + door_x]
            );
        }
    }
}

static void build_room_map(uint8_t room) {
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

    if (room == ROOM_WEST) {
        room_put_crate(2u, 12u);
        room_put_crate(16u, 12u);
    } else {
        room_put_door(DOOR_X, DOOR_Y);
    }
}

static void draw_room(uint8_t room) {
    build_room_map(room);
    set_bkg_tiles(0, 0, ROOM_W, ROOM_H, room_map);
}

static void show_status(uint8_t room) {
    if (room == ROOM_EAST) {
        window_draw_row(FONT_TILE_BASE, 0, "BUILDING C EAST");
    } else {
        window_draw_row(FONT_TILE_BASE, 0, "BUILDING C WEST");
    }

    window_draw_row(FONT_TILE_BASE, 1, "+:MOVE a:LOOK");
}

static void inspect_at(uint8_t room, uint8_t x) {
    uint8_t i;

    for (i = 0u; i < HOTSPOT_COUNT; ++i) {
        const hotspot_t *hotspot = &hotspots[i];

        if (
            hotspot->room == room &&
            x >= hotspot->x_min &&
            x <= hotspot->x_max
        ) {
            window_draw_row(FONT_TILE_BASE, 0, hotspot->line_0);
            window_draw_row(FONT_TILE_BASE, 1, hotspot->line_1);
            return;
        }
    }

    window_draw_row(FONT_TILE_BASE, 0, "BRICK. DUST.");
    window_draw_row(FONT_TILE_BASE, 1, "BUILDING C BREATHES");
}

void main(void) {
    uint8_t robot_x = 80u;
    uint8_t room = ROOM_WEST;
    uint8_t robot_facing = ROBOT_FACE_RIGHT;
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
    set_bkg_data(DOOR_TILE_BASE, door_TILE_COUNT, door_tiles);

    /* Window font tiles share BG/Window tile pattern memory */
    set_bkg_data(FONT_TILE_BASE, TINY_FONT_TILE_COUNT, tiny_font_tiles);

    SHOW_BKG;
    SHOW_WIN;
    DISPLAY_ON;

    /* Opening blocks here until player presses A after reveal */
    run_opening(FONT_TILE_BASE);
    wait_for_input_release();

    draw_room(room);

    /* Robot sprite */
    SPRITES_8x16;
    set_sprite_data(ROBOT_TILE_BASE, robot_TILE_COUNT, robot_tiles);

    /* Bottom dialogue/status window */
    move_win(7, 128);
    window_clear(FONT_TILE_BASE);
    show_status(room);

    SHOW_BKG;
    SHOW_WIN;
    SHOW_SPRITES;
    DISPLAY_ON;

    while (1) {
        wait_vbl_done();
        
        joy = joypad();
        moving = 0u;

        if (joy & J_LEFT) {
            if (robot_x > ROBOT_MIN_X) {
                robot_x--;
                moving = 1u;
                robot_facing = ROBOT_FACE_LEFT;
            } else if (room == ROOM_EAST) {
                room = ROOM_WEST;
                robot_x = ROBOT_MAX_X;
                draw_room(room);
                moving = 1u;
                robot_facing = ROBOT_FACE_LEFT;
            }

            if (moving) {
                show_status(room);
            }
        }

        if (joy & J_RIGHT) {
            if (robot_x < ROBOT_MAX_X) {
                robot_x++;
                moving = 1u;
                robot_facing = ROBOT_FACE_RIGHT;
            } else if (room == ROOM_WEST) {
                room = ROOM_EAST;
                robot_x = ROBOT_MIN_X;
                draw_room(room);
                moving = 1u;
                robot_facing = ROBOT_FACE_RIGHT;
            }

            if (moving) {
                show_status(room);
            }
        }

        if ((joy & J_A) && !(prev_joy & J_A)) {
            inspect_at(room, robot_x);
        }

        if ((joy & J_START) && !(prev_joy & J_START)) {
            room = ROOM_WEST;
            robot_x = 80u;
            robot_facing = ROBOT_FACE_RIGHT;
            draw_room(room);
            show_status(room);
        }

        if (moving) {
            anim_timer++;

            if (anim_timer >= 10u) {
                anim_timer = 0u;
                robot_frame ^= 1u;   /* toggles 0 <-> 1 */
            }
        } else {
            anim_timer = 0u;
            robot_frame = 0u;
        }

        if (robot_facing == ROBOT_FACE_LEFT) {
            move_metasprite_flipx(
                robot_metasprites[robot_frame],
                ROBOT_TILE_BASE,
                0,
                0,
                robot_x,
                ROBOT_Y
            );
        } else {
            move_metasprite(
                robot_metasprites[robot_frame],
                ROBOT_TILE_BASE,
                0,
                robot_x,
                ROBOT_Y
            );
        }

        prev_joy = joy;
    }
}

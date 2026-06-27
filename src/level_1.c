#include <gb/gb.h>
#include <gb/metasprites.h>
#include <stdint.h>

#include "level_1.h"
#include "brickwall.h"
#include "floor.h"
#include "crate.h"
#include "door.h"
#include "exit_sign.h"
#include "inventory.h"
#include "robot.h"
#include "sfx.h"
#include "tiny_font.h"
#include "window_text.h"
#include "typewriter.h"
#include "audio/music.h"

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
#define EXIT_SIGN_TILE_BASE (DOOR_TILE_BASE + door_TILE_COUNT)

#define FONT_TILE_BASE  (EXIT_SIGN_TILE_BASE + exit_sign_TILE_COUNT)
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
#define EAST_DOOR_X 16u
#define NORTH_DOOR_X (ROOM_W - EAST_DOOR_X - DOOR_W)
#define DOOR_Y 10u
#define MAX_DOORS_PER_ROOM 3u
#define DOOR_INDEX_NONE 0xFFu
#define DOOR_ACTION_OPEN 0u
#define DOOR_ACTION_LOCKED 1u

#define EXIT_SIGN_W 3u
#define EXIT_SIGN_X 16u
#define EXIT_SIGN_Y 8u

#define ROBOT_Y 120u
#define ROBOT_MIN_X 8u
#define ROBOT_MAX_X 160u
#define STATUS_WIN_Y 128u
#define INVENTORY_WIN_Y 0u
#define INVENTORY_HIDDEN_WIN_Y 144u
#define FULLSCREEN_WIN_H 18u
#define INVENTORY_FIRST_ROW 3u

#define ROOM_WEST 0u
#define ROOM_EAST 1u
#define ROOM_NORTH 2u
#define ROOM_NORTH_WEST 3u
#define ROOM_NORTH_EAST 4u

#define ROBOT_FACE_RIGHT 0u
#define ROBOT_FACE_LEFT  1u

#define STATUS_PROMPT_LOOK 0u
#define STATUS_PROMPT_OPEN 1u
#define STATUS_PROMPT_LOCKED 2u
#define STATUS_PROMPT_NONE 0xFFu

#define EAST_DOOR_HOTSPOT_X_MIN 128u
#define EAST_DOOR_HOTSPOT_X_MAX 160u
#define NORTH_DOOR_HOTSPOT_X_MIN 8u
#define NORTH_DOOR_HOTSPOT_X_MAX 40u
#define NORTH_LOCKED_DOOR_HOTSPOT_X_MIN 128u
#define NORTH_LOCKED_DOOR_HOTSPOT_X_MAX 160u
#define NORTH_EAST_CRATE_X 16u
#define NORTH_EAST_CRATE_Y 12u
#define NORTH_EAST_CRATE_HOTSPOT_X_MIN 120u
#define NORTH_EAST_CRATE_HOTSPOT_X_MAX 152u

#define HOTSPOT_COUNT ((uint8_t)(sizeof(hotspots) / sizeof(hotspots[0])))
#define DOOR_COUNT ((uint8_t)(sizeof(doors) / sizeof(doors[0])))

typedef struct hotspot_t {
    uint8_t room;
    uint8_t x_min;
    uint8_t x_max;
    const char *line_0;
    const char *line_1;
} hotspot_t;

typedef struct door_t {
    uint8_t room;
    uint8_t index;
    uint8_t tile_x;
    uint8_t tile_y;
    uint8_t x_min;
    uint8_t x_max;
    uint8_t action;
    uint8_t dest_room;
    uint8_t dest_x;
} door_t;

typedef struct world_state_t {
    uint8_t room;
    uint8_t robot_x;
    uint8_t north_east_crate_looted;
    uint8_t north_locked_door_unlocked;
    uint8_t north_room;
    uint8_t north_locked_door_x_min;
    uint8_t north_locked_door_x_max;
} world_state_t;

typedef struct level_1_runtime_t {
    uint8_t robot_facing;
    uint8_t robot_frame;
    uint8_t anim_timer;
    uint8_t previous_joy;
    uint8_t shown_room;
    uint8_t shown_prompt;
    uint8_t inventory_open;
    uint8_t paused;
} level_1_runtime_t;

static const hotspot_t hotspots[] = {
    { ROOM_WEST, 12u, 40u,   "SEALED CRATE",  "???" },
    { ROOM_WEST, 70u, 96u,   "WALL SENSOR",   "IT WATCHES YOU" },
    { ROOM_WEST, 120u, 152u, "SUPPLY CRATE",  "EMPTY. TOO CLEAN" },
    { ROOM_EAST, 70u, 96u,   "WALL SENSOR",   "IT WATCHES YOU" }
};

static const door_t doors[] = {
    {
        ROOM_EAST,
        0u,
        EAST_DOOR_X,
        DOOR_Y,
        EAST_DOOR_HOTSPOT_X_MIN,
        EAST_DOOR_HOTSPOT_X_MAX,
        DOOR_ACTION_OPEN,
        ROOM_NORTH,
        NORTH_DOOR_HOTSPOT_X_MAX
    },
    {
        ROOM_NORTH,
        0u,
        NORTH_DOOR_X,
        DOOR_Y,
        NORTH_DOOR_HOTSPOT_X_MIN,
        NORTH_DOOR_HOTSPOT_X_MAX,
        DOOR_ACTION_OPEN,
        ROOM_EAST,
        EAST_DOOR_HOTSPOT_X_MIN
    },
    {
        ROOM_NORTH,
        1u,
        EAST_DOOR_X,
        DOOR_Y,
        NORTH_LOCKED_DOOR_HOTSPOT_X_MIN,
        NORTH_LOCKED_DOOR_HOTSPOT_X_MAX,
        DOOR_ACTION_LOCKED,
        ROOM_NORTH_EAST,
        ROBOT_MIN_X
    }
};

static uint8_t room_map[ROOM_W * ROOM_H];

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

static void room_put_exit_sign(uint8_t x, uint8_t y) {
    uint8_t sign_x;

    for (sign_x = 0u; sign_x < EXIT_SIGN_W; ++sign_x) {
        room_set_tile(
            x + sign_x,
            y,
            EXIT_SIGN_TILE_BASE + exit_sign_map[sign_x]
        );
    }
}

static void build_room_map(uint8_t room) {
    uint8_t x;
    uint8_t y;
    uint8_t i;

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
    } else if (room == ROOM_NORTH_EAST) {
        room_put_crate(NORTH_EAST_CRATE_X, NORTH_EAST_CRATE_Y);
    }

    for (i = 0u; i < DOOR_COUNT; ++i) {
        if (doors[i].room == room && doors[i].index < MAX_DOORS_PER_ROOM) {
            room_put_door(doors[i].tile_x, doors[i].tile_y);
        }
    }

    if (room == ROOM_NORTH) {
        room_put_exit_sign(EXIT_SIGN_X, EXIT_SIGN_Y);
    }
}

static void draw_room(uint8_t room) {
    build_room_map(room);
    set_bkg_tiles(0, 0, ROOM_W, ROOM_H, room_map);
}

static void enter_room(uint8_t *room, uint8_t next_room, uint8_t *robot_x, uint8_t next_x) {
    *room = next_room;
    *robot_x = next_x;
    draw_room(*room);
}

static uint8_t find_door(uint8_t room, uint8_t x) {
    uint8_t i;

    for (i = 0u; i < DOOR_COUNT; ++i) {
        if (
            doors[i].room == room &&
            x >= doors[i].x_min &&
            x <= doors[i].x_max
        ) {
            return i;
        }
    }

    return DOOR_INDEX_NONE;
}

static uint8_t is_north_locked_door(uint8_t door_index) {
    if (door_index >= DOOR_COUNT) {
        return 0u;
    }

    return (uint8_t)(
        doors[door_index].room == ROOM_NORTH &&
        doors[door_index].index == 1u
    );
}

static uint8_t door_effective_action(uint8_t door_index, const world_state_t *world) {
    if (door_index >= DOOR_COUNT) {
        return DOOR_ACTION_LOCKED;
    }

    if (
        is_north_locked_door(door_index) &&
        world->north_locked_door_unlocked
    ) {
        return DOOR_ACTION_OPEN;
    }

    return doors[door_index].action;
}

static void draw_status(uint8_t room, uint8_t prompt) {
    if (room == ROOM_EAST) {
        window_draw_row(FONT_TILE_BASE, 0, "BUILDING C EAST");
    } else if (room == ROOM_NORTH) {
        window_draw_row(FONT_TILE_BASE, 0, "BUILDING C NORTH");
    } else if (room == ROOM_NORTH_WEST) {
        window_draw_row(FONT_TILE_BASE, 0, "BUILDING C N/WEST");
    } else if (room == ROOM_NORTH_EAST) {
        window_draw_row(FONT_TILE_BASE, 0, "BUILDING C N/EAST");
    } else {
        window_draw_row(FONT_TILE_BASE, 0, "BUILDING C WEST");
    }

    if (prompt == STATUS_PROMPT_OPEN) {
        window_draw_row(FONT_TILE_BASE, 1, "+:MOVE a:OPEN");
    } else if (prompt == STATUS_PROMPT_LOCKED) {
        window_draw_row(FONT_TILE_BASE, 1, "* LOCKED!");
    } else {
        window_draw_row(FONT_TILE_BASE, 1, "+:MOVE a:LOOK");
    }
}

static uint8_t get_status_prompt(const world_state_t *world) {
    uint8_t door_index = find_door(world->room, world->robot_x);

    if (door_index != DOOR_INDEX_NONE) {
        if (door_effective_action(door_index, world) == DOOR_ACTION_OPEN) {
            return STATUS_PROMPT_OPEN;
        }

        return STATUS_PROMPT_LOCKED;
    }

    return STATUS_PROMPT_LOOK;
}

static void update_status_if_changed(
    const world_state_t *world,
    uint8_t *shown_room,
    uint8_t *shown_prompt
) {
    uint8_t prompt = get_status_prompt(world);

    if (*shown_room != world->room || *shown_prompt != prompt) {
        draw_status(world->room, prompt);
        *shown_room = world->room;
        *shown_prompt = prompt;
    }
}

static void show_locked_status(
    uint8_t room,
    uint8_t *shown_room,
    uint8_t *shown_prompt
) {
    draw_status(room, STATUS_PROMPT_LOCKED);
    *shown_room = room;
    *shown_prompt = STATUS_PROMPT_LOCKED;
}

static void show_pause_status(level_1_runtime_t *runtime) {
    move_win(7, STATUS_WIN_Y);
    window_draw_row(FONT_TILE_BASE, 0, "PAUSED");
    window_draw_row(FONT_TILE_BASE, 1, "START:RESUME");
    runtime->shown_room = 0xFFu;
    runtime->shown_prompt = STATUS_PROMPT_NONE;
}

static void hide_pause_status(
    const world_state_t *world,
    level_1_runtime_t *runtime
) {
    update_status_if_changed(
        world,
        &runtime->shown_room,
        &runtime->shown_prompt
    );
}

static void clear_fullscreen_window(void) {
    typewriter_clear_box(FONT_TILE_BASE, 0u, 0u, WIN_W, FULLSCREEN_WIN_H);
}

static void draw_inventory_empty(const char *line_2, const char *line_3) {
    clear_fullscreen_window();
    window_draw_row(FONT_TILE_BASE, 0, "INVENTORY");
    window_draw_row(FONT_TILE_BASE, 3, "EMPTY");
    window_draw_row(FONT_TILE_BASE, 6, line_2);
    window_draw_row(FONT_TILE_BASE, 16, line_3);
}

static void draw_inventory_item_row(
    uint8_t y,
    inventory_item_id_t item_id,
    uint8_t selected
) {
    uint8_t row[WIN_W];
    uint8_t i;
    uint8_t name_i = 0u;
    const inventory_item_t *item = inventory_get_item(item_id);
    const char *name = "";

    if (item != 0) {
        name = item->name_row;
    }

    for (i = 0u; i < WIN_W; ++i) {
        row[i] = tiny_font_tile_for_char(FONT_TILE_BASE, ' ');
    }

    row[0] = tiny_font_tile_for_char(FONT_TILE_BASE, selected ? '>' : ' ');

    for (i = 2u; i < WIN_W && name[name_i] != '\0'; ++i) {
        row[i] = tiny_font_tile_for_char(FONT_TILE_BASE, name[name_i]);
        ++name_i;
    }

    set_win_tiles(0u, y, WIN_W, 1u, row);
}

static void draw_inventory_slot_row(
    const inventory_t *inventory,
    uint8_t slot,
    uint8_t selected
) {
    inventory_item_id_t item_id = inventory_item_at(inventory, slot);

    if (item_id == INVENTORY_ITEM_INVALID) {
        return;
    }

    draw_inventory_item_row(
        (uint8_t)(INVENTORY_FIRST_ROW + slot),
        item_id,
        selected
    );
}

static void draw_inventory_menu(inventory_t *inventory) {
    uint8_t i;
    uint8_t item_count;

    inventory_clamp_selection(inventory);
    item_count = inventory_size(inventory);

    if (item_count == 0u) {
        draw_inventory_empty("NO ITEMS", "SELECT:CLOSE");
        return;
    }

    clear_fullscreen_window();
    window_draw_row(FONT_TILE_BASE, 0, "INVENTORY");
    for (i = 0u; i < item_count; ++i) {
        draw_inventory_slot_row(
            inventory,
            i,
            (uint8_t)(i == inventory->selected_slot)
        );
    }
    window_draw_row(FONT_TILE_BASE, 12, "a:INSPECT b:USE");
    window_draw_row(FONT_TILE_BASE, 14, "^v ITEM");
    window_draw_row(FONT_TILE_BASE, 16, "SELECT:CLOSE");
}

static void draw_inventory_selection_change(inventory_t *inventory, uint8_t old_slot) {
    uint8_t item_count;
    uint8_t new_slot;

    inventory_clamp_selection(inventory);
    item_count = inventory_size(inventory);

    if (item_count == 0u) {
        return;
    }

    new_slot = inventory->selected_slot;

    if (old_slot < item_count && old_slot != new_slot) {
        draw_inventory_slot_row(inventory, old_slot, 0u);
    }

    if (new_slot < item_count && old_slot != new_slot) {
        draw_inventory_slot_row(inventory, new_slot, 1u);
    }
}

static void draw_inventory_inspect(inventory_t *inventory) {
    inventory_item_id_t item_id;
    const inventory_item_t *item;

    inventory_clamp_selection(inventory);

    if (inventory_size(inventory) == 0u) {
        draw_inventory_empty("NOTHING TO INSPECT", "SELECT:CLOSE");
        return;
    }

    item_id = inventory_item_at(inventory, inventory->selected_slot);
    item = inventory_get_item(item_id);

    if (item == 0) {
        draw_inventory_empty("NOTHING TO INSPECT", "SELECT:CLOSE");
        return;
    }

    clear_fullscreen_window();
    window_draw_row(FONT_TILE_BASE, 0, "INVENTORY");
    window_draw_row(FONT_TILE_BASE, 3, item->name_row);
    window_draw_row(FONT_TILE_BASE, 6, item->inspect_line_0);
    window_draw_row(FONT_TILE_BASE, 7, item->inspect_line_1);
    window_draw_row(FONT_TILE_BASE, 15, "a/b:BACK");
    window_draw_row(FONT_TILE_BASE, 16, "SELECT:CLOSE");
}

static void draw_inventory_use(
    inventory_t *inventory,
    const item_use_result_t *result
) {
    inventory_item_id_t item_id;
    const inventory_item_t *item;

    inventory_clamp_selection(inventory);

    if (inventory_size(inventory) == 0u) {
        draw_inventory_empty("NOTHING TO USE", "SELECT:CLOSE");
        return;
    }

    item_id = inventory_item_at(inventory, inventory->selected_slot);
    item = inventory_get_item(item_id);

    if (item == 0) {
        draw_inventory_empty("NOTHING TO USE", "SELECT:CLOSE");
        return;
    }

    clear_fullscreen_window();
    window_draw_row(FONT_TILE_BASE, 0, "INVENTORY");
    window_draw_row(FONT_TILE_BASE, 3, item->name_row);
    window_draw_row(FONT_TILE_BASE, 6, result->line_0);
    window_draw_row(FONT_TILE_BASE, 7, result->line_1);
    window_draw_row(FONT_TILE_BASE, 15, "a/b:BACK");
    window_draw_row(FONT_TILE_BASE, 16, "SELECT:CLOSE");
}

static uint8_t world_near_north_locked_door(const world_state_t *world) {
    return (uint8_t)(
        world->room == world->north_room &&
        world->robot_x >= world->north_locked_door_x_min &&
        world->robot_x <= world->north_locked_door_x_max
    );
}

static item_use_result_t level_1_use_item(
    inventory_t *inventory,
    inventory_item_id_t item,
    world_state_t *world
) {
    item_use_result_t result;

    result.line_0 = "NOTHING TO USE";
    result.line_1 = "";
    result.consumed = 0u;
    result.world_changed = 0u;

    if (!inventory_has(inventory, item)) {
        return result;
    }

    if (item == ITEM_COIN) {
        result.line_0 = "IT CLINKS";
        result.line_1 = "";
    } else if (item == ITEM_ACCESS_CARD) {
        if (world_near_north_locked_door(world)) {
            if (world->north_locked_door_unlocked) {
                result.line_0 = "DOOR ALREADY";
                result.line_1 = "UNLOCKED";
            } else {
                world->north_locked_door_unlocked = 1u;
                result.line_0 = "ACCESS GRANTED";
                result.line_1 = "DOOR UNLOCKED";
                result.world_changed = 1u;
            }
        } else {
            result.line_0 = "CARD HAS NO READER";
            result.line_1 = "NEARBY";
        }
    } else if (item == ITEM_DRAINED_CELL) {
        result.line_0 = "NO POWER PORT";
        result.line_1 = "NEARBY";
    }

    if (result.consumed) {
        inventory_remove(inventory, item);
    }

    return result;
}

static void handle_inventory_input(
    uint8_t joy,
    uint8_t prev_joy,
    inventory_t *inventory,
    world_state_t *world
) {
    uint8_t old_slot;
    inventory_item_id_t item_id;
    item_use_result_t result;

    if (inventory->view == INVENTORY_VIEW_MENU) {
        if (
            ((joy & J_LEFT) && !(prev_joy & J_LEFT)) ||
            ((joy & J_UP) && !(prev_joy & J_UP))
        ) {
            old_slot = inventory->selected_slot;
            inventory_select_prev(inventory);
            draw_inventory_selection_change(inventory, old_slot);
        } else if (
            ((joy & J_RIGHT) && !(prev_joy & J_RIGHT)) ||
            ((joy & J_DOWN) && !(prev_joy & J_DOWN))
        ) {
            old_slot = inventory->selected_slot;
            inventory_select_next(inventory);
            draw_inventory_selection_change(inventory, old_slot);
        } else if ((joy & J_A) && !(prev_joy & J_A)) {
            inventory->view = INVENTORY_VIEW_INSPECT;
            draw_inventory_inspect(inventory);
        } else if ((joy & J_B) && !(prev_joy & J_B)) {
            inventory_clamp_selection(inventory);
            item_id = inventory_item_at(inventory, inventory->selected_slot);
            result = level_1_use_item(inventory, item_id, world);
            inventory->view = INVENTORY_VIEW_USE;
            draw_inventory_use(inventory, &result);
        }
    } else if (
        ((joy & J_A) && !(prev_joy & J_A)) ||
        ((joy & J_B) && !(prev_joy & J_B))
    ) {
        inventory->view = INVENTORY_VIEW_MENU;
        draw_inventory_menu(inventory);
    }
}

static void open_inventory(inventory_t *inventory) {
    HIDE_SPRITES;
    inventory->view = INVENTORY_VIEW_MENU;
    inventory_clamp_selection(inventory);
    move_win(7, INVENTORY_HIDDEN_WIN_Y);
    draw_inventory_menu(inventory);
    move_win(7, INVENTORY_WIN_Y);
}

static void close_inventory(
    const world_state_t *world,
    uint8_t *shown_room,
    uint8_t *shown_prompt
) {
    move_win(7, STATUS_WIN_Y);
    SHOW_SPRITES;
    *shown_room = 0xFFu;
    *shown_prompt = STATUS_PROMPT_NONE;
    update_status_if_changed(world, shown_room, shown_prompt);
}

static uint8_t inspect_north_east_crate(world_state_t *world, inventory_t *inventory) {
    if (
        world->robot_x < NORTH_EAST_CRATE_HOTSPOT_X_MIN ||
        world->robot_x > NORTH_EAST_CRATE_HOTSPOT_X_MAX
    ) {
        return 0u;
    }

    if (world->north_east_crate_looted) {
        window_draw_row(FONT_TILE_BASE, 0, "CRATE EMPTY");
        window_draw_row(FONT_TILE_BASE, 1, "CARD ALREADY TAKEN");
    } else if (inventory_add(inventory, ITEM_ACCESS_CARD)) {
        world->north_east_crate_looted = 1u;
        window_draw_row(FONT_TILE_BASE, 0, "ACCESS CARD");
        window_draw_row(FONT_TILE_BASE, 1, "ADDED TO INVENTORY");
    } else {
        world->north_east_crate_looted = 1u;
        window_draw_row(FONT_TILE_BASE, 0, "ACCESS CARD");
        window_draw_row(FONT_TILE_BASE, 1, "ALREADY CARRIED");
    }

    return 1u;
}

static void inspect_at(world_state_t *world, inventory_t *inventory) {
    uint8_t i;

    if (
        world->room == ROOM_NORTH_EAST &&
        inspect_north_east_crate(world, inventory)
    ) {
        return;
    }

    for (i = 0u; i < HOTSPOT_COUNT; ++i) {
        const hotspot_t *hotspot = &hotspots[i];

        if (
            hotspot->room == world->room &&
            world->robot_x >= hotspot->x_min &&
            world->robot_x <= hotspot->x_max
        ) {
            window_draw_row(FONT_TILE_BASE, 0, hotspot->line_0);
            window_draw_row(FONT_TILE_BASE, 1, hotspot->line_1);
            return;
        }
    }

    window_draw_row(FONT_TILE_BASE, 0, "BRICK. DUST.");
    window_draw_row(FONT_TILE_BASE, 1, "BUILDING C BREATHES");
}

static void level_1_init_world(world_state_t *world) {
    world->room = ROOM_WEST;
    world->robot_x = 80u;
    world->north_east_crate_looted = 0u;
    world->north_locked_door_unlocked = 0u;
    world->north_room = ROOM_NORTH;
    world->north_locked_door_x_min = NORTH_LOCKED_DOOR_HOTSPOT_X_MIN;
    world->north_locked_door_x_max = NORTH_LOCKED_DOOR_HOTSPOT_X_MAX;
}

static void level_1_init_runtime(level_1_runtime_t *runtime) {
    runtime->robot_facing = ROBOT_FACE_RIGHT;
    runtime->robot_frame = 0u;
    runtime->anim_timer = 0u;
    runtime->previous_joy = 0u;
    runtime->shown_room = 0xFFu;
    runtime->shown_prompt = STATUS_PROMPT_NONE;
    runtime->inventory_open = 0u;
    runtime->paused = 0u;
}

static void level_1_load_assets(void) {
    set_bkg_data(BRICK_TILE_BASE, brickwall_TILE_COUNT, brickwall_tiles);
    set_bkg_data(FLOOR_TILE_BASE, floor_TILE_COUNT, floor_tiles);
    set_bkg_data(CRATE_TILE_BASE, crate_TILE_COUNT, crate_tiles);
    set_bkg_data(DOOR_TILE_BASE, door_TILE_COUNT, door_tiles);
    set_bkg_data(EXIT_SIGN_TILE_BASE, exit_sign_TILE_COUNT, exit_sign_tiles);

    SPRITES_8x16;
    set_sprite_data(ROBOT_TILE_BASE, robot_TILE_COUNT, robot_tiles);
}

static void level_1_draw_robot(
    const world_state_t *world,
    const level_1_runtime_t *runtime
) {
    if (runtime->robot_facing == ROBOT_FACE_LEFT) {
        move_metasprite_flipx(
            robot_metasprites[runtime->robot_frame],
            ROBOT_TILE_BASE,
            0,
            0,
            world->robot_x,
            ROBOT_Y
        );
    } else {
        move_metasprite(
            robot_metasprites[runtime->robot_frame],
            ROBOT_TILE_BASE,
            0,
            world->robot_x,
            ROBOT_Y
        );
    }
}

static void level_1_draw_initial_state(
    const world_state_t *world,
    level_1_runtime_t *runtime
) {
    draw_room(world->room);

    move_win(7, STATUS_WIN_Y);
    window_clear(FONT_TILE_BASE);
    update_status_if_changed(
        world,
        &runtime->shown_room,
        &runtime->shown_prompt
    );

    level_1_draw_robot(world, runtime);

    SHOW_BKG;
    SHOW_WIN;
    SHOW_SPRITES;
}

static uint8_t level_1_update(
    world_state_t *world,
    inventory_t *inventory,
    level_1_runtime_t *runtime
) {
    uint8_t joy;
    uint8_t moving;
    uint8_t door_index;
    uint8_t inventory_toggled;

    joy = joypad();
    moving = 0u;
    inventory_toggled = 0u;

    if (
        !runtime->inventory_open &&
        (joy & J_START) &&
        !(runtime->previous_joy & J_START)
    ) {
        if (runtime->paused) {
            runtime->paused = 0u;
            hide_pause_status(world, runtime);
        } else {
            runtime->paused = 1u;
            runtime->anim_timer = 0u;
            runtime->robot_frame = 0u;
            show_pause_status(runtime);
            level_1_draw_robot(world, runtime);
        }

        runtime->previous_joy = joy;
        return 0u;
    }

    if (runtime->paused) {
        runtime->previous_joy = joy;
        return 0u;
    }

    if ((joy & J_SELECT) && !(runtime->previous_joy & J_SELECT)) {
        inventory_toggled = 1u;

        if (runtime->inventory_open) {
            runtime->inventory_open = 0u;
            close_inventory(
                world,
                &runtime->shown_room,
                &runtime->shown_prompt
            );
        } else {
            runtime->inventory_open = 1u;
            runtime->anim_timer = 0u;
            runtime->robot_frame = 0u;
            open_inventory(inventory);
        }
    }

    if (runtime->inventory_open) {
        if (!inventory_toggled) {
            handle_inventory_input(
                joy,
                runtime->previous_joy,
                inventory,
                world
            );
        }
    } else if (!inventory_toggled && (joy & J_LEFT)) {
        if (world->robot_x > ROBOT_MIN_X) {
            world->robot_x--;
            moving = 1u;
            runtime->robot_facing = ROBOT_FACE_LEFT;
        } else if (world->room == ROOM_EAST) {
            enter_room(&world->room, ROOM_WEST, &world->robot_x, ROBOT_MAX_X);
            moving = 1u;
            runtime->robot_facing = ROBOT_FACE_LEFT;
        } else if (world->room == ROOM_NORTH) {
            enter_room(&world->room, ROOM_NORTH_WEST, &world->robot_x, ROBOT_MAX_X);
            moving = 1u;
            runtime->robot_facing = ROBOT_FACE_LEFT;
        } else if (world->room == ROOM_NORTH_EAST) {
            enter_room(&world->room, ROOM_NORTH, &world->robot_x, ROBOT_MAX_X);
            moving = 1u;
            runtime->robot_facing = ROBOT_FACE_LEFT;
        }

        if (moving) {
            update_status_if_changed(
                world,
                &runtime->shown_room,
                &runtime->shown_prompt
            );
        }
    }

    if (!runtime->inventory_open && !inventory_toggled && (joy & J_RIGHT)) {
        if (world->robot_x < ROBOT_MAX_X) {
            world->robot_x++;
            moving = 1u;
            runtime->robot_facing = ROBOT_FACE_RIGHT;
        } else if (world->room == ROOM_WEST) {
            enter_room(&world->room, ROOM_EAST, &world->robot_x, ROBOT_MIN_X);
            moving = 1u;
            runtime->robot_facing = ROBOT_FACE_RIGHT;
        } else if (world->room == ROOM_NORTH) {
            enter_room(&world->room, ROOM_NORTH_EAST, &world->robot_x, ROBOT_MIN_X);
            moving = 1u;
            runtime->robot_facing = ROBOT_FACE_RIGHT;
        } else if (world->room == ROOM_NORTH_WEST) {
            enter_room(&world->room, ROOM_NORTH, &world->robot_x, ROBOT_MIN_X);
            moving = 1u;
            runtime->robot_facing = ROBOT_FACE_RIGHT;
        }

        if (moving) {
            update_status_if_changed(
                world,
                &runtime->shown_room,
                &runtime->shown_prompt
            );
        }
    }

    if (
        !runtime->inventory_open &&
        !inventory_toggled &&
        (joy & J_A) &&
        !(runtime->previous_joy & J_A)
    ) {
        door_index = find_door(world->room, world->robot_x);

        if (
            door_index != DOOR_INDEX_NONE &&
            door_effective_action(door_index, world) == DOOR_ACTION_OPEN
        ) {
            world->room = doors[door_index].dest_room;
            world->robot_x = doors[door_index].dest_x;
            draw_room(world->room);
            update_status_if_changed(
                world,
                &runtime->shown_room,
                &runtime->shown_prompt
            );
        } else if (door_index != DOOR_INDEX_NONE) {
            show_locked_status(
                world->room,
                &runtime->shown_room,
                &runtime->shown_prompt
            );
        } else {
            inspect_at(world, inventory);
            runtime->shown_prompt = STATUS_PROMPT_NONE;
        }
    }

    if (moving) {
        runtime->anim_timer++;

        if (runtime->anim_timer >= 10u) {
            runtime->anim_timer = 0u;
            runtime->robot_frame ^= 1u;   /* toggles 0 <-> 1 */

            if (runtime->robot_frame == 1u) {
                sfx_robot_step();
            }
        }
    } else {
        runtime->anim_timer = 0u;
        runtime->robot_frame = 0u;
    }

    level_1_draw_robot(world, runtime);
    runtime->previous_joy = joy;
    return 0u;
}

level_result_t level_1_run(void) {
    world_state_t world;
    inventory_t inventory;
    level_1_runtime_t runtime;

    level_1_init_world(&world);
    inventory_init(&inventory);
    level_1_init_runtime(&runtime);

    DISPLAY_OFF;
    level_1_load_assets();
    level_1_draw_initial_state(&world, &runtime);
    DISPLAY_ON;

    music_play_building();

    while (1) {
        wait_vbl_done();

        if (level_1_update(&world, &inventory, &runtime)) {
            return LEVEL_RESULT_COMPLETE;
        }
    }
}

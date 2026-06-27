#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdint.h>

typedef enum inventory_item_id_t {
    ITEM_COIN = 0u,
    ITEM_ACCESS_CARD,
    ITEM_DRAINED_CELL,
    ITEM_COUNT
} inventory_item_id_t;

#define INVENTORY_ITEM_INVALID ITEM_COUNT

typedef enum inventory_view_t {
    INVENTORY_VIEW_MENU = 0u,
    INVENTORY_VIEW_INSPECT,
    INVENTORY_VIEW_USE
} inventory_view_t;

typedef struct inventory_item_t {
    const char *name_row;
    const char *inspect_line_0;
    const char *inspect_line_1;
} inventory_item_t;

typedef struct inventory_t {
    uint8_t owned;
    uint8_t selected_slot;
    uint8_t view;
} inventory_t;

typedef struct world_state_t {
    uint8_t room;
    uint8_t robot_x;
    uint8_t north_east_crate_looted;
    uint8_t north_locked_door_unlocked;
    uint8_t north_room;
    uint8_t north_locked_door_x_min;
    uint8_t north_locked_door_x_max;
} world_state_t;

typedef struct item_use_result_t {
    const char *line_0;
    const char *line_1;
    uint8_t consumed;
    uint8_t world_changed;
} item_use_result_t;

void inventory_init(inventory_t *inventory);
uint8_t inventory_has(const inventory_t *inventory, inventory_item_id_t item);
uint8_t inventory_add(inventory_t *inventory, inventory_item_id_t item);
uint8_t inventory_remove(inventory_t *inventory, inventory_item_id_t item);
uint8_t inventory_size(const inventory_t *inventory);
inventory_item_id_t inventory_item_at(const inventory_t *inventory, uint8_t visible_slot);
void inventory_clamp_selection(inventory_t *inventory);
void inventory_select_prev(inventory_t *inventory);
void inventory_select_next(inventory_t *inventory);
const inventory_item_t *inventory_get_item(inventory_item_id_t item);
item_use_result_t inventory_use_item(
    inventory_t *inventory,
    inventory_item_id_t item,
    world_state_t *world
);

#endif

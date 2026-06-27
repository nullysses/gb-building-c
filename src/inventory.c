#include <stdint.h>

#include "inventory.h"

#define ITEM_MASK(item) ((uint8_t)(1u << (uint8_t)(item)))

static const inventory_item_t inventory_items[ITEM_COUNT] = {
    {
        "COIN",
        "(5 MXN)",
        ""
    },
    {
        "ACCESS CARD",
        "OLD ACCESS CARD",
        "ID: MAINT 03"
    },
    {
        "DRAINED CELL",
        "DRAINED POWER CELL",
        "IT IS COLD"
    }
};

static uint8_t inventory_item_valid(inventory_item_id_t item) {
    return (uint8_t)(item < ITEM_COUNT);
}

void inventory_init(inventory_t *inventory) {
    inventory->owned = ITEM_MASK(ITEM_COIN);
    inventory->selected_slot = 0u;
    inventory->view = INVENTORY_VIEW_MENU;
}

uint8_t inventory_has(const inventory_t *inventory, inventory_item_id_t item) {
    if (!inventory_item_valid(item)) {
        return 0u;
    }

    return (uint8_t)((inventory->owned & ITEM_MASK(item)) != 0u);
}

uint8_t inventory_add(inventory_t *inventory, inventory_item_id_t item) {
    if (!inventory_item_valid(item)) {
        return 0u;
    }

    if (inventory_has(inventory, item)) {
        return 0u;
    }

    inventory->owned |= ITEM_MASK(item);
    inventory_clamp_selection(inventory);
    return 1u;
}

uint8_t inventory_remove(inventory_t *inventory, inventory_item_id_t item) {
    if (!inventory_item_valid(item)) {
        return 0u;
    }

    if (!inventory_has(inventory, item)) {
        return 0u;
    }

    inventory->owned &= (uint8_t)~ITEM_MASK(item);
    inventory_clamp_selection(inventory);
    return 1u;
}

uint8_t inventory_size(const inventory_t *inventory) {
    uint8_t item;
    uint8_t count = 0u;

    for (item = 0u; item < ITEM_COUNT; ++item) {
        if (inventory_has(inventory, (inventory_item_id_t)item)) {
            ++count;
        }
    }

    return count;
}

inventory_item_id_t inventory_item_at(const inventory_t *inventory, uint8_t visible_slot) {
    uint8_t item;
    uint8_t slot = 0u;

    for (item = 0u; item < ITEM_COUNT; ++item) {
        if (inventory_has(inventory, (inventory_item_id_t)item)) {
            if (slot == visible_slot) {
                return (inventory_item_id_t)item;
            }

            ++slot;
        }
    }

    return INVENTORY_ITEM_INVALID;
}

void inventory_clamp_selection(inventory_t *inventory) {
    uint8_t size = inventory_size(inventory);

    if (size == 0u) {
        inventory->selected_slot = 0u;
    } else if (inventory->selected_slot >= size) {
        inventory->selected_slot = (uint8_t)(size - 1u);
    }
}

void inventory_select_prev(inventory_t *inventory) {
    uint8_t size = inventory_size(inventory);

    if (size == 0u) {
        inventory->selected_slot = 0u;
        return;
    }

    inventory_clamp_selection(inventory);

    if (inventory->selected_slot == 0u) {
        inventory->selected_slot = (uint8_t)(size - 1u);
    } else {
        --inventory->selected_slot;
    }
}

void inventory_select_next(inventory_t *inventory) {
    uint8_t size = inventory_size(inventory);

    if (size == 0u) {
        inventory->selected_slot = 0u;
        return;
    }

    inventory_clamp_selection(inventory);
    ++inventory->selected_slot;

    if (inventory->selected_slot >= size) {
        inventory->selected_slot = 0u;
    }
}

const inventory_item_t *inventory_get_item(inventory_item_id_t item) {
    if (!inventory_item_valid(item)) {
        return 0;
    }

    return &inventory_items[(uint8_t)item];
}

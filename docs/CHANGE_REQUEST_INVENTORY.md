# Change Request: Implement a Robust Inventory System

## Context

The current inventory implementation uses `inventory_count` to represent ownership of the first _N_ entries in `inventory_items`.

This works only while items are acquired in a fixed order:

1. Coin
2. Access card
3. Drained cell

It will fail when items can be acquired out of order, consumed, removed, or conditionally used. The current “use” action also displays static text rather than interacting with the room or world state.

This change should replace the prototype inventory with an explicit, extensible inventory and item-use model suitable for future gameplay.

---

## Objectives

1. Represent ownership independently for every inventory item.
2. Allow items to be acquired in any order.
3. Prevent duplicate acquisition unless an item explicitly supports quantities.
4. Separate inventory contents from world-state flags such as whether a crate has been looted.
5. Make item use aware of the current room and player position.
6. Preserve the existing inventory UI behavior:
   - `SELECT` opens and closes the inventory.
   - Directional input changes the selected item.
   - `A` inspects the selected item.
   - `B` uses the selected item.
7. Avoid out-of-bounds reads when the inventory is empty or when selection becomes invalid.
8. Keep the design appropriate for Game Boy memory and CPU constraints.

---

## Required Design

### Item identifiers

Define stable item identifiers rather than relying on array position plus `inventory_count`.

Example:

```c
typedef enum inventory_item_id_t {
    ITEM_COIN = 0u,
    ITEM_ACCESS_CARD,
    ITEM_DRAINED_CELL,
    ITEM_COUNT
} inventory_item_id_t;
```

`inventory_items` may remain an indexed metadata table, but ownership must not mean “all entries below this index are owned.”

---

### Ownership state

Use an explicit ownership representation.

A bitset is preferred while the game has fewer than eight inventory items:

```c
typedef struct inventory_t {
    uint8_t owned;
    uint8_t selected_slot;
    uint8_t view;
} inventory_t;
```

Provide operations rather than modifying the bitset directly throughout the game:

```c
uint8_t inventory_has(const inventory_t *inventory, inventory_item_id_t item);
uint8_t inventory_add(inventory_t *inventory, inventory_item_id_t item);
uint8_t inventory_remove(inventory_t *inventory, inventory_item_id_t item);
uint8_t inventory_size(const inventory_t *inventory);
```

Expected behavior:

- `inventory_add()` returns nonzero only when ownership changes.
- Adding an already-owned item does not create a duplicate.
- Removing an absent item is harmless.
- Invalid item identifiers are rejected.

Do not expose an `inventory_count` variable as the source of truth.

---

### Visible inventory slots

The menu must enumerate only owned items.

A visible slot is not necessarily the same as an item identifier. For example, visible slot `0` may refer to `ITEM_ACCESS_CARD` when the card is the only owned item.

Provide a lookup such as:

```c
inventory_item_id_t inventory_item_at(
    const inventory_t *inventory,
    uint8_t visible_slot
);
```

The implementation must:

- Return only owned items.
- Use a deterministic order.
- Clamp or reset `selected_slot` when an item is removed.
- Never index `inventory_items` using an unchecked visible slot.

---

### Inventory views

Represent the current inventory page explicitly:

```c
typedef enum inventory_view_t {
    INVENTORY_VIEW_MENU = 0u,
    INVENTORY_VIEW_INSPECT,
    INVENTORY_VIEW_USE
} inventory_view_t;
```

Expected controls:

| View | Input | Result |
|---|---|---|
| Any inventory view | `SELECT` | Close inventory |
| Menu | Up/Left | Previous item |
| Menu | Down/Right | Next item |
| Menu | `A` | Inspect selected item |
| Menu | `B` | Use selected item |
| Inspect | `A` or `B` | Return to menu |
| Use result | `A` or `B` | Return to menu |

Directional input on an inspect or use-result page must not silently change the selected item.

---

## Contextual Item Use

Replace static `use_line_0` and `use_line_1` behavior with a use operation that receives world context.

Suggested interface:

```c
typedef struct world_state_t {
    uint8_t room;
    uint8_t robot_x;
    uint8_t north_east_crate_looted;
    uint8_t north_locked_door_unlocked;
} world_state_t;

typedef struct item_use_result_t {
    const char *line_0;
    const char *line_1;
    uint8_t consumed;
    uint8_t world_changed;
} item_use_result_t;

item_use_result_t inventory_use_item(
    inventory_t *inventory,
    inventory_item_id_t item,
    world_state_t *world
);
```

The exact structures may differ, but item use must have enough context to affect gameplay.

### Access card behavior

The access card must interact with the locked door in `ROOM_NORTH`.

Required behavior:

- When used near the locked door:
  - Unlock the door.
  - Preserve the access card unless the design explicitly requires consumption.
  - Display a success message.
- When used elsewhere:
  - Display a context-appropriate failure message.
  - Do not modify world state.
- Once unlocked:
  - The door must remain unlocked after changing rooms.
  - The room drawing and door interaction logic must reflect the new state.

The locked door must not be represented only by immutable `DOOR_ACTION_LOCKED` data. Its effective action must be derived from mutable world state.

---

## World Item Acquisition

The north-east crate must have its own world-state flag:

```c
uint8_t north_east_crate_looted;
```

Required behavior:

- Inspecting the crate before it is looted adds the access card.
- Inspecting it again reports that it is empty.
- Removing or consuming the access card later must not repopulate the crate.
- Crate state and inventory ownership must remain independent.

Do not infer whether the crate was looted from whether the player currently owns the card.

---

## Rendering Requirements

1. Draw only owned inventory items.
2. Support an empty inventory without invalid indexing.
3. Clamp selection before rendering.
4. Preserve selection when returning from inspect/use views where possible.
5. Do not clear and redraw the entire 20×18 Window tilemap for every selection change.
6. When moving between inventory items, redraw only:
   - the previously selected row;
   - the newly selected row.
7. Prepare the inventory window while it is hidden or offscreen before moving it onscreen, to reduce visible tilemap updates.
8. Restore the status window and sprites correctly when closing inventory.

---

## Suggested Separation

Move inventory logic out of `main.c`.

Suggested files:

```text
src/inventory.h
src/inventory.c
```

The module should own:

- Item identifiers
- Item metadata
- Ownership operations
- Visible-slot lookup
- Selection normalization
- Inventory view state

World-specific effects may remain in `main.c` or move into a later gameplay/world module. The inventory module should not need to know how rooms are rendered.

---

## Acceptance Criteria

The change is complete when all of the following are true:

- [ ] The game starts with the coin and no other item.
- [ ] Inventory ownership is not represented by a prefix count.
- [ ] The access card can be added independently of the coin and drained cell.
- [ ] Adding an already-owned item does not duplicate it.
- [ ] The inventory can be empty without crashing or reading outside `inventory_items`.
- [ ] Selection wraps correctly through owned items only.
- [ ] Inspect and use-result pages have explicit navigation behavior.
- [ ] Using the access card near the north locked door unlocks it.
- [ ] Using the access card elsewhere does not unlock the door.
- [ ] The unlocked door remains unlocked after room transitions.
- [ ] The north-east crate cannot respawn the card after being looted.
- [ ] Removing an item does not corrupt selection.
- [ ] Opening or closing inventory does not trigger movement, inspection, or door actions in the same frame.
- [ ] The project builds successfully with the existing GBDK-2020 toolchain.
- [ ] `dist/building_c.gb` is regenerated from the completed source.

---

## Regression Checks

Manually verify the following sequences:

### Normal acquisition

1. Start the game.
2. Open inventory.
3. Confirm that only the coin is present.
4. Travel to the north-east room.
5. Inspect the crate.
6. Confirm that the access card is added exactly once.
7. Leave and return.
8. Confirm that the crate remains empty.

### Contextual use

1. Use the access card away from the locked door.
2. Confirm that nothing unlocks.
3. Stand near the locked door.
4. Use the access card.
5. Confirm that the door unlocks.
6. Leave the room and return.
7. Confirm that the door remains unlocked and usable.

### Selection safety

1. Open an empty inventory in a debug build or temporary test setup.
2. Press every directional and action button.
3. Confirm that no invalid item is rendered or accessed.
4. Add one item and select it.
5. Remove it.
6. Confirm that selection resets safely.

### Input isolation

1. Hold a direction and press `SELECT`.
2. Confirm that opening the inventory does not move the robot.
3. Close the inventory while holding `A`, `B`, or a direction.
4. Confirm that no world action occurs on the closing frame.

---

## Non-Goals

The following are outside this change unless required by the implementation:

- Stackable item quantities
- Inventory sorting selected by the player
- Save-game persistence
- Animated inventory transitions
- Equipment slots
- More than eight simultaneous item types
- Generalized scripting for item effects

The design should not prevent these features, but they do not need to be implemented now.

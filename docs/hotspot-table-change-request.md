# Change Request: Replace Hardcoded Inspect Branches with Hotspot Table

## Goal

Refactor the current `inspect_at(room, x)` logic in `src/main.c` so inspectable objects are defined as data instead of hardcoded `if` / `else if` branches.

This should make rooms easier to author as the project grows.

Current behavior should remain equivalent, except adding the EAST room door hotspot.

---

## Current Problem

`inspect_at()` currently encodes object positions and text directly in conditional logic:

```c
static void inspect_at(uint8_t room, uint8_t x) {
    if (room == ROOM_WEST && x >= 12u && x <= 40u) {
        window_draw_row(FONT_TILE_BASE, 0, "SEALED CRATE");
        window_draw_row(FONT_TILE_BASE, 1, "???");
    } else if (room == ROOM_WEST && x >= 120u && x <= 152u) {
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
```

This is acceptable for a first prototype, but it will become brittle as more rooms and props are added.

---

## Required Change

Introduce a small hotspot data structure.

Suggested shape:

```c
typedef struct hotspot_t {
    uint8_t room;
    uint8_t x_min;
    uint8_t x_max;
    const char *line_0;
    const char *line_1;
} hotspot_t;
```

Add a static table:

```c
static const hotspot_t hotspots[] = {
    { ROOM_WEST, 12u, 40u,   "SEALED CRATE",  "???" },
    { ROOM_WEST, 70u, 96u,   "WALL SENSOR",   "IT WATCHES YOU" },
    { ROOM_WEST, 120u, 152u, "SUPPLY CRATE",  "EMPTY. TOO CLEAN" },

    { ROOM_EAST, 70u, 96u,   "WALL SENSOR",   "IT WATCHES YOU" },
    { ROOM_EAST, 128u, 160u, "DOOR LOCKED",   "NO PROMPT FOUND" }
};
```

Add:

```c
#define HOTSPOT_COUNT ((uint8_t)(sizeof(hotspots) / sizeof(hotspots[0])))
```

Then rewrite `inspect_at()` to scan the table:

```c
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
```

---

## Preserve Existing Behavior

The following should still work:

```text
WEST room:
- left crate inspection
- right crate inspection
- wall sensor inspection
- default brick/dust inspection

EAST room:
- wall sensor inspection
- default brick/dust inspection
```

Add this new behavior:

```text
EAST room:
- door inspection near the door
```

Suggested text:

```text
DOOR LOCKED
NO PROMPT FOUND
```

---

## Constraints

Keep the implementation simple.

Do not add:

```text
- dynamic allocation
- function pointers
- generic event systems
- room object systems
- collision integration
- Y-axis checks yet
```

For now this is a **side-view 1D inspect table** keyed by:

```text
room + robot_x range
```

This is deliberate. The project does not yet need a full interaction engine.

---

## Notes

The `x_min` / `x_max` values are in sprite/world pixel coordinates, matching the current `robot_x` inspection logic.

Use `uint8_t` for all coordinates and counts.

Keep fallback text in code for now.

---

## Acceptance Criteria

After the change:

1. The project builds with `make`.
2. Existing WEST room crate inspections still work.
3. Existing wall sensor inspection still works.
4. EAST room door inspection displays:

   ```text
   DOOR LOCKED
   NO PROMPT FOUND
   ```

5. Inspecting empty wall/floor areas still displays:

   ```text
   BRICK. DUST.
   BUILDING C BREATHES
   ```

6. `inspect_at()` no longer contains a chain of room/object-specific `if` / `else if` branches.
7. The new hotspot table is easy to extend with more objects later.

---

## Optional Follow-up

Later, this can evolve into:

```c
typedef struct hotspot_t {
    uint8_t room;
    uint8_t x_min;
    uint8_t x_max;
    uint8_t flags;
    const char *line_0;
    const char *line_1;
} hotspot_t;
```

Possible future flags:

```text
HOTSPOT_REPEATABLE
HOTSPOT_LOCKED
HOTSPOT_REQUIRES_STATE
HOTSPOT_ADVANCES_STORY
```

Do not implement this yet.

# Change Request: Extract Current Gameplay into `level_1.c`

## Context

The current `main.c` contains both application-level initialization and all level-specific gameplay logic.

It currently owns:

* Hardware and display initialization
* Opening sequence
* Shared font, audio, and sprite setup
* Room definitions and rendering
* Door and hotspot definitions
* World state
* Inventory state
* Robot movement and animation
* Level input handling
* Level-specific item interactions
* The permanent gameplay loop

This makes `main.c` responsible for both starting the program and implementing the current level. Before adding more gameplay, the existing level should be extracted into its own module.

The goal is to establish a clear scene/level boundary without redesigning the room system or introducing a general-purpose engine.

---

## Objectives

1. Move all current level-specific gameplay into `src/level_1.c`.
2. Keep `main.c` responsible only for application-level startup and sequencing.
3. Allow the current level to be restarted with fresh state.
4. Give the level an explicit completion/result boundary.
5. Keep reusable systems outside the level module.
6. Preserve existing gameplay behavior during the extraction.
7. Prepare the codebase for a future `level_2.c` or other scenes.

---

## Required Files

Add:

```text
src/level_1.c
src/level_1.h
```

Update the `Makefile` so `src/level_1.c` is included in `C_SOURCES`.

---

## Public Level Interface

`level_1.h` should expose only the level entry point and its result type.

```c
#ifndef LEVEL_1_H
#define LEVEL_1_H

typedef enum level_result_t {
    LEVEL_RESULT_COMPLETE = 0u,
    LEVEL_RESULT_RESTART
} level_result_t;

level_result_t level_1_run(void);

#endif
```

The exact names may differ, but:

* The level entry point must be callable from `main.c`.
* The level must be able to return.
* The caller must distinguish normal completion from restart.
* Level-internal functions and data must not be exposed through the header.

---

## `main.c` Responsibilities

After the change, `main.c` should own only application-level behavior.

Expected responsibilities:

* Hardware initialization
* LCD and palette configuration
* Shared font loading
* Shared audio/SFX initialization
* Opening or title sequence
* Waiting for opening input release
* Starting the current level
* Handling the result returned by the level
* Future level progression

Suggested structure:

```c
#include <gb/gb.h>

#include "level_1.h"

static void game_init(void);
static void run_opening(void);
static void wait_for_input_release(void);

void main(void) {
    level_result_t result;

    game_init();
    run_opening();
    wait_for_input_release();

    while (1) {
        result = level_1_run();

        if (result == LEVEL_RESULT_COMPLETE) {
            /* Future: run the next level or ending scene. */
        } else if (result == LEVEL_RESULT_RESTART) {
            /* Re-enter level_1_run() with fresh local state. */
        }
    }
}
```

Do not leave room logic, level input handling, or level-specific state in `main.c`.

---

## `level_1.c` Responsibilities

Move the current gameplay-specific implementation into `level_1.c`.

This includes:

* Room identifiers
* Room dimensions and room tilemap
* Door definitions
* Hotspot definitions
* Level-specific geometry constants
* Room construction and drawing
* Door lookup and effective-action logic
* Level-specific world-state initialization
* Inventory instance used by the level
* Robot movement
* Robot facing and animation
* Current room and position
* Status-window logic
* Inventory-window integration
* North-east crate interaction
* Access-card and locked-door behavior
* The per-frame gameplay loop
* Current level music selection
* Level-specific asset loading

All implementation details should be `static` unless they form part of the public level interface.

---

## Runtime State

Replace the current group of loose gameplay variables with a level runtime structure.

```c
typedef struct level_1_runtime_t {
    uint8_t robot_facing;
    uint8_t robot_frame;
    uint8_t anim_timer;
    uint8_t previous_joy;
    uint8_t shown_room;
    uint8_t shown_prompt;
    uint8_t inventory_open;
} level_1_runtime_t;
```

The exact fields may differ, but transient per-run state should be grouped rather than scattered across `level_1_run()`.

The level should initialize its own:

```c
world_state_t world;
inventory_t inventory;
level_1_runtime_t runtime;
```

These should normally be local to `level_1_run()` so calling the function again creates fresh level state.

Do not retain mutable level state in file-scope globals unless required by GBDK constraints.

---

## Level Initialization

Add private initialization functions such as:

```c
static void level_1_init_world(world_state_t *world);
static void level_1_init_runtime(level_1_runtime_t *runtime);
static void level_1_load_assets(void);

static void level_1_draw_initial_state(
    const world_state_t *world,
    const level_1_runtime_t *runtime
);
```

Initialization must reset:

* Starting room
* Robot position
* Robot facing
* Animation frame and timer
* Door unlock state
* Crate-looted state
* Inventory contents
* Inventory selection and view
* Status-window cache
* Input history
* Inventory-open state

Calling `level_1_run()` after `LEVEL_RESULT_RESTART` must not preserve state from the previous run.

---

## Display and VRAM Handling

The level must explicitly control display state while loading or replacing VRAM data.

Recommended sequence:

```c
DISPLAY_OFF;

level_1_load_assets();
level_1_draw_initial_state(...);

DISPLAY_ON;
```

This establishes that each level may reuse the same tile ranges.

Do not assume that level assets remain permanently resident after `level_1_run()` returns.

The implementation must preserve the existing separation between:

* Background tilemap
* Window tilemap
* Sprite tile data

---

## Shared Systems That Must Remain Separate

Do not move reusable systems into `level_1.c`.

The following should remain independent modules:

```text
src/inventory.c
src/typewriter.c
src/window_text.c
src/tiny_font.c
src/sfx.c
src/audio/music.c
```

Generic asset-loading helpers may also remain shared if they are not specific to this level.

---

## Inventory and World Interaction Boundary

The inventory container should continue to own:

* Item identifiers
* Ownership
* Visible-slot mapping
* Selection
* Inventory view state
* Item metadata

Level-specific effects should be owned by `level_1.c`.

In particular, the effect of using the access card on the north locked door is level-specific. The preferred direction is:

```c
static item_use_result_t level_1_use_item(
    inventory_t *inventory,
    inventory_item_id_t item,
    world_state_t *world
);
```

or an equivalent level callback.

The extraction does not strictly require moving this behavior out of `inventory.c` if doing so would expand the change too much, but `level_1.c` should become the eventual owner of level geometry and level-specific item consequences.

---

## Level Loop

The level loop should remain inside `level_1_run()`.

```c
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
```

The exact update structure may differ.

The permanent application loop must remain in `main.c`. `level_1_run()` may loop until completion or restart, but it must have an explicit return path.

---

## Restart Behavior

The existing `START` behavior currently resets only part of the level.

Change it to return:

```c
return LEVEL_RESULT_RESTART;
```

Then let `main.c` call `level_1_run()` again.

Restart must reset:

* Inventory
* Looted crate state
* Door unlock state
* Robot state
* UI state
* Cached prompts
* Input state

A partial reset that only changes room and robot position is not sufficient.

---

## Behavior Preservation

The extraction must not intentionally change:

* Opening text
* Initial inventory
* Room layout
* Room transitions
* Door interactions
* Locked-door behavior
* Crate interaction
* Inventory controls
* Robot movement speed
* Robot animation timing
* Sound effects
* Music start point
* Status-window text

This should be a structural change first.

Do not simultaneously redesign:

* The room graph
* Door representation
* Inventory architecture
* Collision
* Asset formats
* Music system
* Scene scripting

---

## Acceptance Criteria

* [ ] `src/level_1.c` exists.
* [ ] `src/level_1.h` exists.
* [ ] `src/level_1.c` is included in the `Makefile`.
* [ ] `main.c` no longer contains room, door, hotspot, movement, inventory UI, or level interaction logic.
* [ ] `main.c` initializes the application and calls `level_1_run()`.
* [ ] `level_1_run()` initializes fresh world, inventory, and runtime state.
* [ ] The level has an explicit return type.
* [ ] Restarting the level creates fresh state.
* [ ] All level-internal functions and tables are `static`.
* [ ] Shared systems remain in their existing modules.
* [ ] The level explicitly loads its own assets.
* [ ] Display and VRAM transition handling is explicit.
* [ ] Existing gameplay behaves the same as before the extraction.
* [ ] The project builds successfully with the existing GBDK-2020 toolchain.
* [ ] `dist/building_c.gb` is regenerated from the completed source.

---

## Regression Checks

### Startup

1. Start the ROM.
2. Confirm that the opening sequence behaves exactly as before.
3. Confirm that music begins at the same point.
4. Confirm that the player begins in the same room and position.

### Navigation

1. Traverse every currently reachable room.
2. Confirm all left/right room transitions.
3. Confirm all door interactions.
4. Confirm the status prompt changes correctly near doors.

### Inventory

1. Open and close the inventory.
2. Inspect the coin.
3. Collect the access card.
4. Confirm the crate remains looted.
5. Use the access card near and away from the locked door.
6. Confirm the door unlock persists during the current run.

### Restart

1. Collect the access card.
2. Unlock the door.
3. Trigger level restart.
4. Confirm the player returns to the initial room.
5. Confirm the inventory contains only the initial coin.
6. Confirm the crate is no longer marked as looted.
7. Confirm the door is locked again.
8. Confirm inventory selection and view are reset.

### Input isolation

1. Open and close inventory while holding movement input.
2. Confirm no unintended movement occurs.
3. Restart while buttons are held.
4. Confirm stale input does not trigger an immediate action after re-entry.

---

## Non-Goals

* Implementing level 2
* Generalized scene management
* Save-game support
* Dynamic level loading
* General room scripting
* A reusable entity-component system
* General collision architecture
* Refactoring all assets into per-level packages
* Replacing the current inventory implementation

The change should create a clean level boundary without introducing unnecessary engine abstractions.
::: 

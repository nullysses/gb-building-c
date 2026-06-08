# Change Request: Add Two Procedural Game Boy Sound Effects

## Goal

Add a minimal sound-effects layer for the current GBDK prototype.

Implement two procedural SFX:

```text
1. Robot step sound
2. Text character print sound
```

These should use Game Boy sound registers directly. Do not add music, waveform assets, external sound drivers, or a full audio engine yet.

---

## Current Context

The game currently has:

```text
- opening typewriter text
- Window-layer text rendering
- 2-frame robot movement animation
- room movement and inspection
```

The next audio goal is to make the prototype feel more alive with two small sounds:

```text
robot walking  -> low mechanical step/thud
typewriter     -> tiny high tick per printed visible character
```

---

## Required Files

Add:

```text
src/sfx.h
src/sfx.c
```

Update:

```text
src/main.c
src/typewriter.c
Makefile
```

---

## Required API

Create `src/sfx.h`:

```c
#ifndef SFX_H
#define SFX_H

void sfx_init(void);
void sfx_robot_step(void);
void sfx_text_tick(void);

#endif
```

---

## Suggested Implementation

Create `src/sfx.c`:

```c
#include <gb/gb.h>

#include "sfx.h"

void sfx_init(void) {
    /* Enable sound globally. */
    NR52_REG = 0x80;

    /* Route all channels to both left and right speakers. */
    NR51_REG = 0xFF;

    /* Master volume: max left + max right. */
    NR50_REG = 0x77;
}

void sfx_robot_step(void) {
    /*
     * Noise channel: short low mechanical step.
     *
     * NR41: sound length
     * NR42: envelope
     * NR43: noise frequency/randomness
     * NR44: trigger
     */
    NR41_REG = 0x20;
    NR42_REG = 0x81;
    NR43_REG = 0x45;
    NR44_REG = 0x80;
}

void sfx_text_tick(void) {
    /*
     * Square channel 1: tiny high blip.
     *
     * NR10: sweep
     * NR11: duty/length
     * NR12: envelope
     * NR13/NR14: frequency + trigger
     */
    NR10_REG = 0x00;
    NR11_REG = 0x80;
    NR12_REG = 0x21;
    NR13_REG = 0xC0;
    NR14_REG = 0x87;
}
```

These register values are first-pass placeholders. They should compile and produce audible SFX, but may need tuning by ear.

---

## Makefile Change

Add `src/sfx.c` to `C_SOURCES`.

Example:

```make
C_SOURCES := \
	src/main.c \
	src/brickwall.c \
	src/floor.c \
	src/crate.c \
	src/door.c \
	src/robot.c \
	src/tiny_font.c \
	src/window_text.c \
	src/typewriter.c \
	src/sfx.c
```

Keep the actual list consistent with the current repository state. If `src/door.c` is not currently in `C_SOURCES`, do not add it just because it appears in this example.

---

## `main.c` Changes

Include the SFX header:

```c
#include "sfx.h"
```

Initialize sound once during startup, before gameplay begins.

Suggested placement:

```c
BGP_REG = 0xE4;
OBP0_REG = 0xE4;

sfx_init();
```

Do not repeatedly call `sfx_init()`.

---

## Robot Step Sound Behavior

Play `sfx_robot_step()` only when the robot’s walking animation advances, not every frame.

For the current two-frame robot animation, use a pattern like this:

```c
if (moving) {
    anim_timer++;

    if (anim_timer >= 10u) {
        anim_timer = 0u;
        robot_frame ^= 1u;

        if (robot_frame == 1u) {
            sfx_robot_step();
        }
    }
} else {
    anim_timer = 0u;
    robot_frame = 0u;
}
```

The exact animation threshold may differ from the current code. Preserve the current movement feel unless it needs minor tuning.

Important:

```text
Do not trigger a step sound every frame.
Do not trigger a step sound while idle.
```

---

## Text Tick Sound Behavior

Modify `src/typewriter.c`.

Include:

```c
#include "sfx.h"
```

When `typewriter_put_char()` writes a visible non-newline, non-null character to the Window tilemap, call:

```c
sfx_text_tick();
```

The tick should happen only for visible characters.

It should not tick for:

```text
'\\0'
'\\n'
```

Suggested placement:

```c
tile = tiny_font_tile_for_char(tw->font_base, c);

set_win_tiles(
    (uint8_t)(tw->box_x + tw->cursor_x),
    (uint8_t)(tw->box_y + tw->cursor_y),
    1u,
    1u,
    &tile
);

sfx_text_tick();
```

---

## Reveal-All Caveat

Current `typewriter_reveal_all()` loops through all remaining characters immediately.

If `sfx_text_tick()` is called inside `typewriter_put_char()`, reveal-all may trigger many ticks in the same frame.

For this change request, use the simple implementation first.

Acceptable behavior for now:

```text
A while typing may produce one messy/collapsed tick burst.
```

Do not add a more complex sound-suppression path unless the simple version sounds bad in testing.

Optional future refinement:

```c
static void typewriter_put_char(typewriter_t *tw, char c, uint8_t play_sound);
```

Do not implement that unless needed.

---

## Constraints

Keep this change small.

Do not add:

```text
- music
- sound engine
- waveform sample playback
- external audio driver
- interrupt-driven audio system
- stateful sound mixer
- per-room audio
- SFX priority system
```

This is a direct-register procedural SFX prototype.

---

## Acceptance Criteria

After the change:

1. `make clean && make` succeeds.
2. The game boots normally.
3. Opening text still types character-by-character.
4. A short tick/blip plays when visible characters are printed.
5. Newline and end-of-string do not intentionally trigger text ticks.
6. Robot movement still works.
7. Robot step sound plays while walking, but not on every frame.
8. Robot step sound does not play while idle.
9. Existing controls and room transitions still work.
10. No new sound files or audio assets are introduced.

---

## Manual Test Plan

Run the ROM.

### Opening

Check:

```text
- text appears normally
- each printed character produces a small tick/blip
- pressing A to reveal the page does not crash or hang
- page transitions still work
```

### Gameplay

Check:

```text
- robot walks left/right
- step sound occurs periodically while walking
- no step sound while idle
- A inspection still works
- START reset still works if currently implemented
- room transition still works
```

### Audio Tuning

If the sounds are too harsh, tune only the SFX register constants in `src/sfx.c`.

Suggested tuning targets:

```text
robot step:
  lower, shorter, more mechanical

text tick:
  higher, quieter, very short
```

Do not change game logic to compensate for bad SFX tuning.

---

## Notes for Future Work

Potential future audio improvements:

```text
- separate `sfx_text_tick()` suppression for reveal-all
- vary robot step pitch every other step
- add door locked buzz
- add terminal beep
- add cutscene transition sound
- eventually replace raw register pokes with a tiny SFX manager
```

Do not implement these now.

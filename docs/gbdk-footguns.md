# GBDK / Building C Footguns

Project notes for the `gb-building-c` Game Boy / GBDK-2020 toolchain.

These are not general laws. They are local lessons learned while building this project.

---

## 1. `GBDKDIR` needs a trailing slash

GBDK `lcc` expects `GBDKDIR` to end with `/`.

Correct:

```bash
export GBDKDIR="$HOME/.local/opt/gbdk/"
export GBDK_HOME="$GBDKDIR"
export PATH="${GBDKDIR}bin:$PATH"
```

Wrong:

```bash
export GBDKDIR="$HOME/.local/opt/gbdk"
```

The wrong version can produce paths like:

```text
/home/ulises/.local/opt/gbdkbin/sdcc
```

instead of:

```text
/home/ulises/.local/opt/gbdk/bin/sdcc
```

---

## 2. `png2asset` help behavior is crusty

This build of `png2asset` does not use `-h` normally.

Use:

```bash
png2asset
```

It may print usage and still emit a decoder warning. That does not necessarily mean the install is broken.

Actual conversion tests matter more than help output.

---

## 3. Use the right `png2asset` mode

Common modes used in this project:

### Raw tiles

Use for floor, brick, dingbats, and tile vocabulary sheets.

```bash
png2asset assets/floor.png \
  -tiles_only \
  -keep_palette_order \
  -o src/floor.c
```

### Ordered font/dingbat strips

Use conservative flags so tile order stays stable.

```bash
png2asset assets/dingbats.png \
  -tiles_only \
  -spr8x8 \
  -keep_palette_order \
  -keep_duplicate_tiles \
  -noflip \
  -o src/dingbats.c
```

Important: include `-spr8x8` for 8×8 font/dingbat tiles. Without it, `png2asset` may treat the output as 8×16 and double the tile count.

### Animated 16×16 sprites

Use metasprite slicing.

```bash
png2asset assets/robot.png \
  -spr8x16 \
  -sw 16 \
  -sh 16 \
  -keep_palette_order \
  -o src/robot.c
```

### Composite background objects

For objects like a 16×16 crate or larger door, prefer `-map` when you want the generated map to define the assembly order.

```bash
png2asset assets/door.png \
  -map \
  -keep_palette_order \
  -keep_duplicate_tiles \
  -noflip \
  -o src/door.c
```

Use `-tiles_only` only when you are intentionally managing tile order yourself.

---

## 4. Do not assume 16×16 `-tiles_only` order is row-major

Observed with `crate.png`:

Visual crate:

```text
TL TR
BL BR
```

Expected row-major assumption:

```text
0 1
2 3
```

Observed effective order:

```text
0 2
1 3
```

Correct macros for the crate were:

```c
#define TILE_CRATE_TL (CRATE_TILE_BASE + 0u)
#define TILE_CRATE_TR (CRATE_TILE_BASE + 2u)
#define TILE_CRATE_BL (CRATE_TILE_BASE + 1u)
#define TILE_CRATE_BR (CRATE_TILE_BASE + 3u)
```

Rule:

```text
For composite BG objects:
- validate tile order visually in an asset test ROM, or
- use png2asset -map and consume the generated map.
```

For one-row or one-column strips, the ordering ambiguity mostly disappears.

---

## 5. Background and Window share tile graphics

The Background and Window layers use separate tilemaps, but they share BG/Window tile pattern memory.

This is valid:

```c
set_bkg_data(FONT_TILE_BASE, TINY_FONT_TILE_COUNT, tiny_font_tiles);
set_win_tiles(0, 0, 20, 2, row_tiles);
```

The Window tilemap can reference font tile IDs loaded through `set_bkg_data()`.

---

## 6. Separate BG and Window tilemaps

Use separate BG and Window tilemap areas to avoid Window writes colliding with the room map.

```c
LCDC_REG &= ~LCDCF_BG9C00;  /* BG map:     $9800 */
LCDC_REG |=  LCDCF_WIN9C00; /* Window map: $9C00 */
```

Do this before writing BG or Window tilemaps.

---

## 7. `printf` is not free once using custom backgrounds

`printf()` uses GBDK’s console/font system on the background layer.

Once a custom BG map is active, avoid:

```c
printf("HELLO");
```

Prefer project-owned text functions that write to the Window layer:

```c
window_draw_row(FONT_TILE_BASE, 0, "BUILDING C");
```

---

## 8. Short text rows must not read past `\0`

Bad pattern:

```c
for (i = 0; i < WIN_W; ++i) {
    char c = text[i];
    row[i] = c ? tiny_font_tile_for_char(font_base, c)
               : tiny_font_tile_for_char(font_base, ' ');
}
```

This only handles the first null byte. After that, it keeps reading memory after the string literal and can leak adjacent text.

Safe pattern:

```c
void window_draw_row(uint8_t font_base, uint8_t y, const char *text) {
    uint8_t row[WIN_W];
    uint8_t i;
    uint8_t ended = 0u;

    for (i = 0u; i < WIN_W; ++i) {
        char c;

        if (ended) {
            c = ' ';
        } else {
            c = text[i];

            if (c == '\0') {
                c = ' ';
                ended = 1u;
            }
        }

        row[i] = tiny_font_tile_for_char(font_base, c);
    }

    set_win_tiles(0u, y, WIN_W, 1u, row);
}
```

---

## 9. Unknown font characters currently fall back to space or `?`

The font lookup scans `TINY_FONT_CHARS`.

If a character is not present, choose an explicit fallback policy.

Stable/release behavior:

```c
return tile_base; /* space */
```

Development behavior:

```c
return tiny_font_tile_for_char(tile_base, '?');
```

Avoid Unicode in ROM strings for now. Use ASCII symbols mapped to custom dingbats.

Current intended contract:

```c
#define TINY_FONT_CHARS " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/:.-!?@ab+<>^vx*se"
```

Dingbat meanings:

```text
@ = robot/system head
a = A button
b = B button
+ = D-pad cross
< = left arrow
> = right arrow
^ = up arrow
v = down arrow
x = cancel / blocked / no
* = attention / signal / important
s = START
e = SELECT
```

Lowercase letters are icons, not prose.

---

## 10. `SPRITES_8x16` needs even tile bases

In 8×16 sprite mode, sprite tile IDs must be even.

Use rounded bases:

```c
#define ROBOT_TILE_BASE ((FONT_TILE_BASE + TINY_FONT_TILE_COUNT + 1u) & 0xFEu)
```

The same base must be used in both places:

```c
set_sprite_data(ROBOT_TILE_BASE, robot_TILE_COUNT, robot_tiles);

move_metasprite(
    robot_metasprites[robot_frame],
    ROBOT_TILE_BASE,
    0,
    robot_x,
    robot_y
);
```

If the bases differ, sprites draw wrong tiles.

---

## 11. The A press that exits an opening can leak into gameplay

If the player exits the opening by pressing A, the first gameplay frame may still see A held.

Bad result:

```text
Opening exits
First gameplay frame reads A
Inspect text appears immediately
```

Fix by waiting for input release or initializing `prev_joy` after transition.

Helper:

```c
static void wait_for_input_release(void) {
    while (joypad()) {
        wait_vbl_done();
    }
}
```

Use after opening:

```c
run_opening(FONT_TILE_BASE);
wait_for_input_release();
```

---

## 12. One frame counter can be okay, but do not rely on exact hits if speed changes

Bug pattern:

```c
if ((frame & 15u) == 0u) {
    BGP_REG = palettes[(frame >> 4u) & 3u];
}

frame += speed;
```

If `speed == 2` and `frame` is odd, it may never hit a multiple of 16.

Primitive fix:

```c
uint8_t p = (frame >> 4u) & 3u;
BGP_REG = palettes[p];
OBP0_REG = palettes[p];

frame += speed;
```

No exact-hit condition, no missed event.

---

## 13. Check generated headers after asset changes

Useful checks:

```bash
grep TILE_COUNT src/*.h
grep -E 'WIDTH|HEIGHT|TILE_COUNT' src/crate.h src/robot.h
```

Expected examples:

```text
dingbats_TILE_COUNT == 12
crate_TILE_COUNT == 4
```

If tile counts drift, your hardcoded tile bases or map assumptions may be wrong.

---

## 14. Build artifact naming must stay boring

Avoid testing stale ROMs.

Use one canonical build target:

```text
build/demo001.gb
```

Optional committed release artifact:

```text
dist/demo001.gb
```

Do not manually switch between `hello.gb`, `demo001.gb`, and random copied ROM names while debugging.

---

## 15. Commit source assets, ignore generated outputs

Track:

```text
assets/*.png
assets/*.aseprite
src/main.c
src/tiny_font.c
src/tiny_font.h
src/window_text.c
src/window_text.h
src/typewriter.c
src/typewriter.h
Makefile
```

Ignore:

```text
build/
src/brickwall.c
src/brickwall.h
src/floor.c
src/floor.h
src/crate.c
src/crate.h
src/robot.c
src/robot.h
*.map
*.noi
*.sym
*.lst
*.rel
*.ihx
*.asm
*.o
```

Generated `png2asset` files should be reproducible from source PNGs.

---

## 16. Good local loop

```bash
make clean
make
make run
```

When debugging assets:

```bash
make clean
make
grep TILE_COUNT src/*.h
```

If something visual is wrong, first suspect:

```text
- stale ROM
- stale generated asset
- wrong tile order
- wrong tile base
- wrong sprite base
- BG/Window tilemap collision
```

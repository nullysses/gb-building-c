#include <gb/gb.h>

#include "sfx.h"

void sfx_init(void) {
    NR52_REG = 0x80;
    NR51_REG = 0xFF;
    NR50_REG = 0x77;
}

void sfx_robot_step(void) {
    NR41_REG = 0x20;
    NR42_REG = 0x81;
    NR43_REG = 0x45;
    NR44_REG = 0x80;
}

void sfx_text_tick(void) {
    NR10_REG = 0x00;
    NR11_REG = 0x80;
    NR12_REG = 0x21;
    NR13_REG = 0xC0;
    NR14_REG = 0x87;
}

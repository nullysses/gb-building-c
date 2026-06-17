// src/audio/music.c
#include <gb/gb.h>
#include "hUGEDriver.h"
#include "music.h"

extern const hUGESong_t bg_building;

static uint8_t music_vbl_installed = 0;

void music_init(void) {
    NR52_REG = 0x80;
    NR50_REG = 0x77;
    NR51_REG = 0xFF;

    if (!music_vbl_installed) {
        __critical {
            add_VBL(hUGE_dosound);
        }
        music_vbl_installed = 1;
    }
}

void music_play_building(void) {
    music_init();

    __critical {
        hUGE_init(&bg_building);
    }
}
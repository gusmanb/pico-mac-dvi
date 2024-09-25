#ifndef __MENU__
#define __MENU__

#include <inttypes.h>
#include <stdbool.h>
#include "ff.h"

#define FONT_CHAR_WIDTH 8
#define FONT_CHAR_HEIGHT 8
#define FONT_N_CHARS 95
#define FONT_FIRST_ASCII 32
#define VIDEO_FB_HRES           512
#define VIDEO_FB_VRES           342
#define STRIDE (VIDEO_FB_HRES / 8)
#define COLS (VIDEO_FB_HRES / 8)
#define ROWS (VIDEO_FB_VRES / 8)
#define FIRST_ITEM_ROW 5
#define ITEM_ROWS (ROWS - (FIRST_ITEM_ROW + 1))
void init_menu(uint8_t* fb, FATFS* filesystem);
bool process_menu(FIL* file);

#endif
#ifndef __MENU__
#define __MENU__

#include <inttypes.h>
#include <stdbool.h>

#define FONT_CHAR_WIDTH 8
#define FONT_CHAR_HEIGHT 8
#define FONT_N_CHARS 95
#define FONT_FIRST_ASCII 32
#define VIDEO_FB_HRES           512
#define VIDEO_FB_VRES           342
#define STRIDE (VIDEO_FB_HRES / 8)
#define COLS (VIDEO_FB_HRES / FONT_CHAR_WIDTH)
#define ROWS (VIDEO_FB_VRES / (FONT_CHAR_HEIGHT + 1))

void init_menu(uint8_t* fb);
void add_menu_entry(const char* entry);
void print_string(const char* str, uint16_t x, uint16_t y);
void print_char(const char c, uint16_t x, uint16_t y);
uint8_t process_menu();

#endif
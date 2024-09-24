#include "menu.h"
#include <string.h>
#include "font_8x8.h"
#include "kbd.h"
#include "keymap.h"

uint8_t* buffer;
uint8_t entry_count = 0;
uint8_t selected_entry = 0;

void init_menu(uint8_t* fb)
{
    buffer = fb;
    memset(buffer, 0, (VIDEO_FB_HRES * VIDEO_FB_VRES) / 8);
    entry_count = 0;
    selected_entry = 0;
}

void add_menu_entry(const char* entry)
{
    uint16_t row = (entry_count % (ROWS - 2)) + 1;
    uint16_t col = entry_count > ROWS - 2 ? 33 : 1;

    print_string(entry, col, row);

    if(entry_count++ == 0)
        print_char('>', col - 1, row);
}

void print_string(const char* str, uint16_t x, uint16_t y)
{
    uint16_t pos = 0;
    uint8_t cnt = 0;
    while(*str && cnt++ < 30)
    {
        print_char(*str, x + pos++, y);
        str++;
    }
}

unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

void print_char(const char c, uint16_t x, uint16_t y)
{
    uint16_t pos = y * 9 * STRIDE + x;
    uint16_t ch = c - FONT_FIRST_ASCII;

    for(uint8_t i = 0; i < 8; i++)
    {
        buffer[pos] = reverse(font_8x8[ch]);
        ch += FONT_N_CHARS;
        pos += STRIDE;
    }
}

void select_entry(uint8_t entry)
{
    uint16_t row = (selected_entry % (ROWS - 2)) + 1;
    uint16_t col = selected_entry > ROWS - 2 ? 32 : 0;

    print_char(' ', col, row);

    selected_entry = entry;

    row = (selected_entry % (ROWS - 2)) + 1;
    col = selected_entry > ROWS - 2 ? 32 : 0;

    print_char('>', col, row);
}

uint8_t process_menu()
{
    if(kbd_queue_empty())
        return 0xFF;

    uint16_t key = kbd_queue_pop();

    if(!(key & 0x8000))
    {
        return 0xFF;
    }

    uint8_t val = (key & 0xFF) >> 1;

    switch(val)
    {
        case MKC_Up:
        {
            uint8_t next = selected_entry + 1;
            if(next >= entry_count)
                next = 0;

            select_entry(next);
            break;
        }

        case MKC_Down:
        {
            uint8_t next = selected_entry - 1;
            if(next >= entry_count)
                next = entry_count - 1;

            select_entry(next);
            break;
        }

        case MKC_Enter:
        case MKC_Return:
            return selected_entry;
    }

    return 0xFF;
}
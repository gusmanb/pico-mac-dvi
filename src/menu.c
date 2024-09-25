#include "menu.h"
#include <string.h>
#include <math.h>
#include "font_8x8.h"
#include "kbd.h"
#include "keymap.h"
#include "tf_card.h"
#include "ff.h"

static FATFS* fs;
uint8_t* buffer;

#define PAGE_SIZE (COLS * ROWS)

uint32_t current_page = 0;
uint32_t current_entry = 0;
uint32_t entry_count = 0;
uint32_t total_entries = 0;
uint32_t total_pages = 0;
DIR current_dir;
FILINFO current_file;
char current_path[256];


//Reverse byte (MAC screen has bits reversed)
unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

//Print a char in the screen
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


//Print a string in screen
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


//Add an entry to the menu
void add_menu_entry(const char* entry, bool isFolder)
{
    uint16_t row = (entry_count % ITEM_ROWS) + FIRST_ITEM_ROW;
    uint16_t col = entry_count >= ITEM_ROWS ? 33 : 1;

    if(isFolder)
    {
        print_char('/', col, row);
        print_string(entry, col + 1, row);    
    }
    else
        print_string(entry, col, row); 

    entry_count++;
}

//Select an entry in screen (adds ">" in front of it)
void select_entry(uint8_t entry)
{
    uint16_t row = (current_entry % ITEM_ROWS) + FIRST_ITEM_ROW;
    uint16_t col = current_entry >= ITEM_ROWS ? 32 : 0;

    print_char(' ', col, row);

    current_entry = entry;

    row = (current_entry % ITEM_ROWS) + FIRST_ITEM_ROW;
    col = current_entry >= ITEM_ROWS ? 32 : 0;

    print_char('>', col, row);
}

//Seeks in the directory for an entry based on its index, leaves the DIR ready for READDIR
void seek_entry(uint32_t page, uint32_t entry)
{
    f_rewinddir(&current_dir);
    uint32_t seeks = page * PAGE_SIZE + entry;

    //First entry is our fake "..", second entry is entry 0 (no seek needed)
    if(seeks < 2)
        return;

    seeks--; //Reduce one the number of seeks

    while(seeks--)
        f_readdir(&current_dir, &current_file);
}

//Read a page of entries
void read_page()
{
    //Erase video memory
    memset(buffer, 0, (VIDEO_FB_HRES * VIDEO_FB_VRES) / 8);

    //Add headers
    print_string("IMAGE SELECTOR", COLS / 2 - 7, 1);

    print_string("PATH: ", 1, 3);
    print_string(current_path, 6, 3);

    //Reset entry info
    current_entry = 0;
    entry_count = 0;

    //Compute how many entries in this page
    uint32_t entries_in_page = MIN(PAGE_SIZE, total_entries - (current_page * PAGE_SIZE));

    //Populate entries
    for(int buc = 0; buc < entries_in_page; buc++)
    {
        //Add ".. as the first entry of the first page"
        if(buc == 0 && current_page == 0)
        {
            add_menu_entry("..", true);
        }
        else
        {
            if(f_readdir(&current_dir, &current_file) == FR_OK)
                add_menu_entry(current_file.fname, current_file.fattrib & AM_DIR);
        }
    }

    select_entry(0);
}

//Open specified folder
void open_folder(char* path)
{
    //Assume filesystem is already initialized
    f_opendir(&current_dir, path);
    total_entries = 0;
    current_page = 0;
    current_entry = 0;

    while (f_readdir(&current_dir, &current_file) == FR_OK)
    {
        if(current_file.fname[0] == 0)
            break;
        
        total_entries++;
    }
    
    f_rewinddir(&current_dir);

    //Add one entry for ".."
    total_entries++;

    //Calculate total pages in this folder
    total_pages = ceil((double)total_entries / (double)PAGE_SIZE);

    //Read first page of the folder
    read_page();
}

//Move to the next page in the current folder
void next_page()
{
    current_page++;

    if(current_page >= total_pages)
        current_page = 0;

    //In this case the seek could be skipped or replaced by a rewind
    //depending on the case, but for clarity both next/prev functions
    //use the same code.
    seek_entry(current_page, 0);
    
    read_page();
}

//Move to the previous page in the current folder
void prev_page()
{
    current_page--;

    if(current_page >= total_pages)
        current_page = total_pages - 1;
    
    //Search for the first entry in the page
    seek_entry(current_page, 0);

    read_page();
}

//Initialize menu
void init_menu(uint8_t* fb, FATFS* filesystem)
{
    //Store variables
    buffer = fb;
    fs = filesystem;
    
    //Initialize path to "/"
    current_path[0] = '/';
    current_path[1] = 0;

    //Open root folder
    open_folder(current_path);
}



//Move up in the folder tree
void prev_folder()
{
    char* pos = strrchr(current_path, '/');

    if(pos == NULL || pos == current_path) //Not found or first slash?
        return;

    *pos = 0; //Put a null char, terminating the string where the slash was

    open_folder(current_path);
}

//Move deeped in the folder tree
void next_folder(char* new_path)
{
    strcat(current_path, "/");
    strcat(current_path, new_path);
    open_folder(current_path);
}

//PRocess menu actions
bool process_menu(FIL* file)
{
    if(kbd_queue_empty())
        return false;

    uint16_t key = kbd_queue_pop();

    //Only react to press
    if(!(key & 0x8000))
    {
        return false;
    }

    //Compute key value
    uint8_t val = (key & 0xFF) >> 1;

    switch(val)
    {
        //Down, move to the next entry
        case MKC_Down:
        {
            uint8_t next = current_entry + 1;
            if(next >= entry_count)
                next = 0;

            select_entry(next);
            break;
        }

        //Up move to the previous entry
        case MKC_Up:
        {
            uint8_t next = current_entry - 1;
            if(next >= entry_count)
                next = entry_count - 1;

            select_entry(next);
            break;
        }

        //Left goes to the previous page
        case MKC_Left:
            prev_page();
            break;;

        //Right moves to the next page
        case MKC_Right:
            next_page();
            break;

        //Return selects an entry
        //If it is a file, it is selected as the disk image.
        //If it is a folder, enters to it
        //If it is ".." moves up one folder
        case MKC_Enter:
        case MKC_Return:

            if(current_page == 0 && current_entry == 0)
            {
                f_closedir(&current_dir);
                prev_folder();
            }
            else
            {
                
                seek_entry(current_page, current_entry);
                f_readdir(&current_dir, &current_file);
                f_closedir(&current_dir);

                if(current_file.fattrib & AM_DIR)
                    next_folder(current_file.fname);
                else
                {
                    f_open(file, strcat(current_path, current_file.fname), FA_OPEN_EXISTING | FA_READ | FA_WRITE);
                    return true;
                }

            }

            break;

        //Esc cancels selection (defaults to embedded flash image)
        case MKC_Escape:

            f_closedir(&current_dir);
            file->err = 0xFF;
            return true;
    }

    return false;
}
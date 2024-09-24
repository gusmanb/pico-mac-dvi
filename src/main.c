/* pico-umac
 *
 * Main loop to initialise umac, and run main event loop (piping
 * keyboard/mouse events in).
 *
 * Copyright 2024 Matt Evans
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Modified by El Dr. Gusman with HDMI and image selector
 * (C) 2024
 * 
 */

#include <stdio.h>
#include <unistd.h>
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hw.h"
#include "kbd.h"
#include "menu.h"
#include "bsp/rp2040/board.h"
#include "tusb.h"
#include "umac.h"
#include "tf_card.h"
#include "ff.h"
#include "video.h"

////////////////////////////////////////////////////////////////////////////////
// Imports and data

extern void     hid_app_task(void);
extern int cursor_x;
extern int cursor_y;
extern int cursor_button;

static pico_fatfs_spi_config_t picofat_config =
{
    .spi_inst = spi0,
    .clk_slow = CLK_SLOW_DEFAULT,
    .clk_fast = SD_MHZ * 1000 * 1000,
    .pin_miso = SD_RX,
    .pin_mosi = SD_TX,
    .pin_sck = SD_SCK,
    .pin_cs = SD_CS,
    .pullup = false
};

static FATFS fs;

// Mac binary data:  disc and ROM images
static const uint8_t umac_disc[] = {
#include "umac-disc.h"
};
static const uint8_t umac_rom[] = {
#include "umac-rom.h"
};

static uint8_t umac_ram[RAM_SIZE];

////////////////////////////////////////////////////////////////////////////////

static void     poll_led_etc()
{
        static int led_on = 0;
        static absolute_time_t last = 0;
        absolute_time_t now = get_absolute_time();
}

static int umac_cursor_x = 0;
static int umac_cursor_y = 0;
static int umac_cursor_button = 0;

static void     poll_umac()
{
        static absolute_time_t last_1hz = 0;
        static absolute_time_t last_vsync = 0;
        absolute_time_t now = get_absolute_time();

        umac_loop();

        int64_t p_1hz = absolute_time_diff_us(last_1hz, now);
        int64_t p_vsync = absolute_time_diff_us(last_vsync, now);
        if (p_vsync >= 16667) {
                /* FIXME: Trigger this off actual vsync */
                umac_vsync_event();
                last_vsync = now;
        }
        if (p_1hz >= 1000000) {
                umac_1hz_event();
                last_1hz = now;
        }

        int update = 0;
        int dx = 0;
        int dy = 0;
        int b = umac_cursor_button;
        if (cursor_x != umac_cursor_x) {
                dx = cursor_x - umac_cursor_x;
                umac_cursor_x = cursor_x;
                update = 1;
        }
        if (cursor_y != umac_cursor_y) {
                dy = cursor_y - umac_cursor_y;
                umac_cursor_y = cursor_y;
                update = 1;
        }
        if (cursor_button != umac_cursor_button) {
                b = cursor_button;
                umac_cursor_button = cursor_button;
                update = 1;
        }
        if (update) {
                umac_mouse(dx, -dy, b);
        }

        if (!kbd_queue_empty()) {
                uint16_t k = kbd_queue_pop();
                umac_kbd_event(k & 0xff, !!(k & 0x8000));
        }
}

static int      disc_do_read(void *ctx, uint8_t *data, unsigned int offset, unsigned int len)
{
        FIL *fp = (FIL *)ctx;
        f_lseek(fp, offset);
        unsigned int did_read = 0;
        FRESULT fr = f_read(fp, data, len, &did_read);
        if (fr != FR_OK || len != did_read) {
                return -1;
        }
        return 0;
}

static int      disc_do_write(void *ctx, uint8_t *data, unsigned int offset, unsigned int len)
{
        FIL *fp = (FIL *)ctx;
        f_lseek(fp, offset);
        unsigned int did_write = 0;
        FRESULT fr = f_write(fp, data, len, &did_write);
        if (fr != FR_OK || len != did_write) {
                return -1;
        }
        return 0;
}

static FIL discfp;

static void     disc_setup(disc_descr_t discs[DISC_NUM_DRIVES])
{

        char *disc0_name;
        const char *disc_pattern = "*.img";

        /* Mount SD filesystem */
        pico_fatfs_set_config(&picofat_config);
        FRESULT fr = f_mount(&fs, "", 1);
        if (fr != FR_OK) {
                goto no_sd;
        }

        /* Look for a disc image */
        DIR di = {0};
        FILINFO fi = {0};

        fr = f_findfirst(&di, &fi, "/", disc_pattern);
        if (fr != FR_OK) {
                goto no_sd;
        }

        //If we found at least one image, start selector menu
        init_menu(umac_ram);

        //List all .img files
        while(fr == FR_OK && fi.fname[0])
        {
                add_menu_entry(fi.fname);
                fr = f_findnext(&di, &fi);
        }

        f_closedir(&di);

        uint8_t selected = 0xFF;
        //Wait for the user to select one
        while (selected == 0xFF)
        {
                selected = process_menu();
                tuh_task();
                hid_app_task();
        }

        //Open the selected image 
        fr = f_findfirst(&di, &fi, "/", disc_pattern);
        if (fr != FR_OK) {
                goto no_sd;
        }

        while (selected--)
        {
                fr = f_findnext(&di, &fi);
                if (fr != FR_OK) {
                        goto no_sd;
                }
        }
        
        disc0_name = fi.fname;
        f_closedir(&di);

        /* Open image, set up disc info: */
        fr = f_open(&discfp, disc0_name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
        if (fr != FR_OK && fr != FR_EXIST) {
                goto no_sd;
        } else {
                discs[0].base = 0; // Means use R/W ops
                discs[0].read_only = false;
                discs[0].size = f_size(&discfp);
                discs[0].op_ctx = &discfp;
                discs[0].op_read = disc_do_read;
                discs[0].op_write = disc_do_write;
        }

        return;

no_sd:

        /* If we don't find an SD-based image, attempt
         * to use in-flash disc image:
         */
        discs[0].base = (void *)umac_disc;
        discs[0].read_only = 1;
        discs[0].size = sizeof(umac_disc);
}

static void     core1_main()
{
        disc_descr_t discs[DISC_NUM_DRIVES] = {0};

        tusb_init();

        disc_setup(discs);

        umac_init(umac_ram, (void *)umac_rom, discs);
        set_framebuffer((uint8_t *)(umac_ram + umac_get_fb_offset()));

        while (true) {
                poll_umac();
                tuh_task();
                hid_app_task();
        }
}

int main()
{
        memset(umac_ram, 0xFA, 128 * 1024);
        //Init video using the umac ram as framebuffer, used by the menu
        video_init();
        set_framebuffer(umac_ram);

        //Launch main code
        multicore_launch_core1(core1_main);

        //Infinite loop
	while(true)
                __wfi();
                
	return 0;
}


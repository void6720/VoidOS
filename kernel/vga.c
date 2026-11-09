#include "vga.h"
#include "io.h"

#define VIDEO_MEMORY ((volatile uint16_t *)0xB8000)
#define MAX_ROWS 25
#define MAX_COLS 80

static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t attr = VGA_LIGHT_GREY;

static uint16_t cell(char c) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(cursor_y * MAX_COLS + cursor_x);
    port_byte_out(0x3D4, 0x0F);
    port_byte_out(0x3D5, (uint8_t)(pos & 0xFF));
    port_byte_out(0x3D4, 0x0E);
    port_byte_out(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void) {
    for (int y = 1; y < MAX_ROWS; y++)
        for (int x = 0; x < MAX_COLS; x++)
            VIDEO_MEMORY[(y - 1) * MAX_COLS + x] = VIDEO_MEMORY[y * MAX_COLS + x];
    for (int x = 0; x < MAX_COLS; x++)
        VIDEO_MEMORY[(MAX_ROWS - 1) * MAX_COLS + x] = cell(' ');
    cursor_y = MAX_ROWS - 1;
}

void vga_set_attr(uint8_t a) { attr = a; }

void vga_clear(void) {
    for (int i = 0; i < MAX_ROWS * MAX_COLS; i++)
        VIDEO_MEMORY[i] = cell(' ');
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void vga_init(void) {
    attr = VGA_LIGHT_GREY;
    vga_clear();
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else {
        VIDEO_MEMORY[cursor_y * MAX_COLS + cursor_x] = cell(c);
        cursor_x++;
    }
    if (cursor_x >= MAX_COLS) {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= MAX_ROWS) scroll();
    update_cursor();
}

void vga_puts(const char *s) {
    while (*s) vga_putc(*s++);
}

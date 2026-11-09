#pragma once
#include <stdint.h>

/* VGA text mode console: 80x25 characters at physical address 0xB8000. */
#define VGA_WHITE_ON_BLACK 0x0F
#define VGA_WHITE_ON_BLUE  0x1F
#define VGA_LIGHT_GREY     0x07

void vga_init(void);
void vga_set_attr(uint8_t attr);
void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char *s);

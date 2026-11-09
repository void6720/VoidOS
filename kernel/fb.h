#pragma once
#include <stdint.h>
#include "bootinfo.h"

/* Linear framebuffer driver with a software backbuffer. All drawing goes
   to the backbuffer; fb_flush* copies regions to the real framebuffer.
   The mouse cursor is drawn directly on the front buffer (fb_cursor_*). */
int fb_init(const boot_info_t *bi);      /* 1 = framebuffer ready */
int fb_width(void);
int fb_height(void);
uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b);

void fb_clear(uint32_t color);
void fb_pixel(int x, int y, uint32_t color);
void fb_fill(int x, int y, int w, int h, uint32_t color);
void fb_outline(int x, int y, int w, int h, uint32_t color);
void fb_vgradient(int y0, int y1, uint32_t top, uint32_t bottom);
void fb_char(int x, int y, char c, uint32_t fg, int32_t bg); /* bg<0 = transparent */
void fb_text(int x, int y, const char *s, uint32_t fg);
void fb_text_bg(int x, int y, const char *s, uint32_t fg, uint32_t bg);
int fb_text_width(const char *s);

void fb_flush(void);
void fb_flush_rect(int x, int y, int w, int h);

void fb_cursor_hide(void);
void fb_cursor_show(int x, int y);

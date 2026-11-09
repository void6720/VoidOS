#include "fb.h"
#include "font.h"

#define FB_MAX_W 1024
#define FB_MAX_H 768

static uint32_t *front;              /* real framebuffer (memory-mapped) */
static uint32_t pitch_bytes;
static int W, H;
static uint8_t rpos, gpos, bpos;
/* 3 MiB backbuffer. It MUST live above the 0xA0000-0xFFFFF hardware hole
   (VGA aperture + ROM shadow), so it gets its own section that the linker
   places at 0x200000 (see linker.ld, NOLOAD). fb_init zeroes it. */
static uint32_t back[FB_MAX_W * FB_MAX_H]
    __attribute__((section(".fbbss"), aligned(4096)));

int fb_init(const boot_info_t *bi) {
    if (!bi || bi->magic != BOOTINFO_MAGIC || bi->fb_phys == 0)
        return 0;
    if (bi->width > FB_MAX_W || bi->height > FB_MAX_H || bi->bpp != 32)
        return 0;
    front = (uint32_t *)bi->fb_phys;
    pitch_bytes = bi->pitch ? bi->pitch : bi->width * 4;
    for (int i = 0; i < FB_MAX_W * FB_MAX_H; i++)
        back[i] = 0;
    W = (int)bi->width;
    H = (int)bi->height;
    rpos = bi->red_pos;
    gpos = bi->green_pos;
    bpos = bi->blue_pos;
    return 1;
}

int fb_width(void) { return W; }
int fb_height(void) { return H; }

uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << rpos) | ((uint32_t)g << gpos) | ((uint32_t)b << bpos);
}

static void rgb_split(uint32_t c, int *r, int *g, int *b) {
    *r = (c >> rpos) & 0xFF;
    *g = (c >> gpos) & 0xFF;
    *b = (c >> bpos) & 0xFF;
}

void fb_clear(uint32_t color) {
    for (int i = 0; i < W * H; i++)
        back[i] = color;
}

void fb_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < W && y >= 0 && y < H)
        back[y * W + x] = color;
}

void fb_fill(int x, int y, int w, int h, uint32_t color) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            back[yy * W + xx] = color;
}

void fb_outline(int x, int y, int w, int h, uint32_t color) {
    fb_fill(x, y, w, 1, color);
    fb_fill(x, y + h - 1, w, 1, color);
    fb_fill(x, y, 1, h, color);
    fb_fill(x + w - 1, y, 1, h, color);
}

void fb_vgradient(int y0, int y1, uint32_t top, uint32_t bottom) {
    int tr, tg, tb, br, bg, bb;
    rgb_split(top, &tr, &tg, &tb);
    rgb_split(bottom, &br, &bg, &bb);
    int span = (y1 - y0) > 0 ? (y1 - y0) : 1;
    for (int y = y0; y < y1 && y < H; y++) {
        int t = y - y0;
        uint32_t c = fb_rgb(tr + (br - tr) * t / span,
                            tg + (bg - tg) * t / span,
                            tb + (bb - tb) * t / span);
        for (int x = 0; x < W; x++)
            back[y * W + x] = c;
    }
}

void fb_char(int x, int y, char c, uint32_t fg, int32_t bg) {
    unsigned char uc = (unsigned char)c;
    if (uc < FONT_FIRST || uc > FONT_LAST)
        uc = '?';
    const uint8_t *glyph = font10x20[uc - FONT_FIRST];
    for (int row = 0; row < FONT_H; row++) {
        uint8_t b0 = glyph[row * 2], b1 = glyph[row * 2 + 1];
        for (int col = 0; col < FONT_W; col++) {
            int on = col < 8 ? (b0 >> (7 - col)) & 1 : (b1 >> (7 - (col - 8))) & 1;
            if (on)
                fb_pixel(x + col, y + row, fg);
            else if (bg >= 0)
                fb_pixel(x + col, y + row, (uint32_t)bg);
        }
    }
}

void fb_text_bg(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
    for (; *s; s++, x += FONT_W)
        fb_char(x, y, *s, fg, (int32_t)bg);
}

void fb_text(int x, int y, const char *s, uint32_t fg) {
    for (; *s; s++, x += FONT_W)
        fb_char(x, y, *s, fg, -1);
}

int fb_text_width(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n * FONT_W;
}

void fb_flush(void) { fb_flush_rect(0, 0, W, H); }

void fb_flush_rect(int x, int y, int w, int h) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    if (x1 <= x0 || y1 <= y0) return;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t *dst = (uint32_t *)((uint8_t *)front + yy * pitch_bytes) + x0;
        const uint32_t *src = back + yy * W + x0;
        for (int xx = x0; xx < x1; xx++)
            *dst++ = *src++;
    }
}

/* ---------------- mouse cursor (front buffer, save-under) ---------------- */
#define CUR_W 12
#define CUR_H 18
static const char *cursor_shape[CUR_H] = {
    "X           ",
    "XX          ",
    "XOX         ",
    "XOOX        ",
    "XOOOX       ",
    "XOOOOX      ",
    "XOOOOOX     ",
    "XOOOOOOX    ",
    "XOOOOOOOOX  ",
    "XOOOOOX     ",
    "XOOXOX      ",
    "XOX XOX     ",
    "XX  XOX     ",
    "X    XOX    ",
    "     XOX    ",
    "     XOX    ",
    "      XX    ",
    "            ",
};
static uint32_t cur_under[CUR_W * CUR_H];
static int cur_x = -1, cur_y = -1, cur_shown;

void fb_cursor_hide(void) {
    if (!cur_shown) return;
    for (int yy = 0; yy < CUR_H; yy++) {
        int sy = cur_y + yy;
        if (sy < 0 || sy >= H) continue;
        uint32_t *dst = (uint32_t *)((uint8_t *)front + sy * pitch_bytes);
        for (int xx = 0; xx < CUR_W; xx++) {
            int sx = cur_x + xx;
            if (sx >= 0 && sx < W)
                dst[sx] = cur_under[yy * CUR_W + xx];
        }
    }
    cur_shown = 0;
}

void fb_cursor_show(int x, int y) {
    if (cur_shown) fb_cursor_hide();
    cur_x = x;
    cur_y = y;
    uint32_t black = fb_rgb(0, 0, 0), white = fb_rgb(255, 255, 255);
    for (int yy = 0; yy < CUR_H; yy++) {
        int sy = y + yy;
        for (int xx = 0; xx < CUR_W; xx++) {
            int sx = x + xx;
            uint32_t saved = 0;
            if (sy >= 0 && sy < H && sx >= 0 && sx < W) {
                uint32_t *fp = (uint32_t *)((uint8_t *)front + sy * pitch_bytes) + sx;
                saved = *fp;
                char p = cursor_shape[yy][xx];
                if (p == 'X') *fp = black;
                else if (p == 'O') *fp = white;
            }
            cur_under[yy * CUR_W + xx] = saved;
        }
    }
    cur_shown = 1;
}

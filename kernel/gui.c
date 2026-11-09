#include "gui.h"
#include "fb.h"
#include "font.h"
#include "io.h"
#include "interrupts.h"
#include "keyboard.h"
#include "mouse.h"

/* ---------------- small helpers (freestanding: no libc) ---------------- */
static int g_streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static int g_strneq(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}
static void fmt2(char *out, int v) { /* two digits */
    out[0] = '0' + (v / 10) % 10;
    out[1] = '0' + v % 10;
}
static void fmt_u32(char *out, uint32_t v) {
    char tmp[12];
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0;
}

/* ---------------- palette (built in gui_run via fb_rgb) ---------------- */
static uint32_t C_WALL_TOP, C_WALL_BOT, C_GLOW1, C_GLOW2;
static uint32_t C_TASKBAR, C_TASKBAR_LINE, C_BTN, C_BTN_LINE;
static uint32_t C_TITLE_ON, C_TITLE_OFF, C_CLOSE, C_WIN_EDGE;
static uint32_t C_TERM_BG, C_TERM_FG, C_TERM_DIM, C_TERM_PROMPT;
static uint32_t C_ABOUT_BG, C_ABOUT_FG, C_SHADOW, C_WHITE, C_MENU;
static uint32_t C_LOGO[4];

#define TASKBAR_H 44
#define TITLE_H 30

/* ---------------- windows ---------------- */
enum { WIN_TERM = 0, WIN_ABOUT = 1, WIN_COUNT = 2 };
typedef struct {
    int x, y, w, h;
    int visible;
    const char *title;
} win_t;
static win_t wins[WIN_COUNT];
static int zorder[WIN_COUNT] = {WIN_TERM, WIN_ABOUT}; /* bottom .. top */
static int menu_open;
static const char *g_vendor = "?";

static int win_focused(void) {
    for (int i = WIN_COUNT - 1; i >= 0; i--)
        if (wins[zorder[i]].visible) return zorder[i];
    return -1;
}
static void win_raise(int id) {
    int pos = 0;
    for (int i = 0; i < WIN_COUNT; i++)
        if (zorder[i] == id) pos = i;
    for (int i = pos; i < WIN_COUNT - 1; i++)
        zorder[i] = zorder[i + 1];
    zorder[WIN_COUNT - 1] = id;
}
static int in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

/* ---------------- terminal + tiny shell ---------------- */
#define TMAXC 80
#define TMAXR 24
static char tgrid[TMAXR][TMAXC];
static int tcols = 68, trows = 19, tcx, tcy;
static char input_buf[96];
static int input_len;

static void term_scroll(void) {
    for (int r = 0; r < trows - 1; r++)
        for (int c = 0; c < tcols; c++)
            tgrid[r][c] = tgrid[r + 1][c];
    for (int c = 0; c < tcols; c++)
        tgrid[trows - 1][c] = ' ';
    tcy = trows - 1;
}
static void term_newline(void) {
    tcx = 0;
    if (++tcy >= trows) term_scroll();
}
static void term_putc(char c) {
    if (c == '\n') { term_newline(); return; }
    if (tcx >= tcols) term_newline();
    tgrid[tcy][tcx++] = c;
}
static void term_puts(const char *s) { while (*s) term_putc(*s++); }
static void term_prompt(void) { term_puts("void@voidos:~$ "); }

/* Used by the Start menu's "Boot log" item. */
static void shell_exec_bootlog(void) {
    term_puts("[ ok ] bootloader handoff, 32-bit protected mode + GDT\n");
    term_puts("[ ok ] IDT + PIC: timer / keyboard / mouse IRQs live\n");
    term_puts("[ ok ] VBE framebuffer desktop running\n");
    term_prompt();
}

/* CMOS RTC clock (HH:MM:SS), for the taskbar and the `time` command. */
static void rtc_time_str(char out[9]) {
    port_byte_out(0x70, 0x00);
    uint8_t sec = port_byte_in(0x71);
    port_byte_out(0x70, 0x02);
    uint8_t min = port_byte_in(0x71);
    port_byte_out(0x70, 0x04);
    uint8_t hr = port_byte_in(0x71);
    port_byte_out(0x70, 0x0B);
    uint8_t regb = port_byte_in(0x71);
    if (!(regb & 0x04)) { /* BCD mode */
        sec = (sec & 0x0F) + (sec >> 4) * 10;
        min = (min & 0x0F) + (min >> 4) * 10;
        hr = ((hr & 0x0F) + (hr >> 4) * 10) | (hr & 0x80);
    }
    if (!(regb & 0x02) && (hr & 0x80)) { /* 12-hour mode */
        hr = ((hr & 0x7F) % 12) + 12;
    }
    hr &= 0x3F;
    fmt2(out + 0, hr);
    out[2] = ':';
    fmt2(out + 3, min);
    out[5] = ':';
    fmt2(out + 6, sec);
    out[8] = 0;
}

static void shell_exec(void) {
    input_buf[input_len] = 0;
    const char *cmd = input_buf;
    if (g_streq(cmd, "")) {
        /* just a fresh prompt */
    } else if (g_streq(cmd, "help")) {
        term_puts("commands: help  ver  about  echo <text>  ls  uptime  time\n");
        term_puts("          bootlog  gui  clear   (ESC = Start menu)\n");
    } else if (g_streq(cmd, "ver")) {
        term_puts("VoidOS 0.1.0 - freestanding C + NASM assembly kernel\n");
    } else if (g_streq(cmd, "about")) {
        wins[WIN_ABOUT].visible = 1;
        win_raise(WIN_ABOUT);
        term_puts("About window opened. CPU: ");
        term_puts(g_vendor);
        term_putc('\n');
    } else if (g_strneq(cmd, "echo ", 5)) {
        term_puts(cmd + 5);
        term_putc('\n');
    } else if (g_streq(cmd, "echo")) {
        term_putc('\n');
    } else if (g_streq(cmd, "ls")) {
        term_puts("bin  etc  home  lib  proc  root  sbin  sys  usr  var\n");
        term_puts("(rootfs staging tree - initramfs mount is milestone 3)\n");
    } else if (g_streq(cmd, "uptime")) {
        char b[16];
        fmt_u32(b, timer_ticks() / 100);
        term_puts("up ");
        term_puts(b);
        term_puts(" seconds\n");
    } else if (g_streq(cmd, "time")) {
        char t[9];
        rtc_time_str(t);
        term_puts(t);
        term_putc('\n');
    } else if (g_streq(cmd, "bootlog")) {
        term_puts("[ ok ] bootloader handoff, 32-bit protected mode + GDT\n");
        term_puts("[ ok ] IDT + PIC: timer / keyboard / mouse IRQs live\n");
        term_puts("[ ok ] VBE framebuffer desktop running\n");
    } else if (g_streq(cmd, "gui")) {
        term_puts("VoidOS desktop: VBE framebuffer, draggable windows,\n");
        term_puts("taskbar + Start menu, PS/2 mouse and keyboard.\n");
    } else if (g_streq(cmd, "clear")) {
        for (int r = 0; r < trows; r++)
            for (int c = 0; c < tcols; c++)
                tgrid[r][c] = ' ';
        tcx = 0;
        tcy = 0;
    } else {
        term_puts("unknown command: ");
        term_puts(cmd);
        term_puts("  (try 'help')\n");
    }
    input_len = 0;
    term_prompt();
}

static void shell_key(char c) {
    if (c == '\n') {
        term_newline();
        shell_exec();
    } else if (c == '\b') {
        if (input_len > 0) {
            input_len--;
            if (tcx > 0) {
                tcx--;
                tgrid[tcy][tcx] = ' ';
            }
        }
    } else if (c >= 32 && c < 127) {
        if (input_len < (int)sizeof(input_buf) - 1) {
            input_buf[input_len++] = c;
            term_putc(c);
        }
    }
}

static void term_init_text(void) {
    for (int r = 0; r < TMAXR; r++)
        for (int c = 0; c < TMAXC; c++)
            tgrid[r][c] = ' ';
    tcx = 0;
    tcy = 0;
    term_puts("VoidOS terminal - milestone 2 desktop\n");
    term_puts("Type 'help' for commands. Drag windows by their title bars.\n\n");
    term_prompt();
}

/* ---------------- drawing ---------------- */
static void draw_logo(int x, int y, int cell) {
    fb_fill(x, y, cell, cell, C_LOGO[0]);
    fb_fill(x + cell + 2, y, cell, cell, C_LOGO[1]);
    fb_fill(x, y + cell + 2, cell, cell, C_LOGO[2]);
    fb_fill(x + cell + 2, y + cell + 2, cell, cell, C_LOGO[3]);
}

static void draw_window_frame(const win_t *w, int focused) {
    fb_fill(w->x + 6, w->y + 8, w->w, w->h, C_SHADOW);       /* drop shadow */
    fb_fill(w->x, w->y, w->w, TITLE_H, focused ? C_TITLE_ON : C_TITLE_OFF);
    fb_text(w->x + 10, w->y + 5, w->title, C_WHITE);
    fb_fill(w->x + w->w - 40, w->y + 4, 34, 22, C_CLOSE);  /* close box */
    fb_text(w->x + w->w - 28, w->y + 5, "X", C_WHITE);
    fb_outline(w->x, w->y, w->w, w->h, C_WIN_EDGE);
}

static void draw_term(const win_t *w, int focused) {
    fb_fill(w->x, w->y + TITLE_H, w->w, w->h - TITLE_H, C_TERM_BG);
    int ox = w->x + 10, oy = w->y + TITLE_H + 8;
    for (int r = 0; r < trows; r++) {
        for (int c = 0; c < tcols; c++) {
            char ch = tgrid[r][c];
            if (ch != ' ')
                fb_char(ox + c * FONT_W, oy + r * FONT_H, ch, C_TERM_FG, -1);
        }
    }
    /* text cursor: solid block on the prompt colour, blinking at 2 Hz */
    if (focused && ((timer_ticks() / 50) % 2 == 0))
        fb_fill(ox + tcx * FONT_W, oy + tcy * FONT_H + FONT_H - 3,
                FONT_W, 3, C_TERM_PROMPT);
    (void)focused;
}

static void draw_about(const win_t *w) {
    fb_fill(w->x, w->y + TITLE_H, w->w, w->h - TITLE_H, C_ABOUT_BG);
    int x = w->x + 18, y = w->y + TITLE_H + 14;
    draw_logo(x, y + 2, 13);
    fb_text(x + 44, y, "VoidOS 0.1.0", C_WHITE);
    fb_text(x + 44, y + 22, "a from-scratch x86 OS", C_ABOUT_FG);
    y += 64;
    fb_text(x, y, "Kernel:   freestanding C + NASM asm", C_ABOUT_FG);
    fb_text(x, y + 24, "Display:  VBE linear framebuffer", C_ABOUT_FG);
    char line[48];
    int n = 0;
    const char *p = "CPU:      ";
    while (*p) line[n++] = *p++;
    p = g_vendor;
    while (*p && n < 46) line[n++] = *p++;
    line[n] = 0;
    fb_text(x, y + 48, line, C_ABOUT_FG);
    fb_text(x, y + 72, "Input:    PS/2 keyboard + mouse (IRQ)", C_ABOUT_FG);
    fb_text(x, y + 96, "Boot:     BIOS -> protected mode -> GUI", C_ABOUT_FG);
    fb_text(x, y + 132, "Milestone 2: the desktop release.", C_TERM_PROMPT);
}

/* ---------------- taskbar + start menu ---------------- */
static int start_btn_x(void) { return 6; }
static int menu_item_rect(int i, int *x, int *y, int *w, int *h) {
    *x = 8;
    *w = 236;
    *h = 34;
    *y = fb_height() - TASKBAR_H - 8 - 4 * 36 + i * 36;
    return 1;
}
static const char *menu_items[4] = {
    "Terminal", "About VoidOS", "Boot log", "Reboot (not wired up)",
};

static void compose(void) {
    int W = fb_width(), H = fb_height();
    fb_vgradient(0, H - TASKBAR_H, C_WALL_TOP, C_WALL_BOT);
    fb_fill(W / 2 - 60, H / 5, W / 2, 190, C_GLOW1);         /* soft panels */
    fb_fill(W / 2 + 30, H / 5 + 40, W / 3, 130, C_GLOW2);

    for (int i = 0; i < WIN_COUNT; i++) {                    /* bottom..top */
        win_t *w = &wins[zorder[i]];
        if (!w->visible) continue;
        int focused = (win_focused() == zorder[i]);
        draw_window_frame(w, focused);
        if (zorder[i] == WIN_TERM) draw_term(w, focused);
        else draw_about(w);
    }

    int tb = H - TASKBAR_H;                                  /* taskbar */
    fb_fill(0, tb, W, TASKBAR_H, C_TASKBAR);
    fb_fill(0, tb, W, 1, C_TASKBAR_LINE);
    fb_fill(start_btn_x(), tb + 4, 100, 36, menu_open ? C_TITLE_ON : C_BTN);
    fb_outline(start_btn_x(), tb + 4, 100, 36, C_BTN_LINE);
    draw_logo(start_btn_x() + 10, tb + 11, 8);
    fb_text(start_btn_x() + 38, tb + 12, "Start", C_WHITE);

    int cx = W / 2 - 40;                                     /* pinned apps */
    fb_fill(cx, tb + 4, 36, 36, C_BTN);
    fb_outline(cx, tb + 4, 36, 36, C_BTN_LINE);
    fb_text(cx + 3, tb + 12, ">_", C_TERM_PROMPT);
    fb_fill(cx + 44, tb + 4, 36, 36, C_BTN);
    fb_outline(cx + 44, tb + 4, 36, 36, C_BTN_LINE);
    fb_text(cx + 57, tb + 12, "i", C_WHITE);

    char clock[40];                                          /* clock */
    char t[9];
    rtc_time_str(t);
    int n = 0;
    for (const char *p = t; *p; p++) clock[n++] = *p;
    clock[n++] = ' ';
    clock[n++] = ' ';
    char up[12];
    fmt_u32(up, timer_ticks() / 100);
    for (const char *p = "up "; *p; p++) clock[n++] = *p;
    for (const char *p = up; *p; p++) clock[n++] = *p;
    clock[n++] = 's';
    clock[n] = 0;
    fb_text(W - fb_text_width(clock) - 12, tb + 12, clock, C_ABOUT_FG);

    if (menu_open) {                                         /* start menu */
        int mx, my, mw, mh;
        menu_item_rect(0, &mx, &my, &mw, &mh);
        int top = my - 6;
        fb_fill(mx - 2, top, mw + 4, 4 * 36 + 12, C_MENU);
        fb_outline(mx - 2, top, mw + 4, 4 * 36 + 12, C_BTN_LINE);
        for (int i = 0; i < 4; i++) {
            menu_item_rect(i, &mx, &my, &mw, &mh);
            if (i == 3) fb_text(mx + 10, my + 7, menu_items[i], C_TERM_DIM);
            else fb_text(mx + 10, my + 7, menu_items[i], C_WHITE);
        }
    }
}

static void present(void) {
    fb_cursor_hide();
    compose();
    fb_flush();
    fb_cursor_show(mouse_x, mouse_y);
}

/* ---------------- event loop ---------------- */
void gui_run(const char *cpu_vendor) {
    g_vendor = cpu_vendor;
    int W = fb_width(), H = fb_height();

    C_WALL_TOP = fb_rgb(18, 52, 110);
    C_WALL_BOT = fb_rgb(4, 30, 48);
    C_GLOW1 = fb_rgb(24, 72, 138);
    C_GLOW2 = fb_rgb(33, 96, 168);
    C_TASKBAR = fb_rgb(20, 21, 26);
    C_TASKBAR_LINE = fb_rgb(70, 80, 100);
    C_BTN = fb_rgb(35, 38, 46);
    C_BTN_LINE = fb_rgb(70, 76, 92);
    C_TITLE_ON = fb_rgb(37, 100, 207);
    C_TITLE_OFF = fb_rgb(58, 58, 62);
    C_CLOSE = fb_rgb(165, 42, 31);
    C_WIN_EDGE = fb_rgb(8, 8, 10);
    C_TERM_BG = fb_rgb(10, 13, 20);
    C_TERM_FG = fb_rgb(204, 214, 226);
    C_TERM_DIM = fb_rgb(120, 128, 140);
    C_TERM_PROMPT = fb_rgb(64, 200, 160);
    C_ABOUT_BG = fb_rgb(30, 30, 36);
    C_ABOUT_FG = fb_rgb(190, 198, 210);
    C_SHADOW = fb_rgb(3, 14, 24);
    C_WHITE = fb_rgb(255, 255, 255);
    C_MENU = fb_rgb(28, 30, 38);
    C_LOGO[0] = fb_rgb(242, 80, 34);
    C_LOGO[1] = fb_rgb(127, 186, 0);
    C_LOGO[2] = fb_rgb(0, 164, 239);
    C_LOGO[3] = fb_rgb(255, 185, 0);

    wins[WIN_TERM] = (win_t){40, 46, 700, 440, 1, "Terminal - void shell"};
    wins[WIN_ABOUT] = (win_t){0, 0, 410, 300, 1, "About VoidOS"};
    wins[WIN_ABOUT].x = W - 410 - 30 > 60 ? W - 410 - 30 : 60;
    wins[WIN_ABOUT].y = 120;
    if (wins[WIN_TERM].x + wins[WIN_TERM].w > W) wins[WIN_TERM].w = W - 60;
    if (wins[WIN_TERM].y + wins[WIN_TERM].h > H - TASKBAR_H)
        wins[WIN_TERM].h = H - TASKBAR_H - 60;
    tcols = (wins[WIN_TERM].w - 20) / FONT_W;
    if (tcols > TMAXC) tcols = TMAXC;
    trows = (wins[WIN_TERM].h - TITLE_H - 16) / FONT_H;
    if (trows > TMAXR) trows = TMAXR;
    term_init_text();

    int prev_mx = mouse_x, prev_my = mouse_y;
    uint8_t prev_btns = 0;
    int drag_win = -1, drag_dx = 0, drag_dy = 0;
    uint32_t last_half = timer_ticks() / 50;
    present();

    for (;;) {
        __asm__ volatile("hlt");
        int dirty = 0;
        int mx = mouse_x, my = mouse_y;
        uint8_t btns = mouse_buttons;

        if (mx != prev_mx || my != prev_my) {                /* mouse move */
            if (drag_win >= 0) {
                win_t *w = &wins[drag_win];
                w->x = mx - drag_dx;
                w->y = my - drag_dy;
                if (w->x < 0) w->x = 0;
                if (w->y < 0) w->y = 0;
                if (w->x + w->w > W) w->x = W - w->w;
                if (w->y + w->h > H - TASKBAR_H) w->y = H - TASKBAR_H - w->h;
                dirty = 1;
            } else {
                fb_cursor_show(mx, my);                      /* cursor only */
            }
            prev_mx = mx;
            prev_my = my;
        }

        if ((btns & 1) && !(prev_btns & 1)) {                /* left press */
            int handled = 0;
            if (menu_open) {
                for (int i = 0; i < 4 && !handled; i++) {
                    int rx, ry, rw, rh;
                    menu_item_rect(i, &rx, &ry, &rw, &rh);
                    if (in_rect(mx, my, rx, ry, rw, rh)) {
                        handled = 1;
                        menu_open = 0;
                        if (i == 0) { wins[WIN_TERM].visible = 1; win_raise(WIN_TERM); }
                        if (i == 1) { wins[WIN_ABOUT].visible = 1; win_raise(WIN_ABOUT); }
                        if (i == 2) {
                            wins[WIN_TERM].visible = 1;
                            win_raise(WIN_TERM);
                            if (win_focused() == WIN_TERM) {
                                term_puts("\n[menu] boot log requested\n");
                                shell_exec_bootlog();
                            }
                        }
                        dirty = 1;
                    }
                }
            }
            if (!handled && in_rect(mx, my, start_btn_x(), H - TASKBAR_H + 4, 100, 36)) {
                menu_open = !menu_open;
                handled = 1;
                dirty = 1;
            }
            if (!handled && my >= H - TASKBAR_H) {           /* pinned icons */
                int cx = W / 2 - 40;
                if (in_rect(mx, my, cx, H - TASKBAR_H + 4, 36, 36)) {
                    wins[WIN_TERM].visible = 1; win_raise(WIN_TERM); dirty = 1;
                } else if (in_rect(mx, my, cx + 44, H - TASKBAR_H + 4, 36, 36)) {
                    wins[WIN_ABOUT].visible = 1; win_raise(WIN_ABOUT); dirty = 1;
                }
                handled = 1;
            }
            if (!handled) {                                  /* windows */
                for (int i = WIN_COUNT - 1; i >= 0; i--) {
                    win_t *w = &wins[zorder[i]];
                    if (!w->visible) continue;
                    if (!in_rect(mx, my, w->x, w->y, w->w, w->h)) continue;
                    int id = zorder[i];
                    if (in_rect(mx, my, w->x + w->w - 40, w->y + 4, 34, 22)) {
                        w->visible = 0;                      /* close box */
                    } else {
                        win_raise(id);
                        if (my < w->y + TITLE_H) {           /* title: drag */
                            drag_win = id;
                            drag_dx = mx - w->x;
                            drag_dy = my - w->y;
                        }
                    }
                    handled = 1;
                    dirty = 1;
                    break;
                }
            }
            if (!handled && menu_open) {                     /* desktop click */
                menu_open = 0;
                dirty = 1;
            }
        }
        if (!(btns & 1) && (prev_btns & 1))                  /* left release */
            drag_win = -1;
        prev_btns = btns;

        int c;                                               /* keyboard */
        while ((c = keyboard_getchar()) >= 0) {
            if (c == 27) {
                menu_open = !menu_open;
                dirty = 1;
            } else if (wins[WIN_TERM].visible) {
                win_raise(WIN_TERM);
                shell_key((char)c);
                dirty = 1;
            }
        }

        uint32_t half = timer_ticks() / 50;                  /* blink + clock */
        if (half != last_half) {
            last_half = half;
            dirty = 1;
        }

        if (dirty)
            present();
    }
}

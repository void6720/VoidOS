#include "keyboard.h"
#include "interrupts.h"
#include "io.h"

static volatile char kb_buf[256];
static volatile uint8_t kb_head, kb_tail;
static int shift_down;

/* Scancode set 1 -> ASCII, indexed by make code (0x00..0x39 used). */
static const char sc_normal[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ',
};
static const char sc_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ',
};

static void kb_push(char c) {
    uint8_t next = kb_head + 1;
    if (next != kb_tail) {
        kb_buf[kb_head] = c;
        kb_head = next;
    }
}

static void keyboard_handler(void) {
    uint8_t sc = port_byte_in(0x60);
    if (sc == 0x2A || sc == 0x36) { shift_down = 1; return; }        /* shift make */
    if (sc == 0xAA || sc == 0xB6) { shift_down = 0; return; }        /* shift break */
    if (sc & 0x80) return;                                          /* other breaks */
    char c = shift_down ? sc_shift[sc] : sc_normal[sc];
    if (c)
        kb_push(c);
}

void keyboard_init(void) { irq_install(1, keyboard_handler); }

int keyboard_getchar(void) {
    if (kb_tail == kb_head)
        return -1;
    char c = kb_buf[kb_tail];
    kb_tail = kb_tail + 1;
    return c;
}

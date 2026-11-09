#include "mouse.h"
#include "interrupts.h"
#include "io.h"

volatile int mouse_x, mouse_y;
volatile uint8_t mouse_buttons;
static int max_x = 1023, max_y = 767;
static uint8_t pkt[3];
static int pkt_phase;

void mouse_set_bounds(int width, int height) {
    max_x = width - 1;
    max_y = height - 1;
}
void mouse_set_pos(int x, int y) { mouse_x = x; mouse_y = y; }

static void mouse_handler(void) {
    uint8_t b = port_byte_in(0x60);
    if (pkt_phase == 0) {
        if (!(b & 0x08)) return; /* out of sync: bit 3 is always set in byte 0 */
        pkt[0] = b;
        pkt_phase = 1;
    } else if (pkt_phase == 1) {
        pkt[1] = b;
        pkt_phase = 2;
    } else {
        pkt[2] = b;
        pkt_phase = 0;
        mouse_buttons = pkt[0] & 0x07;
        if (!(pkt[0] & 0xC0)) { /* no X/Y overflow */
            int dx = pkt[1] - ((pkt[0] & 0x10) ? 256 : 0);
            int dy = pkt[2] - ((pkt[0] & 0x20) ? 256 : 0);
            mouse_x += dx;
            mouse_y -= dy; /* PS/2 Y is up-positive, screen Y is down */
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > max_x) mouse_x = max_x;
            if (mouse_y > max_y) mouse_y = max_y;
        }
    }
}

static void ps2_wait_write(void) { while (port_byte_in(0x64) & 0x02); }
static void ps2_wait_read(void) { while (!(port_byte_in(0x64) & 0x01)); }
static void mouse_write(uint8_t data) {
    ps2_wait_write();
    port_byte_out(0x64, 0xD4); /* next byte goes to the mouse */
    ps2_wait_write();
    port_byte_out(0x60, data);
}
static uint8_t mouse_read(void) {
    ps2_wait_read();
    return port_byte_in(0x60);
}

void mouse_init(void) {
    ps2_wait_write();
    port_byte_out(0x64, 0xA8); /* enable the auxiliary (mouse) device */
    ps2_wait_write();
    port_byte_out(0x64, 0x20); /* read controller config byte */
    uint8_t config = mouse_read();
    config |= 0x02;            /* enable IRQ12 */
    config &= ~0x20;           /* enable mouse clock */
    ps2_wait_write();
    port_byte_out(0x64, 0x60); /* write controller config byte */
    ps2_wait_write();
    port_byte_out(0x60, config);
    mouse_write(0xF6);         /* set defaults */
    mouse_read();              /* ACK */
    mouse_write(0xF4);         /* enable data reporting */
    mouse_read();              /* ACK */
    irq_install(12, mouse_handler);
}

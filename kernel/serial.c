#include "serial.h"
#include "io.h"

#define COM1 0x3F8

void serial_init(void) {
    port_byte_out(COM1 + 1, 0x00);  /* disable interrupts */
    port_byte_out(COM1 + 3, 0x80);  /* DLAB on: set baud rate divisor */
    port_byte_out(COM1 + 0, 0x03);  /* divisor 3 = 38400 baud (low byte) */
    port_byte_out(COM1 + 1, 0x00);  /* divisor high byte */
    port_byte_out(COM1 + 3, 0x03);  /* 8 bits, no parity, 1 stop bit; DLAB off */
    port_byte_out(COM1 + 2, 0xC7);  /* FIFO on, clear, 14-byte threshold */
    port_byte_out(COM1 + 4, 0x03);  /* DTR + RTS */
}

static int serial_ready(void) {
    return port_byte_in(COM1 + 5) & 0x20;  /* transmit holding register empty */
}

void serial_putc(char c) {
    while (!serial_ready()) { }
    port_byte_out(COM1, (uint8_t)c);
}

void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

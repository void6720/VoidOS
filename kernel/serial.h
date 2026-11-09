#pragma once

/* COM1 serial port (0x3F8), 38400 baud 8N1.
   Serial output is mirrored to QEMU's console with: -serial stdio */
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);

#pragma once
#include <stdint.h>

/* x86 port I/O helpers (inline assembly). */
static inline uint8_t port_byte_in(uint16_t port) {
    uint8_t result;
    __asm__ volatile("in %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}

static inline void port_byte_out(uint16_t port, uint8_t data) {
    __asm__ volatile("out %%al, %%dx" : : "a"(data), "d"(port));
}

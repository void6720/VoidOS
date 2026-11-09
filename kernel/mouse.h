#pragma once
#include <stdint.h>

/* PS/2 mouse (IRQ12). Position is absolute, clamped to the bounds set with
   mouse_set_bounds(); button state bit0 = left, bit1 = right, bit2 = middle. */
extern volatile int mouse_x, mouse_y;
extern volatile uint8_t mouse_buttons;

void mouse_set_bounds(int width, int height);
void mouse_set_pos(int x, int y);
void mouse_init(void);

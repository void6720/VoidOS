#pragma once

/* PS/2 keyboard (IRQ1), scancode set 1 -> ASCII ring buffer.
   Special keys are delivered as control codes: '\n' enter, '\b' backspace,
   '\t' tab, 27 escape. */
void keyboard_init(void);
int keyboard_getchar(void); /* next char, or -1 when the buffer is empty */

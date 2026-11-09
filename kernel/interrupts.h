#pragma once
#include <stdint.h>

/* IDT + dual PIC (remapped to vectors 0x20/0x28) + PIT timer. */
typedef void (*irq_handler_t)(void);

void idt_init(void);                       /* build IDT, remap PIC, lidt */
void irq_install(int irq, irq_handler_t handler);
void irq_dispatch(uint32_t irq);           /* called from isr.asm stubs */

void timer_init(uint32_t hz);              /* PIT channel 0 -> IRQ0 */
uint32_t timer_ticks(void);                /* ticks since timer_init */

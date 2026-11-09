#include "interrupts.h"
#include "io.h"

/* ---------------- IDT ---------------- */
typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static irq_handler_t handlers[16];
static volatile uint32_t ticks;

/* The kernel's own GDT (same flat layout as the bootloader's: code 0x08,
   data 0x10). Required: the boot trampoline's copy-down overwrites the
   bootloader's GDT, and the CPU re-reads it on the first interrupt. */
static const uint64_t gdt[3] = {
    0x0000000000000000ULL, /* null */
    0x00CF9A000000FFFFULL, /* code: base 0, limit 4 GiB, ring 0 */
    0x00CF92000000FFFFULL, /* data: base 0, limit 4 GiB, ring 0 */
};
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;
static const gdt_ptr_t gdt_ptr = {sizeof(gdt) - 1, (uint32_t)gdt};
extern void gdt_load(const gdt_ptr_t *ptr);

extern void irq0_stub(void);
extern void irq1_stub(void);
extern void irq12_stub(void);

static void idt_set_gate(int n, uint32_t handler) {
    idt[n].base_low = handler & 0xFFFF;
    idt[n].selector = 0x08; /* kernel code segment */
    idt[n].zero = 0;
    idt[n].flags = 0x8E;    /* present, ring 0, 32-bit interrupt gate */
    idt[n].base_high = (handler >> 16) & 0xFFFF;
}

/* ---------------- PIC ---------------- */
static void pic_remap(void) {
    uint8_t mask1 = port_byte_in(0x21);
    uint8_t mask2 = port_byte_in(0xA1);
    port_byte_out(0x20, 0x11); /* initialise, ICW4 follows */
    port_byte_out(0xA0, 0x11);
    port_byte_out(0x21, 0x20); /* master vector offset 0x20 */
    port_byte_out(0xA1, 0x28); /* slave vector offset 0x28 */
    port_byte_out(0x21, 0x04); /* slave on IRQ2 */
    port_byte_out(0xA1, 0x02);
    port_byte_out(0x21, 0x01); /* 8086 mode */
    port_byte_out(0xA1, 0x01);
    port_byte_out(0x21, mask1);
    port_byte_out(0xA1, mask2);
    /* Unmask IRQ0 (timer), IRQ1 (keyboard), IRQ2 (cascade), IRQ12 (mouse). */
    port_byte_out(0x21, 0xF8);
    port_byte_out(0xA1, 0xEF);
}

void idt_init(void) {
    gdt_load(&gdt_ptr);
    for (int i = 0; i < 256; i++)
        idt_set_gate(i, 0);
    idt_set_gate(0x20, (uint32_t)irq0_stub);
    idt_set_gate(0x21, (uint32_t)irq1_stub);
    idt_set_gate(0x2C, (uint32_t)irq12_stub);
    pic_remap();
    idt_ptr_t ptr = {sizeof(idt) - 1, (uint32_t)idt};
    __asm__ volatile("lidt %0" : : "m"(ptr));
}

void irq_install(int irq, irq_handler_t handler) {
    if (irq >= 0 && irq < 16)
        handlers[irq] = handler;
}

void irq_dispatch(uint32_t irq) {
    if (irq < 16 && handlers[irq])
        handlers[irq]();
    if (irq >= 8)
        port_byte_out(0xA0, 0x20); /* EOI to slave */
    port_byte_out(0x20, 0x20);     /* EOI to master */
}

/* ---------------- PIT timer (IRQ0) ---------------- */
static void timer_handler(void) { ticks++; }

void timer_init(uint32_t hz) {
    uint32_t divisor = 1193180 / hz;
    port_byte_out(0x43, 0x36); /* channel 0, lobyte/hibyte, square wave */
    port_byte_out(0x40, divisor & 0xFF);
    port_byte_out(0x40, (divisor >> 8) & 0xFF);
    irq_install(0, timer_handler);
}

uint32_t timer_ticks(void) { return ticks; }

#include <stdint.h>
#include "vga.h"
#include "serial.h"
#include "interrupts.h"
#include "keyboard.h"
#include "mouse.h"
#include "fb.h"
#include "gui.h"
#include "bootinfo.h"

#define VOIDOSS_VERSION "0.1.0"

/* Kernel console: everything is written to both the VGA screen and COM1,
   so boot output is visible in a GUI window and in headless QEMU logs. */
static void klog(const char *s) {
    vga_puts(s);
    serial_puts(s);
}

static int append_u32(char *out, uint32_t v) {
    char tmp[12];
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    return n;
}

static void cpu_vendor(char out[13]) {
    uint32_t ebx, ecx, edx;
    __asm__ volatile("cpuid"
                     : "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0));
    /* Vendor string order is EBX, EDX, ECX. */
    ((uint32_t *)out)[0] = ebx;
    ((uint32_t *)out)[1] = edx;
    ((uint32_t *)out)[2] = ecx;
    out[12] = '\0';
}

void kmain(void) {
    serial_init();
    vga_init();

    vga_set_attr(VGA_WHITE_ON_BLUE);
    vga_puts("  VoidOS " VOIDOSS_VERSION " - kernel booted                                \n");
    serial_puts("VoidOS " VOIDOSS_VERSION " - kernel booted\n");
    vga_set_attr(VGA_LIGHT_GREY);
    vga_putc('\n');

    char vendor[13];
    cpu_vendor(vendor);

    klog("[ ok ] Bootloader handoff complete (boot/boot.asm)\n");
    klog("[ ok ] 32-bit protected mode, flat GDT active\n");
    klog("[ ok ] VGA text console @ 0xB8000\n");
    klog("[ ok ] Serial COM1 @ 0x3F8, 38400 8N1\n");
    klog("[ ok ] CPU vendor: ");
    klog(vendor);
    klog("\n");
    idt_init();
    timer_init(100);
    keyboard_init();
    klog("[ ok ] IDT + PIC: IRQ0 timer, IRQ1 keyboard, IRQ12 mouse\n");

    boot_info_t *bi = (boot_info_t *)BOOTINFO_ADDR;
    if (fb_init(bi)) {
        char res[24];
        int n = 0;
        n += append_u32(res + n, bi->width);
        res[n++] = 'x';
        n += append_u32(res + n, bi->height);
        res[n] = 0;
        klog("[ ok ] VBE framebuffer ");
        klog(res);
        klog(" @ 32bpp\n");
        mouse_set_bounds((int)bi->width, (int)bi->height);
        mouse_set_pos((int)bi->width / 2, (int)bi->height / 2);
        mouse_init();
        __asm__ volatile("sti");
        klog("\nMilestone 2 reached: desktop GUI (framebuffer + mouse + keyboard).\n");
        gui_run(vendor); /* never returns */
    }

    klog("\nMilestone 2: no VBE framebuffer found - staying in text mode.\n");
    klog("kmain: halting CPU.\n");
    for (;;)
        __asm__ volatile("hlt");
}

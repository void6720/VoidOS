#pragma once
#include <stdint.h>

/* BootInfo block written by boot/boot.asm at physical 0x0500 before the
   protected-mode switch. Trusted only when magic matches and fb_phys != 0. */
#define BOOTINFO_ADDR 0x0500
#define BOOTINFO_MAGIC 0x42534F56u /* "VOSB" */

typedef struct {
    uint32_t magic;      /* BOOTINFO_MAGIC when the VBE setup succeeded */
    uint32_t fb_phys;    /* linear framebuffer physical address */
    uint32_t pitch;      /* bytes per scanline */
    uint32_t width;
    uint32_t height;
    uint32_t bpp;        /* 32 */
    uint8_t red_pos;     /* bit position of the red channel */
    uint8_t green_pos;
    uint8_t blue_pos;
    uint8_t reserved;
    uint32_t vbe_mode;
} boot_info_t;

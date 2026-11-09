/* VoidOS kernel linker script.
   The kernel is linked at 0x1000, matching KERNEL_OFFSET in boot/boot.asm
   and the load address the boot sector reads the kernel image to. */
ENTRY(_start)

SECTIONS {
    . = 0x1000;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }

    /* Framebuffer backbuffer (3 MiB): above the 0xA0000-0xFFFFF hardware
       hole. NOLOAD so objcopy keeps it out of the kernel binary image. */
    .fbbss 0x200000 (NOLOAD) : {
        *(.fbbss)
    }
}

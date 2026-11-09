; Kernel entry stub - the first kernel code the bootloader jumps to.
; Loaded at 0x1000 (see kernel/linker.ld). Clears BSS, then calls kmain.
;
; Build: nasm -f elf32 kernel/kernel_entry.asm -o build/kernel_entry.o

[bits 32]
[extern kmain]
[extern __bss_start]
[extern __bss_end]
global _start

_start:
    mov esp, 0x800000           ; stack at 8 MiB (matches boot/boot.asm)
    mov edi, __bss_start        ; zero the BSS (framebuffer backbuffer,
    mov ecx, __bss_end          ; driver state, terminal grid...)
    sub ecx, edi
    xor eax, eax
    rep stosb
    call kmain                  ; hand control to the C kernel
    cli
.hang:                          ; kmain should never return; halt forever
    hlt
    jmp .hang

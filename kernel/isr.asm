; IRQ entry stubs. Each pushes its IRQ number and falls into a common
; path that saves registers, calls irq_dispatch(irq) in C, restores and
; returns with iret. Vectors: IRQ0 -> 0x20, IRQ1 -> 0x21, IRQ12 -> 0x2C.
;
; Build: nasm -f elf32 kernel/isr.asm -o build/isr.o

[bits 32]
[extern irq_dispatch]
global irq0_stub, irq1_stub, irq12_stub
global gdt_load

; void gdt_load(gdt_ptr_t *) - load a fresh GDT and reload all segment
; registers (the bootloader's GDT is destroyed by the kernel copy-down,
; so the kernel must own its table before the first interrupt arrives).
gdt_load:
    mov eax, [esp + 4]
    lgdt [eax]
    jmp 0x08:.reload
.reload:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

irq0_stub:
    push 0
    jmp irq_common
irq1_stub:
    push 1
    jmp irq_common
irq12_stub:
    push 12
    jmp irq_common

irq_common:
    pusha                       ; edi esi ebp esp ebx edx ecx eax
    mov eax, [esp + 32]         ; the IRQ number pushed by the stub
    push eax
    call irq_dispatch
    add esp, 4
    popa
    add esp, 4                  ; drop the IRQ number
    iret

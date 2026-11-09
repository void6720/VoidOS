; VoidOS boot sector - BIOS, 16-bit real mode
; Loads the kernel to 0x1000, enables A20, picks a 32bpp VESA/VBE graphics
; mode (framebuffer details go to the BootInfo block at 0x0500), switches
; to 32-bit protected mode and jumps to the kernel.
;
; Build: nasm -f bin boot/boot.asm -o build/boot.bin (must fit 510 bytes!)
; No BIOS text output: the mode set would erase it anyway - the kernel
; reports everything on its serial + framebuffer consoles.

[org 0x7c00]
KERNEL_OFFSET equ 0x1000
BOOTINFO      equ 0x0500        ; struct handed to the kernel (see bootinfo.h)
VBE_INFO      equ 0x0600        ; mode-info scratch
VBE_CTRL      equ 0x0800        ; controller-info scratch
KERNEL_SECTORS equ 160          ; 80 KiB kernel budget

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov [BOOT_DRIVE], dl        ; BIOS passes the boot drive number in DL
    mov bp, 0x9000              ; real-mode stack
    mov sp, bp

    in al, 0x92                 ; enable A20 (fast gate) so the kernel can
    or al, 2                    ; reach the framebuffer above 1 MiB
    out 0x92, al

    call load_kernel
    call setup_vbe

    cli                         ; 1. disable interrupts
    lgdt [gdt_descriptor]       ; 2. load the GDT
    mov eax, cr0                ; 3. set the Protection Enable bit
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:init_pm        ; 4. far jump: flush pipeline, load CS

; --- Load KERNEL_SECTORS sectors, one at a time ---------------------------
; All loop state lives in memory: BIOS int 13h is not trusted to preserve
; the registers a register-resident loop would depend on.
load_kernel:
.lk_loop:
    mov es, [lk_es]
    mov bx, [lk_bx]
    mov ah, 0x02                ; BIOS read, 1 sector
    mov al, 1
    mov ch, [lk_cyl]
    mov cl, [lk_sec]
    mov dh, [lk_head]
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc $                        ; disk error: hang
    add word [lk_bx], 512       ; advance destination; on 64 KiB wrap
    jnc .lk_chs                 ; bump the segment instead
    mov ax, [lk_es]
    add ax, 0x1000
    mov [lk_es], ax
.lk_chs:
    inc byte [lk_sec]           ; 18 sectors/track, 2 heads (1.44 MB floppy)
    cmp byte [lk_sec], 19
    jb .lk_next
    mov byte [lk_sec], 1
    inc byte [lk_head]
    cmp byte [lk_head], 2
    jb .lk_next
    mov byte [lk_head], 0
    inc byte [lk_cyl]
.lk_next:
    dec word [lk_left]
    jnz .lk_loop
    xor ax, ax
    mov es, ax
    ret

; --- VESA/VBE: find the best 32bpp linear mode, record it, set it --------
; BootInfo at 0x0500 (zeroed here; the kernel trusts it only if magic set):
;   +0  u32 magic 0x42534F56   +4  u32 framebuffer physical address
;   +8  u32 pitch (bytes)      +12 u32 width    +16 u32 height
;   +20 u32 bpp                +24 u8 red pos   +25 u8 green pos  +26 u8 blue pos
;   +28 u32 VBE mode number
setup_vbe:
    mov di, BOOTINFO            ; zero the BootInfo block (16 words)
    mov cx, 16
    xor ax, ax
    rep stosw

    mov di, VBE_CTRL            ; get controller info
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne .no_vbe
    mov si, VBE_CTRL + 14       ; mode list far pointer -> ml_off/ml_seg
    mov di, ml_off
    movsw
    movsw
.ml_loop:
    mov es, [ml_seg]            ; read next mode number from the list
    mov bx, [ml_off]
    mov cx, es:[bx]
    cmp cx, 0xFFFF
    je .ml_done
    add word [ml_off], 2
    mov [cur_mode], cx
    xor ax, ax                  ; get this mode's info block
    mov es, ax
    mov di, VBE_INFO
    mov ax, 0x4F01
    int 0x10
    cmp ax, 0x004F
    jne .ml_loop
    mov al, [VBE_INFO]          ; attributes: bit0 supported + bit7 LFB
    and al, 0x81
    cmp al, 0x81
    jne .ml_loop
    cmp byte [VBE_INFO + 0x19], 32      ; want 32 bpp
    jne .ml_loop
    cmp byte [VBE_INFO + 0x1B], 6       ; direct-color model
    jne .ml_loop
    mov ax, [VBE_INFO + 0x12]           ; width
    cmp ax, 1024
    ja .ml_loop                         ; too wide: skip
    mov dx, [VBE_INFO + 0x14]           ; height
    cmp dx, 768
    ja .ml_loop                         ; too tall: skip
    mov si, 40                          ; any other size <= 1024x768
    cmp ax, 1024
    jne .scored
    cmp dx, 768
    jne .scored
    mov si, 100                         ; 1024x768: perfect
.scored:
    cmp si, [best_score]
    jbe .ml_loop
    mov [best_score], si                ; record this mode in BootInfo
    mov ax, [cur_mode]
    mov [BOOTINFO + 28], ax
    mov si, VBE_INFO + 0x28             ; PhysBasePtr (2 words)
    mov di, BOOTINFO + 4
    movsw
    movsw
    mov ax, [VBE_INFO + 0x12]           ; width (BootInfo fields are
    mov [BOOTINFO + 12], ax             ; u32: store the words into the
    mov ax, [VBE_INFO + 0x14]           ; low halves; block was zeroed)
    mov [BOOTINFO + 16], ax
    mov ax, [VBE_INFO + 0x32]           ; LinBytesPerScanLine (kernel
    mov [BOOTINFO + 8], ax              ; falls back to width*4 if 0)
    mov byte [BOOTINFO + 20], 32
    mov al, [VBE_INFO + 0x20]           ; red field position
    mov [BOOTINFO + 24], al
    mov al, [VBE_INFO + 0x22]           ; green field position
    mov [BOOTINFO + 25], al
    mov al, [VBE_INFO + 0x24]           ; blue field position
    mov [BOOTINFO + 26], al
    jmp .ml_loop
.ml_done:
    xor ax, ax
    mov es, ax
    cmp word [best_score], 0
    je .no_vbe
    mov bx, [BOOTINFO + 28]             ; set the winning mode (LFB on)
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .no_vbe                 ; mode set failed: magic stays 0, and the
    mov dword [BOOTINFO], 0x42534F56    ; kernel ignores BootInfo entirely
    ret
.no_vbe:
    ret

[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x800000           ; 32-bit stack at 8 MiB (clear of the kernel
                                ; image + 3 MiB framebuffer backbuffer)
    mov esi, trampoline         ; copy the copy-down trampoline out of the
    mov edi, 0x70000            ; danger zone first: the kernel copy sweeps
    mov ecx, (trampoline_end - trampoline + 3) / 4 ; through 0x1000..0x15000,
    rep movsd                   ; which includes this very boot sector, so
    mov eax, 0x70000            ; nothing may execute from low memory while
    jmp eax                     ; it runs

; Position-independent trampoline (only absolute constants): copies the
; kernel from its load address 0x10000 down to its link address 0x1000,
; then jumps to it. Executed from 0x70000, clear of the copy's sweep.
trampoline:
    mov esi, 0x10000
    mov edi, KERNEL_OFFSET
    mov ecx, KERNEL_SECTORS * 128
    rep movsd
    mov eax, KERNEL_OFFSET
    jmp eax
trampoline_end:

; --- Global Descriptor Table: flat 4 GiB code + data segments -------------
gdt_start:
gdt_null:
    dd 0x0
    dd 0x0
gdt_code:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0
gdt_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

BOOT_DRIVE: db 0
lk_left: dw KERNEL_SECTORS
lk_es:   dw 0x1000              ; load destination 0x10000: safely above the
lk_bx:   dw 0                   ; boot sector at 0x7C00 (a load to 0x1000
                                ; would overwrite the running boot code)
lk_cyl:  db 0
lk_head: db 0
lk_sec:  db 2
ml_off:  dw 0
ml_seg:  dw 0
cur_mode: dw 0
best_score: dw 0

times 510 - ($ - $$) db 0
dw 0xaa55

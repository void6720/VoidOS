# VoidOS

A small operating system built from scratch: a hand-written BIOS bootloader,
a freestanding C kernel, and an FHS-style root filesystem tree, booted under
QEMU (with KVM acceleration where available).

## VoidOS Linux — XFCE edition (`distro/`)

The sibling Linux distro: Alpine base + Linux LTS kernel + the **real
XFCE desktop** (Xorg + LightDM autologin as user `void`), built because
real XFCE needs a Linux userland the scratch kernel doesn't have.
Boot it with `distro/run-linux.sh`; details in `distro/README.md`.

**Version:** 0.1.0 — milestone 2: the desktop release.

## What boots today

1. BIOS loads `boot/boot.asm` (sector 1) at `0x7C00`. The bootloader enables
   A20, loads 160 sectors of kernel to `0x10000` via `int 13h` (one at a
   time, state in memory), enumerates VESA/VBE modes and picks the best
   32bpp linear mode (1024×768 preferred), writing a BootInfo block to
   `0x0500`.
2. It switches to 32-bit protected mode; a position-independent trampoline
   at `0x70000` copies the kernel down to its link address `0x1000` and
   jumps in. (The copy can't run from the boot sector: its sweep through
   `0x1000..0x15000` includes the boot sector itself.)
3. The kernel sets up its own GDT (the copy destroys the bootloader's),
   an IDT + remapped PIC, a 100 Hz PIT timer, and PS/2 keyboard + mouse
   drivers — then enters the **desktop**: a framebuffer GUI with a
   wallpaper, draggable windows, a taskbar with Start menu and clock,
   and a terminal window running a tiny shell (`help`, `ver`, `echo`,
   `ls`, `uptime`, `time`, `bootlog`, `clear`...). ESC toggles the menu.

Serial log (`make test`):

```
VoidOS 0.1.0 - kernel booted
[ ok ] Bootloader handoff complete (boot/boot.asm)
[ ok ] 32-bit protected mode, flat GDT active
[ ok ] VGA text console @ 0xB8000
[ ok ] Serial COM1 @ 0x3F8, 38400 8N1
[ ok ] CPU vendor: AuthenticAMD
[ ok ] IDT + PIC: IRQ0 timer, IRQ1 keyboard, IRQ12 mouse
[ ok ] VBE framebuffer 1024x768 @ 32bpp

Milestone 2 reached: desktop GUI (framebuffer + mouse + keyboard).
```

### Memory-map lessons (learned the hard way)

- The kernel loads at `0x10000`, never `0x1000`: an 80 KiB load to `0x1000`
  overwrites the running boot sector at `0x7C00`.
- The GDT must be kernel-owned: the copy-down trashes the bootloader's
  table, and the CPU re-reads it on the first interrupt.
- The 3 MiB framebuffer backbuffer lives at `0x200000` (linker section
  `.fbbss`, NOLOAD): BSS growing from `0x1000` would cross the
  `0xA0000..0xFFFFF` hardware hole (VGA aperture + ROM shadow), where
  "RAM" writes go to devices and reads return ROM bytes.

## Layout

```
boot/      NASM bootloader (sector loader, A20, VBE mode pick, PM switch,
           copy-down trampoline)
kernel/    Freestanding C kernel: entry stub, VGA + serial consoles,
           IDT/PIC/PIT, PS/2 keyboard + mouse, VBE framebuffer driver,
           desktop GUI (windows/taskbar/Start menu/terminal shell),
           minimal libc (memcpy/memset/strlen...)
rootfs/    The future `/` of the running system (FHS layout:
           bin sbin lib etc usr var proc sys dev run tmp home ...)
tools/     check-tools.sh, make_font.py (bitmap font generator)
Makefile   build / run / test / rootfs / clean
run.sh     QEMU launcher (KVM if /dev/kvm is available, TCG otherwise)
```

## Toolchain

| Tool | Used for |
|---|---|
| NASM | bootloader + kernel entry assembly |
| GCC (`-m32 -ffreestanding`) | kernel C code |
| GNU ld + objcopy | link at `0x1000`, produce flat `kernel.bin` |
| GNU make | build orchestration |
| QEMU (`qemu-system-x86_64`) | run / boot-test |
| KVM / pKVM | hardware acceleration (see below) |

Check your host with `./tools/check-tools.sh`.

## Build & run

```sh
make          # builds build/os-image.bin (1.44 MB bootable floppy image)
make run      # boots it in QEMU (GUI if $DISPLAY is set, serial console otherwise)
make test     # headless boot test: boots 6 s, checks the serial log for the banner
make rootfs   # lists the rootfs staging tree
make clean
```

## Languages

The kernel core is **assembly + C**: assembly where the hardware demands it
(boot sector, mode switch, entry stub, port I/O), C for everything else.
C# has no freestanding kernel runtime, so it is not used in the kernel;
C++ can be introduced later for kernel modules where it pays off.

## QEMU / KVM / pKVM

- **x86_64 (this target):** QEMU with KVM acceleration (`-enable-kvm -cpu host`)
  when `/dev/kvm` is available; plain TCG emulation otherwise. `run.sh`
  picks automatically.
- **pKVM** is the protected KVM hypervisor for **ARM64/Android** — it does not
  exist on x86. The tree keeps architecture code separate so an aarch64 port
  can target pKVM later without restructuring the kernel.

## Roadmap

1. ✅ Bootloader + protected-mode C kernel, VGA + serial consoles
2. ✅ IDT, PIC, timer/keyboard/mouse IRQs + framebuffer desktop GUI
3. Paging + a simple physical memory manager
4. Package `rootfs/` as an initramfs (cpio), mount it as `/`
5. First userspace: `/sbin/init` as PID 1, then a shell
6. Long mode (64-bit) port; aarch64 + pKVM target after that

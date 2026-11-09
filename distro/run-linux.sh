#!/bin/sh
# Boot VoidOS Linux (XFCE edition) in QEMU.
# Uses KVM automatically when /dev/kvm is available; TCG emulation
# otherwise (slower: give the desktop ~2-3 minutes to come up).
cd "$(dirname "$0")"
QEMU=qemu-system-x86_64
command -v "$QEMU" >/dev/null 2>&1 || QEMU="$HOME/workspace/tools/bin/qemu-system-x86_64"
ACCEL=""
[ -w /dev/kvm ] && ACCEL="-accel kvm"
if [ -n "$DISPLAY" ]; then DISP="-display gtk"; else DISP="-display none"; echo "headless: no \$DISPLAY, running with -display none"; fi
exec "$QEMU" $ACCEL \
  -m 2048 \
  -kernel rootfs/boot/vmlinuz-voidos \
  -append "root=/dev/vda rootfstype=ext4 rw console=tty0 quiet splash" \
  -drive file=build/voidos-linux.img,if=virtio,format=raw \
  -vga virtio \
  -netdev user,id=n0,hostfwd=tcp:127.0.0.1:2222-:22 -device virtio-net-pci,netdev=n0 \
  -audiodev none,id=snd0 -device intel-hda -device hda-duplex,audiodev=snd0 \
  $DISP "$@"

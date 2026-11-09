#!/usr/bin/env bash
# Boot VoidOS in QEMU.
#   - Uses KVM hardware acceleration when /dev/kvm is available
#     (falls back to QEMU's TCG emulation otherwise).
#   - With a display, opens the VGA window. Headless: mirrors the kernel
#     console over serial to your terminal instead.
#   - pKVM note: pKVM is the ARM64/Android protected hypervisor. This host
#     is x86_64, so QEMU/KVM is the acceleration path here; the tree keeps
#     arch code separate so an aarch64/pKVM target can be added later.
set -euo pipefail
cd "$(dirname "$0")"

IMG=build/os-image.bin
[ -f "$IMG" ] || make

ACCEL=()
if [ -w /dev/kvm ]; then
    ACCEL=(-enable-kvm -cpu host)
else
    echo "note: /dev/kvm not available - using QEMU emulation (TCG)"
fi

if [ -n "${DISPLAY:-}" ]; then
    exec qemu-system-x86_64 "${ACCEL[@]}" -fda "$IMG" -boot order=a "$@"
else
    exec qemu-system-x86_64 "${ACCEL[@]}" -fda "$IMG" -boot order=a \
        -display none -serial stdio "$@"
fi

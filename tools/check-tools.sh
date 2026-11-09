#!/usr/bin/env bash
# Check the VoidOS build toolchain.
set -u

check() {
    if command -v "$1" >/dev/null 2>&1; then
        printf 'ok       %-22s %s\n' "$1" "$($1 --version 2>/dev/null | head -1)"
    else
        printf 'MISSING  %-22s (install: %s)\n' "$1" "$2"
    fi
}

check nasm               "sudo apt install nasm"
check gcc                "sudo apt install gcc"
check ld                 "sudo apt install binutils"
check make               "sudo apt install make"
check qemu-system-x86_64 "sudo apt install qemu-system-x86"
check python3            "sudo apt install python3"

if [ -w /dev/kvm ]; then
    echo "ok       /dev/kvm (KVM acceleration available)"
else
    echo "note     /dev/kvm not available - QEMU will emulate (TCG); fine for dev"
fi

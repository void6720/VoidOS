# VoidOS Linux — XFCE edition

A real Linux distro built for VoidOS: Alpine base + Linux LTS kernel +
**XFCE desktop** (Xorg, LightDM, elogind, D-Bus), branded as VoidOS.
It answers the request the from-scratch kernel couldn't: an actual
XFCE, not a lookalike.

## Boot it

    ./run-linux.sh            # graphical window when $DISPLAY exists

Login: LightDM autologins user **void** (password `void`) into XFCE.
Root filesystem: `build/voidos-linux.img` (3 GiB ext4, label VOIDOS),
kernel/initramfs in `rootfs/boot/` (linux-lts 6.18.58).

## Package management — vpm

VoidOS's package manager is **vpm** (VoidOS Package Manager):
`vpm install|remove|update|upgrade|search|info|list`. It's a
VoidOS-native frontend (POSIX sh) over the apk backend, so VoidOS
keeps one command surface of its own. Source lives in the rootfs at
`usr/local/bin/vpm` (staged copy in `build/overlay/usr/local/bin/`),
so every image build ships it.

## How it was built

- Rootfs assembled on the host with `apk.static --root rootfs --initdb`
  from Alpine `latest-stable` (main + community): alpine-base,
  linux-lts, xfce4, xfce4-terminal, thunar, xorg-server,
  xf86-input-libinput, xf86-video-vesa, eudev, dbus, elogind,
  lightdm + gtk greeter, fonts, Adwaita icons (499 packages, ~1.6 GiB).
- Branding in `build/overlay/` (os-release, hostname `voidos`, motd,
  generated wallpaper). Services enabled by runlevel symlinks
  (apk --root enables none): udev stack in sysinit; dbus, elogind,
  lightdm in default. LightDM: `autologin-user=void`, `user-session=xfce`.
- Disk image: `mke2fs -t ext4 -d rootfs` (no loop mount needed).

## Gotchas (learned by booting it)

- The kernel cmdline MUST carry `rootfstype=ext4 modules=virtio_blk,ext4`:
  without them this initramfs (usr-merged layout, generated inside the
  apk sandbox) fails its root mount with a bare ENOENT, even though the
  device and filesystem are fine.
- Files copied into the rootfs as root need mode 644: the wallpaper
  shipped 660 at first and xfdesktop (running as `void`) got
  "Permission denied" and painted a black desktop.
- Under QEMU TCG (no /dev/kvm here) the XFCE session takes ~2 minutes
  to fully paint; the greeter/session themselves start in seconds of
  guest time.
- The rootfs MUST ship `/etc/apk/repositories` (main + community) or
  guest-side `vpm update`/`apk` have no package sources at all.
- Rebuilding the image with the static /dev node set: the build host
  blocks mknod, so use `build/make-image.sh` inside ONE
  `fakeroot-sysv --unknown-is-real` session. Plain fakeroot
  (unknown-is-root) flattens EVERY file's ownership to root in the
  image — LightDM then dies on /var/lib/lightdm permissions and no
  desktop comes up. Verify with debugfs afterwards: /dev/null is
  "character special", /var/lib/lightdm is uid 101, /home/void 1000.
- USB: launch QEMU with `-device qemu-xhci,id=usb0`; the guest's
  xhci-pci module loads via udev and `lsusb` shows both root hubs,
  with device nodes under /dev/bus/usb.

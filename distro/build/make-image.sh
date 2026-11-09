#!/bin/sh
# Runs INSIDE one fakeroot session: create the standard static /dev
# nodes in the VoidOS rootfs, then build the ext4 image in the same
# session so mke2fs sees (and stores) the device entries.
set -e
DISTRO=/home/hatch/workspace/voidos/distro
cd "$DISTRO/rootfs/dev"
# spec: name type mode major minor   (c = char, b = block)
for spec in 'null c 666 1 3' 'zero c 666 1 5' 'full c 666 1 7' 'random c 666 1 8' \
            'urandom c 666 1 9' 'tty c 666 5 0' 'console c 600 5 1' 'ptmx c 666 5 2' \
            'tty0 c 600 4 0' 'tty1 c 600 4 1' 'ttyS0 c 600 4 64' \
            'vda b 660 253 0' 'vda1 b 660 253 1' 'vda2 b 660 253 2' 'vda3 b 660 253 3' \
            'sda b 660 8 0' 'sda1 b 660 8 1' 'sda2 b 660 8 2' 'sda3 b 660 8 3' \
            'loop0 b 660 7 0' 'loop1 b 660 7 1' 'loop2 b 660 7 2' 'loop3 b 660 7 3' \
            'loop4 b 660 7 4' 'loop5 b 660 7 5' 'loop6 b 660 7 6' 'loop7 b 660 7 7' \
            'loop-control c 660 10 237'; do
    set -- $spec
    rm -f "$1"
    mknod -m "$3" "$1" "$2" "$4" "$5"
done
ls -la
cd "$DISTRO"
rm -f build/voidos-linux.img.new
truncate -s 3G build/voidos-linux.img.new
/usr/sbin/mke2fs -q -t ext4 -m 1 -L VOIDOS -F -d rootfs build/voidos-linux.img.new
echo "image built with device nodes"

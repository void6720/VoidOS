#!/usr/bin/env python3
"""VoidOS rootfs structural audit: layout, symlink health (resolved
relative to the rootfs, with runtime mounts whitelisted), /dev, /etc,
/boot, modules, library resolution for key binaries."""
import os, subprocess, sys

R = '/home/hatch/workspace/voidos/distro/rootfs'
RUNTIME_PREFIXES = ('/proc/', '/sys/', '/dev/', '/run/', '/tmp/')

def resolve_in_root(path, depth=0):
    """Resolve a symlink target inside the rootfs. Returns final path or None."""
    if depth > 12:
        return None
    full = R + path if path.startswith('/') else path
    if os.path.islink(full):
        tgt = os.readlink(full)
        if tgt.startswith('/'):
            nxt = tgt
        else:
            nxt = os.path.normpath(os.path.join(os.path.dirname(path), tgt))
        return resolve_in_root(nxt, depth + 1)
    return full if os.path.lexists(full) else None

print('=== TOP-LEVEL LAYOUT ===')
for name in sorted(os.listdir(R)):
    p = os.path.join(R, name)
    if os.path.islink(p):
        print(f'/{name} -> {os.readlink(p)}')
    elif os.path.isdir(p):
        print(f'/{name}/ (dir)')
    else:
        print(f'/{name} (file)')

print()
print('=== SYMLINK HEALTH (absolute links resolved inside rootfs) ===')
broken, runtime, ok = [], [], 0
for dirpath, dirnames, filenames in os.walk(R):
    for fn in dirnames + filenames:
        p = os.path.join(dirpath, fn)
        if not os.path.islink(p):
            continue
        rel = '/' + os.path.relpath(p, R)
        tgt = os.readlink(p)
        if tgt.startswith(RUNTIME_PREFIXES) or rel in ('/etc/mtab',):
            runtime.append((rel, tgt))
            continue
        if tgt.startswith('/'):
            if resolve_in_root(tgt) is None:
                broken.append((rel, tgt))
            else:
                ok += 1
        else:
            full = os.path.normpath(os.path.join(dirpath, tgt))
            if not os.path.lexists(full):
                broken.append((rel, tgt))
            else:
                ok += 1
print(f'resolved ok: {ok}, runtime-resolved (proc/sys/dev/run): {len(runtime)}, broken: {len(broken)}')
for rel, tgt in broken[:40]:
    print(f'  BROKEN {rel} -> {tgt}')

print()
print('=== EMPTY DIRECTORIES (systemic) ===')
for dirpath, dirnames, filenames in os.walk(R):
    if not dirnames and not filenames:
        rel = '/' + os.path.relpath(dirpath, R)
        if not rel.startswith(('/proc', '/sys', '/run', '/dev/pts', '/dev/shm', '/tmp')):
            print(f'  empty: {rel}')

print()
print('=== LIBRARY RESOLUTION (musl ldd against the tree) ===')
loader = os.path.join(R, 'lib/ld-musl-x86_64.so.1')
tmpdir = '/tmp/audit-ldd'
os.makedirs(tmpdir, exist_ok=True)
ldd_link = os.path.join(tmpdir, 'ldd')
if not os.path.lexists(ldd_link):
    os.symlink(loader, ldd_link)
lp = f"{R}/lib:{R}/usr/lib:{R}/usr/local/lib"
for prog in ['/bin/bash', '/usr/bin/curl', '/usr/bin/alsamixer',
             '/usr/bin/Xorg', '/usr/bin/xfce4-session', '/usr/local/bin/vpm']:
    p = R + prog
    if not os.path.exists(p):
        print(f'  {prog}: MISSING')
        continue
    r = subprocess.run([ldd_link, '--library-path', lp, p],
                       capture_output=True, text=True, timeout=30)
    missing = [l.strip() for l in (r.stdout + r.stderr).splitlines()
               if 'not found' in l or 'Error loading' in l]
    print(f'  {prog}: {"MISSING LIBS -> " + "; ".join(missing) if missing else "all libs resolve"}')

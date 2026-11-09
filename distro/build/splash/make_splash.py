#!/usr/bin/env python3
"""Generate the VoidOS boot splash (fbsplash.ppm, 1280x800) + a PNG preview.
ChromeOS-Flex-style: deep navy glow background, centered logo lockup, calm."""
from PIL import Image, ImageDraw, ImageFont

W, H = 1280, 800
S = 2  # supersample
w, h = W * S, H * S

# --- background: vertical navy gradient + soft elliptical glow ---
top = (10, 24, 48)      # #0a1830
bottom = (4, 9, 20)     # #040914
glow = (20, 51, 95)     # #14335f
bg = Image.new('RGB', (w, h))
px = bg.load()
cx, cy = w * 0.5, h * 0.40
rx, ry = w * 0.62, h * 0.52
import math
for y in range(h):
    t = y / h
    base = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
    for x in range(0, w, 4):  # step 4 for speed, fill blocks
        dx = (x - cx) / rx
        dy = (y - cy) / ry
        d2 = dx * dx + dy * dy
        g = max(0.0, 1.0 - d2)
        g = g * g * 0.85
        col = tuple(int(base[i] + (glow[i] - base[i]) * g) for i in range(3))
        for xx in range(x, min(x + 4, w)):
            px[xx, y] = col
bg = bg.resize((W, H), Image.LANCZOS)
d = ImageDraw.Draw(bg, 'RGBA')

BOLD = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
REG = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'

# --- logo lockup, centered around (640, 345) ---
# two overlapping rounded squares (VoidOS mark), left of the wordmark
sq = 92          # square size
r = 16
gx, gy = 640 - 250, 345 - sq // 2 - 14   # green, upper-left
ox, oy = gx + 26, gy + 30                # orange, lower-right, overlapping
d.rounded_rectangle([ox, oy, ox + sq, oy + sq], r, fill=(245, 166, 35))   # orange
d.rounded_rectangle([gx, gy, gx + sq, gy + sq], r, fill=(121, 179, 74))   # green

# wordmark
font = ImageFont.truetype(BOLD, 118)
tx = gx + sq + 38
ty = 345 - 62
d.text((tx, ty), 'VoidOS', font=font, fill=(238, 244, 251))

# tagline, letterspaced, centered under the lockup
tag = 'L I N U X   ·   X F C E   E D I T I O N'
tf = ImageFont.truetype(REG, 25)
tw = d.textlength(tag, font=tf)
d.text((640 - tw / 2, 452), tag, font=tf, fill=(141, 171, 214))

bg.save('/home/hatch/workspace/voidos/distro/rootfs/fbsplash.ppm')
bg.save('/home/hatch/workspace/voidos/distro/build/splash/fbsplash-preview.png')
print('written: rootfs/fbsplash.ppm + preview png')

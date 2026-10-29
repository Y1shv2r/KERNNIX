#!/usr/bin/python3
"""Render original vector marbling to the TFT's compact RGB565 asset."""
from pathlib import Path
import math
import random
import gi
import cairo
from PIL import Image

gi.require_version('Rsvg','2.0')
from gi.repository import Rsvg
root=Path(__file__).resolve().parents[1]
asset=root/'ui/assets'
def center(y):
    return 105-.59*y+16*math.sin(y*.058)+7*math.sin(y*.12)
def path(points):
    return 'M'+' L'.join(f'{x:.2f} {y:.2f}' for x,y in points)
parts=['''<svg xmlns="http://www.w3.org/2000/svg" width="144" height="144" viewBox="0 0 144 144">
<defs><radialGradient id="night"><stop stop-color="#172d38"/><stop offset="1" stop-color="#020a10"/></radialGradient></defs>
<rect width="144" height="144" fill="url(#night)"/>''']
# Layered, flowing bands retain a hand-printed contour at tiny TFT resolution.
for layer in range(8):
    width=31-layer*2.5
    left=[(center(y)-width,y) for y in range(-8,154,2)]
    right=[(center(y)+width,y) for y in range(152,-10,-2)]
    color=['#776b59','#b09a77','#d0b58b','#ead8b5','#c8ac84','#e7d0a7','#d8bc91','#e9d5b2'][layer]
    parts.append(f'<path d="{path(left+right)} Z" fill="{color}"/>')
for side in [-1,1]:
    for k in range(12):
        offset=side*(9+k*3.3)
        points=[(center(y)+offset+1.1*math.sin(y*.2+k),y) for y in range(-6,152,2)]
        color='#d7bd92' if k>6 else '#645a4b'
        parts.append(f'<path d="{path(points)}" fill="none" stroke="{color}" stroke-width="{.35 if k%3 else .6}" opacity=".65"/>')
# Black stone in the lower right, with fine topographic veins.
parts.append('<path d="M145 62 C114 66 101 82 98 98 C94 116 74 127 70 145 L145 145Z" fill="#050c10"/>')
for k in range(12):
    points=[(145-t*.65,78+k*5+5*math.sin(t*.06+k*.3)) for t in range(0,100,2)]
    parts.append(f'<path d="{path(points)}" fill="none" stroke="#84775d" stroke-width=".45" opacity=".65"/>')
parts.append('<circle cx="43" cy="37" r="12" fill="#e7d2ac"/>')
parts.append('<circle cx="39" cy="34" r="4" fill="#d8c49e" opacity=".6"/><circle cx="46" cy="41" r="2.7" fill="#cbb994" opacity=".45"/>')
rng=random.Random(934)
for _ in range(45):
    x=rng.randrange(1,143);y=rng.randrange(1,143)
    if abs(x-center(y))>34 and (x-43)**2+(y-37)**2>220:
        parts.append(f'<circle cx="{x}" cy="{y}" r=".35" fill="#c6b184" opacity=".65"/>')
parts.append('</svg>')
svg=asset/'eclipse-marble.svg';svg.write_text('\n'.join(parts))
handle=Rsvg.Handle.new_from_file(str(svg))
surface=cairo.ImageSurface(cairo.FORMAT_ARGB32,576,576)
context=cairo.Context(surface);context.scale(4,4)
rect=Rsvg.Rectangle();rect.x=0;rect.y=0;rect.width=144;rect.height=144
handle.render_document(context,rect)
import io
buffer=io.BytesIO();surface.write_to_png(buffer);buffer.seek(0)
im=Image.open(buffer).convert('RGB').resize((144,144),Image.Resampling.LANCZOS)
im.save(asset/'eclipse-marble.png')
values=[((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in im.getdata()]
(asset/'eclipse-marble.h').write_text('/* RGB565 rendition of the original eclipse-marble.svg. */\nstatic const unsigned short theme_marble[144*144]={\n'+',\n'.join(','.join(f'0x{v:04x}' for v in values[i:i+16]) for i in range(0,len(values),16))+'\n};\n')

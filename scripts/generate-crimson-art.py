#!/usr/bin/python3
"""Render original crimson-sky, blossom and mountain artwork for the TFT."""
from pathlib import Path
import io
import math
import random
import gi
import cairo
from PIL import Image

gi.require_version('Rsvg','2.0')
from gi.repository import Rsvg
root=Path(__file__).resolve().parents[1];asset=root/'ui/assets'
parts=['''<svg xmlns="http://www.w3.org/2000/svg" width="144" height="144" viewBox="0 0 144 144">
<defs><linearGradient id="sky" x2="0" y2="1"><stop stop-color="#6b050b"/><stop offset=".5" stop-color="#d32d24"/><stop offset="1" stop-color="#261018"/></linearGradient></defs>
<rect width="144" height="144" fill="url(#sky)"/>
<circle cx="86" cy="44" r="30" fill="#f4dec0"/>
<path d="M0 105 L24 84 L39 93 L64 72 L91 100 L112 88 L144 103 L144 144 L0 144Z" fill="#382c30"/>
<path d="M64 72 L54 88 L64 83 L75 87Z" fill="#af8e78"/>
<path d="M0 116 L20 106 L40 110 L65 95 L84 113 L110 100 L144 113 L144 144 L0 144Z" fill="#101a1d"/>
<path d="M0 137 L30 119 L53 125 L78 107 L107 128 L126 119 L144 125 L144 144 L0 144Z" fill="#050d10"/>
<g fill="none" stroke="#110d11" stroke-linecap="round">
<path d="M-6 111 C11 83 12 58 17 39 C25 18 35 16 47 9" stroke-width="5"/>
<path d="M12 70 C31 54 38 42 59 35 M16 43 C10 29 6 24 0 20 M24 29 C35 32 51 25 66 19" stroke-width="2.5"/>
<path d="M30 54 L23 37 M41 44 L43 23 M54 36 L66 28 M39 20 L34 10 M12 55 L2 43" stroke-width="1.5"/>
</g>''']
rng=random.Random(118)
positions=[(8,41),(13,29),(23,32),(28,19),(35,12),(40,25),(47,18),(49,38),(57,33),(64,24),(22,55),(30,47),(5,61),(10,77)]
for x,y in positions:
 for k in range(5):
  a=k*math.tau/5
  px=x+math.cos(a)*2.6;py=y+math.sin(a)*2.6
  parts.append(f'<ellipse cx="{px:.2f}" cy="{py:.2f}" rx="2.5" ry="1.8" fill="{rng.choice(["#d33132","#ed4b44","#aa1c28","#f06d54"])}"/>')
 parts.append(f'<circle cx="{x}" cy="{y}" r=".8" fill="#e6b97c"/>')
for _ in range(24):
 x=rng.randrange(2,142);y=rng.randrange(106,143)
 parts.append(f'<ellipse cx="{x}" cy="{y}" rx="1.4" ry=".65" transform="rotate(-25 {x} {y})" fill="{rng.choice(["#ec4133","#ab2028","#cf7150"])}"/>')
parts.append('</svg>')
svg=asset/'crimson-blossom.svg';svg.write_text('\n'.join(parts))
handle=Rsvg.Handle.new_from_file(str(svg));surface=cairo.ImageSurface(cairo.FORMAT_ARGB32,576,576)
context=cairo.Context(surface);context.scale(4,4)
rect=Rsvg.Rectangle();rect.x=0;rect.y=0;rect.width=144;rect.height=144
handle.render_document(context,rect)
buffer=io.BytesIO();surface.write_to_png(buffer);buffer.seek(0)
im=Image.open(buffer).convert('RGB').resize((144,144),Image.Resampling.LANCZOS)
im.save(asset/'crimson-blossom.png')
values=[((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in im.getdata()]
(asset/'crimson-blossom.h').write_text('/* RGB565 rendition of original crimson-blossom.svg. */\nstatic const unsigned short theme_blossom[144*144]={\n'+',\n'.join(','.join(f'0x{v:04x}' for v in values[i:i+16]) for i in range(0,len(values),16))+'\n};\n')

#!/usr/bin/env python3
"""Bake fixed-cell TFT glyph masks; runtime needs no font library."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parents[1]
font_path='/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
parts=['/* Generated DejaVu Sans Mono masks; see assets/fonts/LICENSE. */\n']
for name,size,width,height in [('small',9,6,10),('large',15,12,16)]:
    font=ImageFont.truetype(font_path,size)
    parts.append(f'static const unsigned char theme_font_{name}[95][{width*height}] = {{\n')
    for code in range(32,127):
        image=Image.new('L',(width,height))
        draw=ImageDraw.Draw(image)
        # Top alignment keeps the existing renderer's text anchor coordinates.
        box=font.getbbox(chr(code))
        draw.text(((width-font.getlength(chr(code)))/2,-font.getbbox('H')[1]),chr(code),font=font,fill=255)
        parts.append('{'+','.join(str(v) for v in image.tobytes())+'},\n')
    parts.append('};\n')
(root/'ui/assets/fonts/theme-font.h').write_text(''.join(parts))

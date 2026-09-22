#!/usr/bin/env python3
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path("assets-cg")
OUT.mkdir(exist_ok=True)

W, H = 92, 64
font = ImageFont.load_default()

def make(path, selected=False):
    bg = 0 if selected else 255
    fg = 255 if selected else 0
    img = Image.new("RGB", (W, H), (bg, bg, bg))
    d = ImageDraw.Draw(img)

    # Document outline
    x0, y0, x1, y1 = 20, 6, 70, 58
    d.rectangle((x0, y0, x1, y1), outline=(fg, fg, fg), width=3)
    d.polygon([(57,6),(70,19),(57,19)], outline=(fg,fg,fg))
    d.line((57,6,57,19,70,19), fill=(fg,fg,fg), width=2)

    # Text lines
    for y in (27, 34, 41):
        d.line((29, y, 61, y), fill=(fg, fg, fg), width=2)

    label = "PDF"
    box = d.textbbox((0, 0), label, font=font)
    tw = box[2] - box[0]
    d.text(((W - tw)//2, 47), label, font=font, fill=(fg,fg,fg))
    img.save(path)

make(OUT / "icon-uns.png", False)
make(OUT / "icon-sel.png", True)
print("Generated CG50 icons")

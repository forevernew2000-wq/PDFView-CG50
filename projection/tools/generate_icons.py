from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

out = Path("assets-cg")
out.mkdir(parents=True, exist_ok=True)

def make(path, bg, fg):
    img = Image.new("RGB", (92, 64), bg)
    d = ImageDraw.Draw(img)
    d.rectangle((5, 5, 86, 58), outline=fg, width=2)
    d.line((16, 48, 42, 18, 72, 48), fill=fg, width=3)
    d.ellipse((38, 14, 46, 22), outline=fg, width=2)
    d.text((10, 7), "PROY", fill=fg)
    d.text((55, 7), "XY", fill=fg)
    img.save(path)

make(out / "icon-uns.png", (255,255,255), (0,0,0))
make(out / "icon-sel.png", (0,0,0), (255,255,255))
print("Generated ProyCalc icons")

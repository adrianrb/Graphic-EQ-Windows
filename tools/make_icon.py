#!/usr/bin/env python3
"""Gera src/res/app.ico e icon128x128.png (requer Pillow, só para o ícone)."""
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "src" / "res" / "app.ico"
PREVIEW = ROOT / "icon128x128.png"

SIZE = 256
img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

# Painel arredondado
d.rounded_rectangle((8, 8, 248, 248), radius=52, fill=(6, 20, 25, 255),
                    outline=(0, 229, 255, 255), width=6)

# Barras de LED (escala crescente/decrescente)
bars = [70, 100, 140, 172, 136, 96, 66]
xs = [30, 60, 90, 120, 150, 180, 210]
width = 18
base = 208
cyan = (0, 229, 255, 255)
yellow = (255, 224, 77, 255)
orange = (255, 167, 38, 255)
red = (255, 77, 77, 255)
off = (12, 43, 49, 255)

for x, h in zip(xs, bars):
    top = base - h
    d.rounded_rectangle((x, base - 40, x + width, base), radius=5, fill=off)
    d.rounded_rectangle((x, top, x + width, base), radius=5, fill=cyan)
    if h > 120:
        d.rounded_rectangle((x, top, x + width, top + 16), radius=5, fill=red)
    elif h > 90:
        d.rounded_rectangle((x, top, x + width, top + 12), radius=5, fill=yellow)
    # Pico branco
    d.rounded_rectangle((x, top - 10, x + width, top - 6), radius=2,
                        fill=(255, 255, 255, 255))

OUT.parent.mkdir(parents=True, exist_ok=True)
img.save(OUT, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64),
                     (128, 128), (256, 256)])
img.resize((128, 128), Image.Resampling.LANCZOS).save(PREVIEW)
print(f"gerado {OUT} e {PREVIEW}")

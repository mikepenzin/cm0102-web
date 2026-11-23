"""Draws the background of the .dmg window: a red hexagon net with the logo at the top, and a light
area below where the app and the Applications folder sit, with an arrow between them.

Usage:
    python tools/mac/dmg_background.py <out.tiff> [--logo <logo.png>] [--edition "Bosman Edition"]

The logo is the Bosman Edition logo (the site's logo.png, made by tools/logo/make_bosman_logo.py), which
already has BOSMAN EDITION in its art. It is not in git, so the release passes it in. With it the band shows
the logo alone. Without it the band shows only the edition name. Writes one TIFF with a
1x and a 2x picture (tiffutil), so the window is sharp on Retina screens. Needs Pillow.

Layout, in window points (640 x 400): the band is the top 170 points. The icons sit at (170, 255) and
(470, 255), which tools/mac/dmg_settings.py repeats.
"""
import argparse
import math
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

W, H, BAND = 640, 400, 170
RED_BASE, RED_CELL, RED_EDGE = (77, 7, 10), (138, 13, 18), (184, 23, 30)
LIGHT, INK, GOLD, ARROW = (240, 239, 236), (70, 70, 74), (245, 196, 0), (200, 20, 27)
FONT_CANDIDATES = [
    "/System/Library/Fonts/Supplemental/Arial Black.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    "/System/Library/Fonts/Helvetica.ttc",
]
TEXT_FONTS = ["/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc"]


def font(paths, size):
    for p in paths:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def hexagon(cx, cy, r):
    return [(cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))) for a in range(30, 390, 60)]


def draw(scale, logo_path, edition):
    w, h, band = W * scale, H * scale, BAND * scale
    im = Image.new("RGB", (w, h), LIGHT)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, w, band], fill=RED_BASE)
    r = 30 * scale                       # hexagon radius, pointy top, like the box art's net
    dx, dy = r * math.sqrt(3), r * 1.5
    row = 0
    y = 0.0
    while y < band + r:
        x = (dx / 2 if row % 2 else 0.0)
        while x < w + dx:
            d.polygon(hexagon(x, y, r - 3 * scale), fill=RED_CELL, outline=RED_EDGE, width=max(1, 2 * scale))
            x += dx
        y += dy
        row += 1
    d.rectangle([0, band, w, h], fill=LIGHT)   # cut the net off at the band's edge

    if logo_path:
        # The logo art has BOSMAN EDITION in its own lettering (tools/logo), so the band shows the logo
        # alone, centred in the band. A drawn line under it would show the name twice (0.6.1 item 1).
        logo = Image.open(logo_path).convert("RGBA")
        lw = 250 * scale
        logo = logo.resize((lw, round(logo.height * lw / logo.width)), Image.LANCZOS)
        im.paste(logo, ((w - lw) // 2, (band - logo.height) // 2), logo)
    else:
        ef = font(FONT_CANDIDATES, 22 * scale)
        words = edition.upper().rsplit(" ", 1)      # "BOSMAN" in white, "EDITION" in gold, as on the site
        parts = [(words[0] + " ", (255, 255, 255))] + ([(words[1], GOLD)] if len(words) > 1 else [])
        tw = sum(d.textlength(t, font=ef) for t, _ in parts)
        tx, ty = (w - tw) / 2, (band - 32 * scale) / 2
        for t, colour in parts:
            d.text((tx + 2 * scale, ty + 3 * scale), t, font=ef, fill=(0, 0, 0))
            d.text((tx, ty), t, font=ef, fill=colour)
            tx += d.textlength(t, font=ef)

    # The arrow from the app to the Applications folder.
    ay = 255 * scale
    x0, x1 = 250 * scale, 390 * scale
    d.rectangle([x0, ay - 5 * scale, x1 - 22 * scale, ay + 5 * scale], fill=ARROW)
    d.polygon([(x1, ay), (x1 - 26 * scale, ay - 18 * scale), (x1 - 26 * scale, ay + 18 * scale)], fill=ARROW)
    tf = font(TEXT_FONTS, 13 * scale)
    hint = "Drag the game onto Applications to install it"
    d.text(((w - d.textlength(hint, font=tf)) / 2, 368 * scale), hint, font=tf, fill=INK)
    return im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--logo")
    ap.add_argument("--edition", default="Bosman Edition")
    a = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        one, two = os.path.join(tmp, "bg.png"), os.path.join(tmp, "bg@2x.png")
        draw(1, a.logo, a.edition).save(one, dpi=(72, 72))
        draw(2, a.logo, a.edition).save(two, dpi=(144, 144))
        subprocess.run(["tiffutil", "-cathidpicheck", one, two, "-out", a.out], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f"background written to {a.out}")


if __name__ == "__main__":
    main()

"""The .dmg window background (open items 0.6.1, item 1): with the logo, the red band shows the logo art
alone. The logo already has BOSMAN EDITION in its own lettering (tools/logo), so the band must not draw a
second BOSMAN EDITION line under it. Without a logo the band still shows the edition name.

A stand-in logo of one colour takes the place of the real one, which is not in git. Every band pixel
outside the logo must be a colour of the hexagon net. Needs Pillow (exit 77, a skip, without it).
"""
import os
import sys
import tempfile

try:
    from PIL import Image
except ImportError:
    print("Pillow is missing: skip")
    sys.exit(77)

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "mac"))
import dmg_background as bg  # noqa: E402

LOGO_COLOUR = (0, 0, 255)
NET = {bg.RED_BASE, bg.RED_CELL, bg.RED_EDGE}


def check(scale, logo_path):
    im = bg.draw(scale, logo_path, "Bosman Edition")
    band = bg.BAND * scale
    px = im.load()
    logo = [(x, y) for y in range(band) for x in range(im.width) if px[x, y] == LOGO_COLOUR]
    if not logo:
        return [f"scale {scale}: the logo is not in the band"]
    x0, x1 = min(x for x, _ in logo), max(x for x, _ in logo)
    y0, y1 = min(y for _, y in logo), max(y for _, y in logo)
    errors = []
    if y0 < 0 or y1 >= band:
        errors.append(f"scale {scale}: the logo (rows {y0} to {y1}) is not inside the band")
    stray = [(x, y, px[x, y]) for y in range(band) for x in range(im.width)
             if not (x0 <= x <= x1 and y0 <= y <= y1) and px[x, y] not in NET]
    if stray:
        x, y, c = stray[0]
        errors.append(f"scale {scale}: {len(stray)} band pixels outside the logo are not the hexagon net, "
                      f"first at ({x}, {y}) colour {c} (a drawn line under the logo)")
    return errors


def main():
    errors = []
    with tempfile.TemporaryDirectory() as tmp:
        logo_path = os.path.join(tmp, "logo.png")
        Image.new("RGBA", (1294, 671), LOGO_COLOUR + (255,)).save(logo_path)
        for scale in (1, 2):
            errors += check(scale, logo_path)
    # Without a logo the band shows the edition name in white and gold.
    plain = bg.draw(1, None, "Bosman Edition")
    colours = {plain.getpixel((x, y)) for y in range(bg.BAND) for x in range(bg.W)}
    if (255, 255, 255) not in colours or bg.GOLD not in colours:
        errors.append("without a logo the band does not show the edition name")
    for e in errors:
        print(e)
    print("dmg background: " + ("FAIL" if errors else "PASS"))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

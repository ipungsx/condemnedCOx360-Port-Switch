#!/usr/bin/env python3
"""Homebrew Menu icon from YOUR disc: the title image of default.xex (XDBF image 0x8000, 64x64), enlarged to 256x256
with the game's name under it. Written to condemned/assets/icon.jpg, which is never committed (.gitignore); configure
with -DCCO_ICON_DEL_DISCO=ON (tools/build_linux.sh does when the file exists).

    make_icon.py <image_82000000.bin> [out.jpg]

The image is the decrypted XEX the codegen tool writes with REX_DUMP_SECTIONS=<dir> (tools/rexglue-dump-sections.patch).
Needs Pillow.
"""
import os, struct, sys
from io import BytesIO
from PIL import Image, ImageDraw, ImageFilter, ImageFont

IMAGE_BASE = 0x82000000
RESOURCE = (0x833D2100, 0x67831)  # the "534507D2" resource of default.xex (XEX header 0x000002FF)

src = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "assets", "icon.jpg")
img = open(src, "rb").read()
o = RESOURCE[0] - IMAGE_BASE
d = img[o:o + RESOURCE[1]]
if d[:4] != b"XDBF":
    sys.exit("no XDBF resource where default.xex keeps it: is this Condemned's image?")
_, _, tbl_len, tbl_num, free_len, _ = struct.unpack_from(">4sIIIII", d, 0)
data = 24 + tbl_len * 18 + free_len * 8
png = None
for i in range(tbl_num):
    ns, eid, off, ln = struct.unpack_from(">HQII", d, 24 + i * 18)
    if ns == 2 and eid == 0x8000:
        png = d[data + off:data + off + ln]
if not png:
    sys.exit("no title image (XDBF image 0x8000)")

face = Image.open(BytesIO(png)).convert("RGB")
S = 256
canvas = Image.new("RGB", (S, S), (10, 10, 12))
big = face.resize((S, S), Image.LANCZOS).filter(ImageFilter.UnsharpMask(radius=2, percent=60, threshold=2))
canvas.paste(big, (0, 0))

# Dark band with the name at the bottom, and a thin red rule above it.
band = Image.new("L", (S, S), 0)
ImageDraw.Draw(band).rectangle([0, 192, S, S], fill=200)
canvas.paste(Image.new("RGB", (S, S), (8, 8, 8)), (0, 0), band)
draw = ImageDraw.Draw(canvas)
draw.rectangle([0, 190, S, 192], fill=(150, 20, 20))


def font(size):
    for f in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/Library/Fonts/Arial Bold.ttf",
              "C:/Windows/Fonts/arialbd.ttf"):
        if os.path.exists(f):
            return ImageFont.truetype(f, size)
    return ImageFont.load_default()


draw.text((S // 2, 214), "CONDEMNED", font=font(32), fill=(225, 225, 225), anchor="mm")
draw.text((S // 2, 241), "CRIMINAL ORIGINS", font=font(14), fill=(170, 40, 40), anchor="mm")
os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
canvas.save(out, quality=92)
print("wrote", out)

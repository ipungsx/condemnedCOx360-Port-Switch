#!/usr/bin/env python3
"""Extract your Condemned: Criminal Origins (Xbox 360) ISO into the folder the NRO reads.

    python3 condemned/tools/stage_sd.py "Condemned Criminal Origins.iso" -o game

Writes <out>/default.xex, default.archcfg, autoexec.cfg, Layer0.Arch00, Layer1.Arch00 and StartupAssets/, checks
default.xex against the one this port's code was generated from, and can be stopped and run again (it resumes
partial files). Copy the folder to sdmc:/switch/condemned-nx/game/.

The image is XGD2 (the game partition starts at 0xFD90000); the file system is XDVDFS.
"""
import argparse, hashlib, os, struct, sys

XGD_OFFSETS = (0xFD90000, 0x2080000, 0x18300000, 0)  # XGD2, XGD3, XGD1, plain XISO
SECTOR = 2048
MAGIC = b"MICROSOFT*XBOX*MEDIA"

# default.xex the recompiled code matches (retail, Title ID 534507D2, version 0.0.0.2, 2005-10-15).
XEX_SHA256 = "9d2dbd7a709b68f587e59564819315f3955585b91a533ddae241cc832da59fcf"


def find_base(f):
    for base in XGD_OFFSETS:
        f.seek(base + 0x10000)
        if f.read(20) == MAGIC:
            return base
    sys.exit("Not an Xbox 360 disc image (no XDVDFS volume found)")


def walk(f, base, sector, size, path, out):
    f.seek(base + sector * SECTOR)
    data = f.read(size)
    seen, stack = set(), [0]
    while stack:
        o = stack.pop()
        if o in seen or o * 4 + 14 > len(data):
            continue
        seen.add(o)
        p = o * 4
        if data[p:p + 2] == b"\xff\xff":
            continue
        left, right, start, length, attrib, nlen = struct.unpack_from("<HHIIBB", data, p)
        name = data[p + 14:p + 14 + nlen].decode("latin-1")
        full = f"{path}/{name}" if path else name
        if attrib & 0x10:
            if length:
                walk(f, base, start, length, full, out)
        else:
            out.append((full, start, length))
        if left:
            stack.append(left)
        if right:
            stack.append(right)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("iso")
    ap.add_argument("-o", "--out", default="game")
    ap.add_argument("--list", action="store_true", help="only list the files")
    a = ap.parse_args()
    if not os.path.isfile(a.iso):
        sys.exit(f"{a.iso}: not found")
    with open(a.iso, "rb") as f:
        base = find_base(f)
        f.seek(base + 0x10000 + 20)
        root, root_size = struct.unpack("<II", f.read(8))
        files = []
        walk(f, base, root, root_size, "", files)
        if a.list:
            for n, _, z in sorted(files):
                print(f"{z:>12} {n}")
            return
        names = {n.lower() for n, _, _ in files}
        for need in ("default.xex", "layer0.arch00", "layer1.arch00"):
            if need not in names:
                sys.exit(f"{need} is not on this disc: is it Condemned: Criminal Origins?")
        for n, start, length in sorted(files, key=lambda x: x[2]):
            dst = os.path.join(a.out, *n.split("/"))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            have = os.path.getsize(dst) if os.path.exists(dst) else 0
            if have == length:
                continue
            if have > length:
                have = 0
                os.remove(dst)
            f.seek(base + start * SECTOR + have)
            with open(dst, "ab") as w:
                while have < length:
                    chunk = f.read(min(length - have, 8 << 20))
                    if not chunk:
                        sys.exit(f"{n}: the image ends early (truncated ISO?)")
                    w.write(chunk)
                    have += len(chunk)
            print(f"  {n} ({length // 1048576} MB)")
    xex = os.path.join(a.out, "default.xex")
    digest = hashlib.sha256(open(xex, "rb").read()).hexdigest()
    if digest != XEX_SHA256:
        print(f"warning: default.xex sha256 {digest[:16]}... is not the one this port was generated from "
              f"({XEX_SHA256[:16]}...). Regenerate the code from it (tools/codegen.sh) or use the retail disc.")
    print(f"Ready: {a.out} ({len(files)} files). Copy it to sdmc:/switch/condemned-nx/game/")


if __name__ == "__main__":
    main()

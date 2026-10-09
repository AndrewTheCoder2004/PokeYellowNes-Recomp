#!/usr/bin/env python3
"""Thunder Emperor Recomp - offline launcher / asset tool (stdlib only).

Commands:
  info     [--rom ROM]            validate ROM header, print mapper/vectors
  extract  [--rom ROM] [--png]    dump PRG banks (+ optional 2bpp tile sheets)
  mods     list                   list mods in ./mods
  build    [--rom ROM] [--mods a,b] [--out FILE]   apply IPS/JSON mods -> patched ROM
  input    [--class pc|mobile|console]            print the effective input map
  platforms                       print the platform matrix

The ROM is never copied into the repository; output goes to ./userdata (git-ignored).
"""
import argparse, glob, hashlib, json, os, struct, sys, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
USERDATA = os.path.join(ROOT, "userdata")
EXPECTED_NAME = "Lei Dian Huang - Bi Ka Qiu Chuan Shuo (China)(Unlicensed).nes"
KNOWN_SHA1 = {"88ccfb00e1a18ae3c1711eb88d43628e26ea7897"}


def find_rom(arg):
    for c in [arg, os.path.join(ROOT, EXPECTED_NAME), os.path.join(USERDATA, EXPECTED_NAME)]:
        if c and os.path.isfile(c):
            return c
    sys.exit("ROM not found. Place '%s' in the project root or pass --rom." % EXPECTED_NAME)


def parse_ines(data):
    if data[:4] != b"NES\x1a":
        sys.exit("Not an iNES file.")
    prg, chr_ = data[4] * 16384, data[5] * 8192
    f6, f7 = data[6], data[7]
    off = 16 + (512 if f6 & 4 else 0)
    mapper = (f6 >> 4) | (f7 & 0xF0)
    if len(data) < off + prg + chr_:
        sys.exit("Truncated ROM.")
    return dict(prg=data[off:off + prg], chr=data[off + prg:off + prg + chr_],
                mapper=mapper, mirroring="vertical" if f6 & 1 else "horizontal",
                battery=bool(f6 & 2), header=data[:16])


def load(arg):
    path = find_rom(arg)
    data = open(path, "rb").read()
    sha = hashlib.sha1(data).hexdigest()
    return path, data, sha, parse_ines(data)


def cmd_info(a):
    path, data, sha, r = load(a.rom)
    n = len(r["prg"]) // 16384
    last = r["prg"][-16384:]  # fixed bank assumed last; mapper 163 may remap
    vec = struct.unpack("<HHH", last[-6:])
    print("File     :", path)
    print("SHA-1    :", sha, "(matches reference dump)" if sha in KNOWN_SHA1 else "(differs from reference dump)")
    print("Mapper   :", r["mapper"], "(Nanjing/Waixing)" if r["mapper"] == 163 else "")
    print("PRG      : %d x 16KB = %d KB" % (n, len(r["prg"]) // 1024))
    print("CHR      :", "CHR-RAM (graphics live in PRG)" if not r["chr"] else "%d KB ROM" % (len(r["chr"]) // 1024))
    print("Mirroring:", r["mirroring"])
    print("Vectors  : NMI=%04X RESET=%04X IRQ=%04X (last bank)" % vec)


def png_gray(path, w, h, rows):
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def tile_sheet(bank, path):
    # 16KB = 1024 tiles, 16 tiles per row -> 128 x 512 px
    shade = (0, 85, 170, 255)
    W, H = 128, 512
    img = [[0] * W for _ in range(H)]
    for t in range(1024):
        tx, ty = (t % 16) * 8, (t // 16) * 8
        for y in range(8):
            lo, hi = bank[t * 16 + y], bank[t * 16 + 8 + y]
            for x in range(8):
                img[ty + y][tx + x] = shade[((lo >> (7 - x)) & 1) | (((hi >> (7 - x)) & 1) << 1)]
    png_gray(path, W, H, img)


def cmd_extract(a):
    _, _, _, r = load(a.rom)
    out = os.path.join(USERDATA, "assets")
    os.makedirs(os.path.join(out, "prg"), exist_ok=True)
    if a.png:
        os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    for i in range(len(r["prg"]) // 16384):
        b = r["prg"][i * 16384:(i + 1) * 16384]
        open(os.path.join(out, "prg", "bank%03d.bin" % i), "wb").write(b)
        if a.png:
            tile_sheet(b, os.path.join(out, "tiles", "bank%03d.png" % i))
    print("Extracted to", out)


def ips_apply(rom, p):
    d = open(p, "rb").read()
    if d[:5] != b"PATCH":
        sys.exit("%s: bad IPS" % p)
    rom, i = bytearray(rom), 5
    while d[i:i + 3] != b"EOF":
        off = int.from_bytes(d[i:i + 3], "big"); sz = int.from_bytes(d[i + 3:i + 5], "big"); i += 5
        if sz:
            payload = d[i:i + sz]; i += sz
        else:
            rl = int.from_bytes(d[i:i + 2], "big"); payload = d[i + 2:i + 3] * rl; i += 3
        if off + len(payload) > len(rom):
            rom.extend(b"\0" * (off + len(payload) - len(rom)))
        rom[off:off + len(payload)] = payload
    return bytes(rom)


def list_mods():
    mods = {}
    for mj in glob.glob(os.path.join(ROOT, "mods", "*", "mod.json")):
        m = json.load(open(mj)); m["_dir"] = os.path.dirname(mj)
        mods[m["id"]] = m
    return mods


def cmd_mods(a):
    for m in list_mods().values():
        print("%-16s v%-6s %s" % (m["id"], m.get("version", "?"), m.get("description", "")))


def cmd_build(a):
    _, data, _, _ = load(a.rom)
    avail, rom = list_mods(), data
    for mid in [x for x in (a.mods or "").split(",") if x]:
        m = avail.get(mid) or sys.exit("unknown mod " + mid)
        for p in m.get("ips", []):
            rom = ips_apply(rom, os.path.join(m["_dir"], p))
        rom = bytearray(rom)
        for pt in m.get("patches", []):  # {"offset":"0x10","bytes":"AA BB"} offsets are file offsets
            off, bs = int(pt["offset"], 0), bytes.fromhex(pt["bytes"])
            rom[off:off + len(bs)] = bs
        rom = bytes(rom)
        print("applied", mid)
    out = a.out or os.path.join(USERDATA, "patched.nes")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, "wb").write(rom)
    print("wrote", out, hashlib.sha1(rom).hexdigest())


def cmd_input(a):
    cfg = json.load(open(os.path.join(ROOT, "config", "input_map.json")))
    user = os.path.join(USERDATA, "input_map.json")
    if os.path.isfile(user):
        for k, v in json.load(open(user)).items():
            cfg.setdefault(k, {}).update(v)
    print(json.dumps(cfg[a.cls], indent=2))


def cmd_platforms(a):
    for p in json.load(open(os.path.join(ROOT, "platforms", "platforms.json")))["platforms"]:
        print("%-16s tier %s  %-12s %s" % (p["name"], p["tier"], p["input"], p["backend"]))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    for n, f in (("info", cmd_info), ("extract", cmd_extract), ("build", cmd_build)):
        s = sp.add_parser(n); s.add_argument("--rom"); s.set_defaults(fn=f)
        if n == "extract": s.add_argument("--png", action="store_true")
        if n == "build": s.add_argument("--mods"); s.add_argument("--out")
    s = sp.add_parser("mods"); s.add_argument("sub", nargs="?"); s.set_defaults(fn=cmd_mods)
    s = sp.add_parser("input"); s.add_argument("--class", dest="cls", default="pc", choices=["pc", "mobile", "console"]); s.set_defaults(fn=cmd_input)
    sp.add_parser("platforms").set_defaults(fn=cmd_platforms)
    a = ap.parse_args(); a.fn(a)


if __name__ == "__main__":
    main()

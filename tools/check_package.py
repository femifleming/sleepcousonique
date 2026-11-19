#!/usr/bin/env python3
"""Check a release zip (or its unzipped folder) before anyone downloads it.

    python3 tools/check_package.py dist/sleepcousonique-macOS.zip
    python3 tools/check_package.py dist/sleepcousonique-Windows.zip
    python3 tools/check_package.py --bin firmware/dist/sleepcousonique-sp1.bin

The firmware in it must be firmware/dist/sleepcousonique-sp1.bin byte for byte, say the version
CMakeLists.txt ships (SCQ_FIRMWARE_VERSION), and be an image for the SP-1's application slot
(vector table at 0x20000, never below it, ending before the dream slots at 0xE7000). The
Windows .exe must not need the Visual C++ Redistributable. Exits non-zero on any failure.
"""
import re
import struct
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP_START, APP_END = 0x20000, 0xE7000          # slot0_partition .. scq_partition (dream slots)
RAM = (0x20000000, 0x20040000)

fails = 0


def check(ok, what, detail=""):
    global fails
    fails += 0 if ok else 1
    print(f"  [{'PASS' if ok else 'FAIL'}] {what}" + (f"  ({detail})" if detail else ""))


def cmake_var(name):
    m = re.search(r'set\(%s "([^"]+)"\)' % name, (ROOT / "CMakeLists.txt").read_text())
    return m.group(1) if m else None


def project_version():
    m = re.search(r"project\(Sleepcousonique VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text())
    return m.group(1)


def check_bin(data, label):
    fw = cmake_var("SCQ_FIRMWARE_VERSION")
    sp, reset = struct.unpack_from("<II", data, 0)
    check(RAM[0] < sp <= RAM[1], f"{label}: initial stack pointer in the SP-1's RAM", f"{sp:#x}")
    check(reset & 1 and APP_START <= (reset & ~1) < APP_START + len(data),
          f"{label}: reset vector inside the image at 0x20000 (linked for the application slot)", f"{reset:#x}")
    check(APP_START + len(data) <= APP_END, f"{label}: ends before the dream slots (0xE7000)",
          f"ends {APP_START + len(data):#x}, {len(data)} bytes")
    versions = set(re.findall(rb"SLEEPCOUSONIQUE (\d+\.\d+\.\d+[-\w]*)", data))
    check(versions == {fw.encode()}, f"{label}: answers HELLO as SLEEPCOUSONIQUE {fw}",
          ", ".join(v.decode() for v in versions) or "no version string")
    shipped = (ROOT / "firmware" / "dist" / "sleepcousonique-sp1.bin").read_bytes()
    check(data == shipped, f"{label}: is firmware/dist/sleepcousonique-sp1.bin byte for byte")
    versioned = ROOT / "firmware" / "dist" / f"sleepcousonique-sp1-v{fw}.bin"
    check(versioned.exists() and versioned.read_bytes() == data, f"{label}: is {versioned.name}")


def pe_imports(data):
    """The DLL names a PE file imports (enough of the format for that)."""
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe:pe + 4] == b"PE\0\0"
    nsec, = struct.unpack_from("<H", data, pe + 6)
    optsize, = struct.unpack_from("<H", data, pe + 20)
    opt = pe + 24
    magic, = struct.unpack_from("<H", data, opt)
    dd = opt + (112 if magic == 0x20B else 96)
    imp_rva, = struct.unpack_from("<I", data, dd + 8)
    secs = []
    for i in range(nsec):
        s = opt + optsize + 40 * i
        vsize, va, rsize, raw = struct.unpack_from("<IIII", data, s + 8)
        secs.append((va, max(vsize, rsize), raw))

    def off(rva):
        for va, size, raw in secs:
            if va <= rva < va + size:
                return rva - va + raw
        raise ValueError(hex(rva))

    names, p = [], off(imp_rva)
    while True:
        name_rva = struct.unpack_from("<I", data, p + 12)[0]
        if name_rva == 0:
            break
        o = off(name_rva)
        names.append(data[o:data.index(b"\0", o)].decode())
        p += 20
    return names


def main():
    args = sys.argv[1:]
    if args[:1] == ["--bin"]:
        check_bin(Path(args[1]).read_bytes(), Path(args[1]).name)
    else:
        z = Path(args[0])
        files = {}
        if z.suffix == ".zip":
            with zipfile.ZipFile(z) as zf:
                files = {n.split("/", 1)[1]: zf.read(n) for n in zf.namelist() if "/" in n and not n.endswith("/")
                         and "/Contents/Resources/" not in n}
        else:
            files = {str(p.relative_to(z)): p.read_bytes() for p in z.rglob("*") if p.is_file()}
        print(f"check_package: {z}")
        check("sleepcousonique-sp1.bin" in files, "the firmware is in it")
        if "sleepcousonique-sp1.bin" in files:
            check_bin(files["sleepcousonique-sp1.bin"], "sleepcousonique-sp1.bin")
        for f in ("FLASHING.txt", "Sleepcousonique-Manual.pdf", "README.md", "LICENSE", "examples/first-night.dream"):
            check(f in files, f"{f} is in it")
        flashing = files.get("FLASHING.txt", b"").decode("utf-8", "replace")
        check("standby" in flashing.lower(), "FLASHING.txt says the SP-1 waits in standby after flashing")
        ver = project_version()
        if "Sleepcousonique.exe" in files:
            exe = files["Sleepcousonique.exe"]
            dlls = pe_imports(exe)
            crt = [d for d in dlls if re.match(r"(?i)(vcruntime|msvcp|concrt|ucrtbased)", d)]
            check(not crt, "Sleepcousonique.exe needs no Visual C++ Redistributable", ", ".join(crt) or f"{len(dlls)} system DLLs")
            check(f"{ver}".encode("utf-16-le") in exe or ver.encode() in exe, f"Sleepcousonique.exe is version {ver}")
        else:
            plist = files.get("Sleepcousonique.app/Contents/Info.plist", b"").decode("utf-8", "replace")
            check(f"<string>{ver}</string>" in plist, f"Sleepcousonique.app is version {ver}")
            check("Sleepcousonique Array.app/Contents/Info.plist" in files, "Sleepcousonique Array.app is in it")
    print("OK" if not fails else f"FAILED: {fails}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())

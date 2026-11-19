# atmslot.py <path-suffix> [<path-suffix> ...] [--diff]: reads the compiled settings block (DAT1 section 0x02f06d4e, 0x628 bytes) of vanilla atmospheres from toc.BAK,
# maps every 4-byte word to the slot offset the game's parser copies it to (RECON Part 19 table: slot = section + delta of its block), and prints the words as hex/float.
# With --diff (two or more atmospheres) only the words that differ are printed. Nothing is written; needs tools/ALERT.
import contextlib
import io
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib
import dat1lib.crc64 as crc64

A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
BLOCKS = [(0x70, 0xb0, 0x238), (0xb0, 0xd0, 0x240), (0xd0, 0x128, 0x270), (0x128, 0x220, 0x270), (0x220, 0x418, 0x270), (0x418, 0x628, 0x298)]
SECTION = 0x02f06d4e


def slot_of(sec):
    for lo, hi, d in BLOCKS:
        if lo <= sec < hi:
            return sec + d
    return None


def compiled(toc, suffix):
    path = suffix if "/" in suffix else "atmosphere/atm_ibl_lighting/" + suffix
    e = toc.get_asset_entries_by_assetid(crc64.hash(path))
    if not e:
        return None
    with contextlib.redirect_stdout(io.StringIO()):
        data = toc.extract_asset(e[0])
    i = data.find(b"\x31\x54\x41\x44")   # 'DAT1'
    if i < 0:
        return None
    nsec = struct.unpack_from("<H", data, i + 12)[0]
    for k in range(nsec):
        tag, off, size = struct.unpack_from("<III", data, i + 16 + 12 * k)
        if tag == SECTION:
            return data[i + off:i + off + size]
    return None


def main():
    names = [a for a in sys.argv[1:] if not a.startswith("--")]
    diff = "--diff" in sys.argv
    with contextlib.redirect_stdout(io.StringIO()):
        toc = dat1lib.read(open(os.path.join(A, "toc.BAK"), "rb"))
        toc.set_archives_dir(A)
    blobs = []
    for n in names:
        b = compiled(toc, n)
        if b is None:
            print("not found / no settings section:", n)
            return
        blobs.append(b)
    n = min(len(b) for b in blobs)
    print("compiled block sizes:", [len(b) for b in blobs])
    for off in range(0, n - 3, 4):
        ws = [struct.unpack_from("<I", b, off)[0] for b in blobs]
        if diff and len(set(ws)) == 1:
            continue
        if not diff and not any(ws):
            continue
        fs = [struct.unpack_from("<f", b, off)[0] for b in blobs]
        s = slot_of(off)
        print("sec+%03x slot+%s  " % (off, "%03x" % s if s is not None else "---") + "  |  ".join("%08x %-12.6g" % (w, f) for w, f in zip(ws, fs)))


if __name__ == "__main__":
    main()

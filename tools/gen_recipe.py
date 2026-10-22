# usage: gen_recipe.py <base|hof|toe> <game root> <golden pak> <out .recipe>
# Recipe = how to rebuild every entry of the golden pak from the user's own game data:
# a patch (line hunks) against the entry of the same name the game would load, or new content.
import difflib
import struct
import sys
import zipfile
import zlib
from pathlib import Path

# Must match src/data/game_paks.cpp: data\*.pak at 0, then the exe's hard-coded table; our own pak is skipped.
TEXT = -1
TABLES = {
    "base": ("datals", [("p5-texts", TEXT), ("p5-data", 1), ("p6-texts", TEXT), ("p6-data", 2),
                        ("p7-texts", TEXT), ("p7-data", 3), ("p8-texts", TEXT), ("p8-data", 4)]),
    "hof": ("dataa1", [("texts", TEXT), ("p0-data", 1), ("p1-data", 1), ("p2-data", 1), ("p3-data", 1),
                       ("p0-texts", TEXT), ("p1-texts", TEXT), ("p2-texts", TEXT), ("p3-texts", TEXT),
                       ("a1-texts", 2), ("a1-sound", 2), ("a1-data", 2), ("a1p1-data", 3), ("a1p1-texts", TEXT),
                       ("a1p1-sound", 3)]),
    "toe": ("data", []),
}
OWN_PAK = "homm5-widescreen.pak"
KIND_NEW, KIND_PATCH = 0, 1


def dos_datetime(info: zipfile.ZipInfo) -> int:
    y, mo, d, h, mi, s = info.date_time
    return ((y - 1980) << 25) | (mo << 21) | (d << 16) | (h << 11) | (mi << 5) | (s // 2)


def game_paks(root: Path, game: str):
    scanned = sorted((p for p in (root / "data").iterdir() if p.suffix.lower() == ".pak"), key=lambda p: p.name.upper())
    folder, table = TABLES[game]
    mounts = [(p, 0) for p in scanned] + [(root / folder / f"{name}.pak", priority) for name, priority in table]
    return [(p, priority) for p, priority in mounts if p.is_file() and p.name.lower() != OWN_PAK]


class Archives:
    def __init__(self, paks):
        self.files = {}
        for path, priority in paks:
            z = zipfile.ZipFile(path)
            for info in z.infolist():
                key = info.filename.lower()
                current = self.files.get(key)
                if current:
                    current_priority, current_date = current[0]
                    by_date = TEXT in (priority, current_priority) or priority == current_priority
                    if not (dos_datetime(info) > current_date if by_date else priority > current_priority):
                        continue
                self.files[key] = ((priority, dos_datetime(info)), z, info)

    def get(self, name):
        entry = self.files.get(name.lower())
        return entry[1].read(entry[2]) if entry else None


def hunks(source: bytes, target: bytes):
    a, b = source.splitlines(keepends=True), target.splitlines(keepends=True)
    offsets = [0]
    for line in a:
        offsets.append(offsets[-1] + len(line))
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
        if tag != "equal":
            yield offsets[i1], offsets[i2] - offsets[i1], b"".join(b[j1:j2])


def apply(source: bytes, edits) -> bytes:
    out, pos = bytearray(), 0
    for offset, delete, insert in edits:
        out += source[pos:offset] + insert
        pos = offset + delete
    return bytes(out + source[pos:])


def main(game: str, root: str, golden: str, out: str) -> None:
    sources = Archives(game_paks(Path(root), game))
    pak = zipfile.ZipFile(golden)
    infos = sorted(pak.infolist(), key=lambda i: i.filename)
    times = {dos_datetime(i) for i in infos}
    if len(times) != 1:
        raise RuntimeError("golden entries carry different timestamps")
    blob = bytearray(b"H5RECIPE" + struct.pack("<III", 1, times.pop(), len(infos)))
    patched = new = hunk_count = inserted = 0
    for info in infos:
        target = pak.read(info)
        name = info.filename.encode("utf-8")
        blob += struct.pack("<H", len(name)) + name
        source = sources.get(info.filename)
        if source is None:
            blob += struct.pack("<BI", KIND_NEW, len(target)) + target
            new += 1
            inserted += len(target)
            continue
        edits = list(hunks(source, target))
        if apply(source, edits) != target:
            raise RuntimeError(f"{info.filename}: hunks do not reproduce the golden entry")
        blob += struct.pack("<BIII", KIND_PATCH, len(source), zlib.crc32(source), len(edits))
        for offset, delete, insert in edits:
            blob += struct.pack("<III", offset, delete, len(insert)) + insert
            inserted += len(insert)
        patched += 1
        hunk_count += len(edits)
    Path(out).write_bytes(blob)
    print(f"{out}: {patched} patched ({hunk_count} hunks), {new} new, {inserted} inserted bytes, {len(blob)} bytes total")


if __name__ == "__main__":
    main(*sys.argv[1:])

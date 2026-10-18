#!/usr/bin/env python3
"""List and extract files from X-Ray (Anomaly / Monolith) ``.db*`` / ``.xdb*`` archives.

Stdlib only. The format below is derived from the pinned engine source
(``engine/xray-monolith/src``, upstream ``c57fe5b9``), which is the authority:

Container (a flat sequence of chunks, read from file offset 0)
    ``open_chunk`` in ``xrCore/LocatorAPI.cpp:290-337`` reads ``u32 type``,
    ``u32 size``, then either consumes ``size`` bytes (when
    ``type & ~CFS_CompressMark == ID``) or seeks past them. ``CFS_CompressMark``
    is bit 31 (``xrCore/FS.h:8``); a chunk with that bit set is LZHUF-packed
    and inflated through ``_decompressLZ`` (``LocatorAPI.cpp:314-323``).
    ``IReader::open_chunk``/``find_chunk`` in ``xrCore/FS_impl.h:76-110,112-200``
    use the same layout for in-memory readers.

Chunks the engine looks at
    * ``CFS_HeaderChunkID`` = 666 (``FS.h:9``): an ltx/ini blob parsed as
      ``CInifile`` in ``CLocatorAPI::ProcessArchive`` (``LocatorAPI.cpp:462-498``).
      ``[header] auto_load`` decides whether the archive is mounted at start-up
      and ``[header] entry_point`` (``LoadArchive``, ``LocatorAPI.cpp:339-386``)
      is the virtual path prefix: ``gamedata`` maps to ``$fs_root$\\gamedata\\``,
      anything else must start with a ``$alias$``. If the chunk is missing the
      archive is still auto-loaded, but ``LoadArchive`` then asserts
      ``"unsupported"`` (line 384), so shipped archives always carry it.
    * File table = chunk id 1 (``LoadArchive``, ``LocatorAPI.cpp:400-434``).
      Entries repeat until EOF of the (decompressed) chunk::

          u16 buffer_size              # bytes that follow, INCLUDING ptr (hdr->r(buffer, buffer_size))
          u32 size_real
          u32 size_compr
          u32 crc
          u8  name[buffer_size - 16]   # name_length = buffer_size - 4*sizeof(u32); no NUL, backslashes
          u32 ptr                      # absolute offset of the data in the archive

      ``Register`` (``LocatorAPI.cpp:224-262``) lowercases the name and, if the
      same path was registered before, *overwrites* the earlier record. So the
      last archive mounted wins, and loose ``$game_data$`` files (scanned after
      the ``$arch_dir*$`` lines in fsgame.ltx) win over archives.

Per-file payload (``file_from_archive``, ``LocatorAPI.cpp:1167-1203``)
    ``size_real == size_compr``: stored raw at ``ptr``.
    Otherwise: LZO1X, inflated by ``rtc_decompress`` (``xrCore/rt_compressor.cpp:37-46``
    -> ``lzo1x_decompress``, ``rt_lzo1x_d1.cpp:40-45`` including
    ``rt_lzo1x_d.ch:57-456`` with ``LZO1X`` / ``M2_MAX_OFFSET 0x0800`` from
    ``rt_config1x.h:47,65``). It is *not* LZHUF and not zlib. There is no
    per-archive key, XOR or "ANOMALY" header in this engine: the only
    ``.xdb``-specific code in ``LocatorAPI.cpp`` (lines 392-398) is commented out.

LZHUF (chunk compression, ``xrCore/lzhuf.cpp``)
    Classic Okumura LZHUF: ``N=4096, F=60, THRESHOLD=2`` (lines 21-29),
    adaptive Huffman ``StartHuff``/``reconst``/``update`` (414-522),
    ``DecodeChar``/``DecodePosition`` with the ``d_code``/``d_len`` tables
    (339-409, 563-598), bit reader ``GetBit``/``GetByte`` (131-161) and
    ``Decode`` (667-709), which first reads a 4-byte little-endian
    uncompressed size written by ``Encode`` (607-610).

Mount order (which copy of a duplicated path the engine sees)
    ``CLocatorAPI::_initialize`` (``LocatorAPI.cpp:766-838``) walks fsgame.ltx top
    to bottom and calls ``Recurse`` (``LocatorAPI.cpp:600-648``) for each path.
    ``Recurse`` sorts the directory entries by name (``pred_str_ff``, line 565)
    and ``ProcessOne`` (538-562) feeds every ``*.db*``/``*.xdb*`` file to
    ``ProcessArchive``. Passing a directory to this tool expands it the same way
    (sorted, non-recursive). Pass directories in fsgame.ltx order to reproduce the
    engine's overlay; the ``--resolve`` listing / default extraction then keep the
    last occurrence of each path, as ``Register`` does.

Usage
    xray-db-extract.py list    ARCHIVE_OR_DIR... [-m SUBSTR] [-g GLOB] [--resolve] [-l]
    xray-db-extract.py extract ARCHIVE_OR_DIR... -o OUTDIR [-m SUBSTR] [-g GLOB] [--every]
    xray-db-extract.py grep    PATTERN ARCHIVE_OR_DIR... [-g GLOB] [-i]
    xray-db-extract.py info    ARCHIVE...
"""

from __future__ import annotations

import argparse
import fnmatch
import os
import re
import struct
import sys
from dataclasses import dataclass
from typing import Iterable, Iterator

CFS_COMPRESS_MARK = 1 << 31  # FS.h:8
CFS_HEADER_CHUNK_ID = 666  # FS.h:9
FILE_TABLE_CHUNK_ID = 1  # LocatorAPI.cpp:400


# ----------------------------------------------------------------------------
# LZHUF decoder (xrCore/lzhuf.cpp)
# ----------------------------------------------------------------------------

LZ_N = 4096
LZ_F = 60
LZ_THRESHOLD = 2
LZ_N_CHAR = 256 - LZ_THRESHOLD + LZ_F  # 314
LZ_T = LZ_N_CHAR * 2 - 1  # 627
LZ_R = LZ_T - 1  # 626
LZ_MAX_FREQ = 0x4000


def _build_d_tables() -> tuple[list[int], list[int]]:
    # Generates exactly the d_code / d_len tables at lzhuf.cpp:339-409:
    # 6-bit position prefix 'code' is repeated 'count' times, with code length 'length'.
    d_code: list[int] = []
    d_len: list[int] = []
    for codes, repeat, length in (
        (range(0x00, 0x01), 32, 3),
        (range(0x01, 0x04), 16, 4),
        (range(0x04, 0x0C), 8, 5),
        (range(0x0C, 0x18), 4, 6),
        (range(0x18, 0x30), 2, 7),
        (range(0x30, 0x40), 1, 8),
    ):
        for code in codes:
            d_code.extend([code] * repeat)
            d_len.extend([length] * repeat)
    assert len(d_code) == 256 and len(d_len) == 256
    # spot checks against the engine table
    assert d_code[0x80] == 0x0A and d_len[0x80] == 5  # row 16 of the engine table
    assert d_code[0x50] == 0x04 and d_len[0x50] == 5  # row 10
    assert d_code[0xFF] == 0x3F and d_len[0xFF] == 8
    assert d_code[0x20] == 0x01 and d_len[0x20] == 4
    return d_code, d_len


D_CODE, D_LEN = _build_d_tables()


class LZHufDecoder:
    """Port of lzhuf.cpp Decode(); one instance per buffer (the engine's state is global)."""

    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0
        self.getbuf = 0
        self.getlen = 0
        self.freq = [0] * (LZ_T + 1)
        self.prnt = [0] * (LZ_T + LZ_N_CHAR + 1)
        self.son = [0] * LZ_T

    # LZfs::_getb / GetBit / GetByte (lzhuf.cpp:65-69,131-161)
    def _getb(self) -> int:
        if self.pos >= len(self.data):
            return -1
        b = self.data[self.pos]
        self.pos += 1
        return b

    def _fill(self) -> None:
        while self.getlen <= 8:
            i = self._getb()
            if i < 0:
                i = 0
            self.getbuf |= i << (8 - self.getlen)
            self.getlen += 8

    def get_bit(self) -> int:
        self._fill()
        i = self.getbuf
        self.getbuf = (self.getbuf << 1) & 0xFFFF  # only bits 0..15 are ever read back
        self.getlen -= 1
        return (i & 0x8000) >> 15

    def get_byte(self) -> int:
        self._fill()
        i = self.getbuf
        self.getbuf = (self.getbuf << 8) & 0xFFFF
        self.getlen -= 8
        return (i & 0xFF00) >> 8

    # StartHuff (lzhuf.cpp:414-436)
    def start_huff(self) -> None:
        freq, son, prnt = self.freq, self.son, self.prnt
        for i in range(LZ_N_CHAR):
            freq[i] = 1
            son[i] = i + LZ_T
            prnt[i + LZ_T] = i
        i = 0
        j = LZ_N_CHAR
        while j <= LZ_R:
            freq[j] = freq[i] + freq[i + 1]
            son[j] = i
            prnt[i] = prnt[i + 1] = j
            i += 2
            j += 1
        freq[LZ_T] = 0xFFFF
        prnt[LZ_R] = 0

    # reconst (lzhuf.cpp:440-482)
    def reconst(self) -> None:
        freq, son, prnt = self.freq, self.son, self.prnt
        j = 0
        for i in range(LZ_T):
            if son[i] >= LZ_T:
                freq[j] = (freq[i] + 1) // 2
                son[j] = son[i]
                j += 1
        i = 0
        for j in range(LZ_N_CHAR, LZ_T):
            k = i + 1
            f = freq[i] + freq[k]
            freq[j] = f
            k = j - 1
            while f < freq[k]:
                k -= 1
            k += 1
            freq[k + 1 : j + 1] = freq[k:j]
            freq[k] = f
            son[k + 1 : j + 1] = son[k:j]
            son[k] = i
            i += 2
        for i in range(LZ_T):
            k = son[i]
            if k >= LZ_T:
                prnt[k] = i
            else:
                prnt[k] = prnt[k + 1] = i

    # update (lzhuf.cpp:486-522)
    def update(self, c: int) -> None:
        freq, son, prnt = self.freq, self.son, self.prnt
        if freq[LZ_R] == LZ_MAX_FREQ:
            self.reconst()
        c = prnt[c + LZ_T]
        while True:
            freq[c] += 1
            k = freq[c]
            lidx = c + 1
            if k > freq[lidx]:
                lidx += 1
                while k > freq[lidx]:
                    lidx += 1
                lidx -= 1
                freq[c] = freq[lidx]
                freq[lidx] = k
                i = son[c]
                prnt[i] = lidx
                if i < LZ_T:
                    prnt[i + 1] = lidx
                j = son[lidx]
                son[lidx] = i
                prnt[j] = c
                if j < LZ_T:
                    prnt[j + 1] = c
                son[c] = j
                c = lidx
            c = prnt[c]
            if c == 0:
                break

    # DecodeChar (lzhuf.cpp:563-580)
    def decode_char(self) -> int:
        son = self.son
        c = son[LZ_R]
        while c < LZ_T:
            c += self.get_bit()
            c = son[c]
        c -= LZ_T
        self.update(c)
        return c

    # DecodePosition (lzhuf.cpp:582-598)
    def decode_position(self) -> int:
        i = self.get_byte()
        c = D_CODE[i] << 6
        j = D_LEN[i] - 2
        while j > 0:
            i = (i << 1) + self.get_bit()
            j -= 1
        return c | (i & 0x3F)

    # Decode (lzhuf.cpp:667-709)
    def decode(self) -> bytes:
        textsize = self._getb()
        textsize |= self._getb() << 8
        textsize |= self._getb() << 16
        textsize |= self._getb() << 24
        if textsize <= 0:
            return b""
        out = bytearray()
        self.start_huff()
        text_buf = bytearray(LZ_N + LZ_F)
        for i in range(LZ_N - LZ_F):
            text_buf[i] = 0x20
        r = LZ_N - LZ_F
        count = 0
        mask = LZ_N - 1
        while count < textsize:
            c = self.decode_char()
            if c < 256:
                out.append(c)
                text_buf[r] = c
                r = (r + 1) & mask
                count += 1
            else:
                i = (r - self.decode_position() - 1) & mask
                j = c - 255 + LZ_THRESHOLD
                for k in range(j):
                    c = text_buf[(i + k) & mask]
                    out.append(c)
                    text_buf[r] = c
                    r = (r + 1) & mask
                    count += 1
        return bytes(out)


def decompress_lz(data: bytes) -> bytes:
    """_decompressLZ (lzhuf.cpp:734-741)."""
    return LZHufDecoder(data).decode()


# ----------------------------------------------------------------------------
# LZO1X decoder (xrCore/rt_lzo1x_d.ch, LZO1X branch, no dictionary)
# ----------------------------------------------------------------------------

M2_MAX_OFFSET = 0x0800  # rt_config1x.h:65


class LZOError(ValueError):
    pass


def lzo1x_decompress(src: bytes, expected_len: int | None = None) -> bytes:
    """Port of lzo1x_decompress (rt_lzo1x_d.ch:57-456) with the overrun checks of the _safe variant."""
    n = len(src)
    out = bytearray()
    ip = 0

    def need_ip(k: int) -> None:
        if ip + k > n:
            raise LZOError("input overrun")

    def literals(cnt: int) -> None:
        nonlocal ip
        need_ip(cnt)
        out.extend(src[ip : ip + cnt])
        ip += cnt

    def copy_match(m_pos: int, cnt: int) -> None:
        if m_pos < 0:
            raise LZOError("lookbehind overrun")
        op = len(out)
        if op - m_pos >= cnt:
            out.extend(out[m_pos : m_pos + cnt])
        else:  # overlapping copy: repeat byte by byte as the C loop does
            for i in range(cnt):
                out.append(out[m_pos + i])

    if n == 0:
        raise LZOError("empty input")

    t = 0
    state = "loop"
    if src[0] > 17:  # d.ch:104-112
        t = src[0] - 17
        ip = 1
        if t < 4:
            state = "match_next"
        else:
            literals(t)
            state = "first_literal_run"

    while True:
        if state == "loop":  # d.ch:114-163
            need_ip(1)
            t = src[ip]
            ip += 1
            if t >= 16:
                state = "match"
                continue
            if t == 0:
                need_ip(1)
                while src[ip] == 0:
                    t += 255
                    ip += 1
                    need_ip(1)
                t += 15 + src[ip]
                ip += 1
            literals(t + 3)
            state = "first_literal_run"
            continue

        if state == "first_literal_run":  # d.ch:166-194
            need_ip(1)
            t = src[ip]
            ip += 1
            if t >= 16:
                state = "match"
                continue
            need_ip(1)
            m_pos = len(out) - (1 + M2_MAX_OFFSET) - (t >> 2) - (src[ip] << 2)
            ip += 1
            copy_match(m_pos, 3)
            state = "match_done"
            continue

        if state == "match":  # d.ch:199-402
            op = len(out)
            if t >= 64:  # M2
                need_ip(1)
                m_pos = op - 1 - ((t >> 2) & 7) - (src[ip] << 3)
                ip += 1
                t = (t >> 5) - 1
                copy_match(m_pos, t + 2)
            elif t >= 32:  # M3
                t &= 31
                if t == 0:
                    need_ip(1)
                    while src[ip] == 0:
                        t += 255
                        ip += 1
                        need_ip(1)
                    t += 31 + src[ip]
                    ip += 1
                need_ip(2)
                m_pos = op - 1 - ((src[ip] >> 2) + (src[ip + 1] << 6))
                ip += 2
                copy_match(m_pos, t + 2)
            elif t >= 16:  # M4
                m_pos = op - ((t & 8) << 11)
                t &= 7
                if t == 0:
                    need_ip(1)
                    while src[ip] == 0:
                        t += 255
                        ip += 1
                        need_ip(1)
                    t += 7 + src[ip]
                    ip += 1
                need_ip(2)
                m_pos -= (src[ip] >> 2) + (src[ip + 1] << 6)
                ip += 2
                if m_pos == op:  # eof_found (d.ch:333-334, 432-436)
                    if t != 1:
                        raise LZOError("bad EOF marker")
                    if ip != n:
                        raise LZOError("input not consumed" if ip < n else "input overrun")
                    break
                m_pos -= 0x4000
                copy_match(m_pos, t + 2)
            else:  # M1
                need_ip(1)
                m_pos = op - 1 - (t >> 2) - (src[ip] << 2)
                ip += 1
                copy_match(m_pos, 2)
            state = "match_done"
            continue

        if state == "match_done":  # d.ch:404-411
            t = src[ip - 2] & 3
            state = "loop" if t == 0 else "match_next"
            continue

        if state == "match_next":  # d.ch:414-423
            literals(t)
            need_ip(1)
            t = src[ip]
            ip += 1
            state = "match"
            continue

        raise AssertionError(state)

    if expected_len is not None and len(out) != expected_len:
        raise LZOError(f"decompressed {len(out)} bytes, table says {expected_len}")
    return bytes(out)


# ----------------------------------------------------------------------------
# Archive reader
# ----------------------------------------------------------------------------


@dataclass(frozen=True)
class Entry:
    name: str  # as stored, lowercased, forward slashes
    size_real: int
    size_compr: int
    crc: int
    ptr: int
    archive: str


class Archive:
    def __init__(self, path: str):
        self.path = path
        self.header: dict[str, str] = {}
        self.entries: list[Entry] = []
        self.chunks: list[tuple[int, bool, int, int]] = []  # (id, compressed, offset, size)
        with open(path, "rb") as fh:
            self._scan(fh)

    # open_chunk (LocatorAPI.cpp:290-337): walk the chunk list from offset 0
    def _scan(self, fh) -> None:
        fh.seek(0, os.SEEK_END)
        end = fh.tell()
        fh.seek(0)
        pos = 0
        header_raw = None
        table_raw = None
        while pos + 8 <= end:
            fh.seek(pos)
            dw_type, dw_size = struct.unpack("<II", fh.read(8))
            cid = dw_type & ~CFS_COMPRESS_MARK & 0xFFFFFFFF
            compressed = bool(dw_type & CFS_COMPRESS_MARK)
            self.chunks.append((cid, compressed, pos + 8, dw_size))
            if cid in (CFS_HEADER_CHUNK_ID, FILE_TABLE_CHUNK_ID):
                raw = fh.read(dw_size)
                if compressed:
                    raw = decompress_lz(raw)
                if cid == CFS_HEADER_CHUNK_ID and header_raw is None:
                    header_raw = raw
                elif cid == FILE_TABLE_CHUNK_ID and table_raw is None:
                    table_raw = raw
            pos += 8 + dw_size
        if header_raw is not None:
            self.header = _parse_header_ini(header_raw)
        if table_raw is None:
            raise ValueError(f"{path_display(self.path)}: no file table (chunk 1)")
        self._parse_table(table_raw)

    # LoadArchive (LocatorAPI.cpp:400-434)
    def _parse_table(self, buf: bytes) -> None:
        pos = 0
        n = len(buf)
        while pos < n:
            (buffer_size,) = struct.unpack_from("<H", buf, pos)
            pos += 2
            if buffer_size < 16:
                raise ValueError(f"{path_display(self.path)}: bad table entry at {pos - 2}")
            # buffer_size covers size_real, size_compr, crc, name AND ptr (hdr->r(buffer, buffer_size));
            # name_length = buffer_size - 4*sizeof(u32) and ptr is read from the last 4 bytes.
            size_real, size_compr, crc = struct.unpack_from("<III", buf, pos)
            name = buf[pos + 12 : pos + buffer_size - 4].decode("cp1251", "replace")
            (ptr,) = struct.unpack_from("<I", buf, pos + buffer_size - 4)
            pos += buffer_size
            name = name.lower().replace("\\", "/")
            self.entries.append(Entry(name, size_real, size_compr, crc, ptr, self.path))

    @property
    def entry_point(self) -> str:
        return self.header.get("entry_point", "")

    # file_from_archive (LocatorAPI.cpp:1167-1203)
    def read(self, e: Entry) -> bytes:
        with open(self.path, "rb") as fh:
            fh.seek(e.ptr)
            raw = fh.read(e.size_compr)
        if len(raw) != e.size_compr:
            raise ValueError(f"{e.name}: short read from {path_display(self.path)}")
        if e.size_real == e.size_compr:
            return raw
        return lzo1x_decompress(raw, e.size_real)


def _parse_header_ini(raw: bytes) -> dict[str, str]:
    """Minimal CInifile reader for the [header] section (keys lowercased)."""
    result: dict[str, str] = {}
    section = None
    for line in raw.decode("cp1251", "replace").splitlines():
        line = line.split(";", 1)[0].strip()
        if not line:
            continue
        if line.startswith("["):
            section = line.strip("[]").strip().lower()
            continue
        if section != "header" or "=" not in line:
            continue
        k, v = line.split("=", 1)
        result[k.strip().lower()] = v.strip()
    return result


def path_display(p: str) -> str:
    return os.path.relpath(p) if os.path.isabs(p) and p.startswith(os.getcwd()) else p


def is_archive_name(name: str) -> bool:
    # ProcessOne (LocatorAPI.cpp:558): extension starts with ".db" or ".xdb"
    ext = os.path.splitext(name)[1].lower()
    return ext.startswith(".db") or ext.startswith(".xdb")


def expand_archives(args: Iterable[str]) -> list[str]:
    """Files as given; directories expand to their sorted *.db*/*.xdb* members (Recurse, non-recursive)."""
    result: list[str] = []
    for a in args:
        if os.path.isdir(a):
            names = sorted(n for n in os.listdir(a) if is_archive_name(n) and os.path.isfile(os.path.join(a, n)))
            result.extend(os.path.join(a, n) for n in names)
        elif os.path.isfile(a):
            result.append(a)
        else:
            sys.exit(f"error: no such archive or directory: {a}")
    if not result:
        sys.exit("error: no archives found")
    return result


def make_matcher(substr: str | None, glob: str | None):
    substr_l = substr.lower() if substr else None
    glob_l = glob.lower() if glob else None

    def match(name: str) -> bool:
        if substr_l and substr_l not in name:
            return False
        if glob_l and not (fnmatch.fnmatchcase(name, glob_l) or fnmatch.fnmatchcase(os.path.basename(name), glob_l)):
            return False
        return True

    return match


def iter_entries(archives: list[Archive], match) -> Iterator[tuple[Archive, Entry]]:
    for a in archives:
        for e in a.entries:
            if match(e.name):
                yield a, e


def resolve(archives: list[Archive], match) -> dict[str, tuple[Archive, Entry]]:
    """Last registration wins (Register, LocatorAPI.cpp:244-253)."""
    winners: dict[str, tuple[Archive, Entry]] = {}
    for a, e in iter_entries(archives, match):
        winners[e.name] = (a, e)
    return winners


def safe_out_path(outdir: str, name: str) -> str:
    parts = [p for p in name.split("/") if p not in ("", ".")]
    if any(p == ".." for p in parts):
        raise ValueError(f"refusing path with '..': {name}")
    return os.path.join(outdir, *parts)


# ----------------------------------------------------------------------------
# Commands
# ----------------------------------------------------------------------------


def load_archives(paths: list[str], quiet: bool = False) -> list[Archive]:
    archives = []
    for p in paths:
        if not quiet:
            print(f"# reading {path_display(p)}", file=sys.stderr)
        archives.append(Archive(p))
    return archives


def cmd_info(ns) -> int:
    for p in expand_archives(ns.archives):
        a = Archive(p)
        print(f"{path_display(p)}: {len(a.entries)} entries, header={a.header or '(none)'}")
        for cid, comp, off, size in a.chunks:
            print(f"  chunk id={cid} compressed={int(comp)} data_offset={off} size={size}")
        stored = sum(1 for e in a.entries if e.size_real == e.size_compr)
        print(f"  stored={stored} lzo={len(a.entries) - stored}")
    return 0


def cmd_list(ns) -> int:
    archives = load_archives(expand_archives(ns.archives), ns.quiet)
    match = make_matcher(ns.match, ns.glob)
    if ns.resolve:
        items = sorted(resolve(archives, match).values(), key=lambda ae: ae[1].name)
    else:
        items = list(iter_entries(archives, match))
    for a, e in items:
        if ns.long:
            comp = "lzo" if e.size_real != e.size_compr else "raw"
            arch = path_display(a.path)
            print(f"{e.size_real:>10} {e.size_compr:>10} {comp} {e.crc:08x} {e.ptr:>10} {arch} {e.name}")
        else:
            print(f"{path_display(a.path)}\t{e.name}")
    return 0


def cmd_extract(ns) -> int:
    archives = load_archives(expand_archives(ns.archives), ns.quiet)
    match = make_matcher(ns.match, ns.glob)
    if ns.names:
        wanted = {n.lower().replace("\\", "/") for n in ns.names}
        base = match

        def match(name: str) -> bool:  # noqa: F811 - intentional narrowing
            return base(name) and (name in wanted or os.path.basename(name) in wanted)

    if ns.every:
        items = [(a, e) for a, e in iter_entries(archives, match)]
    else:
        items = list(resolve(archives, match).values())
    if not items:
        print("nothing matched", file=sys.stderr)
        return 1
    count = 0
    for a, e in items:
        if e.size_real == 0 and e.name.endswith("/"):
            continue  # directory record
        sub = os.path.splitext(os.path.basename(a.path))[0] + os.path.splitext(a.path)[1] if ns.every else ""
        dest = safe_out_path(os.path.join(ns.outdir, sub) if sub else ns.outdir, e.name)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        data = a.read(e)
        with open(dest, "wb") as fh:
            fh.write(data)
        count += 1
        if not ns.quiet:
            print(f"{path_display(a.path)} -> {dest} ({len(data)} bytes)")
    print(f"extracted {count} file(s) to {ns.outdir}", file=sys.stderr)
    return 0


def cmd_grep(ns) -> int:
    archives = load_archives(expand_archives(ns.archives), ns.quiet)
    match = make_matcher(ns.match, ns.glob)
    flags = re.IGNORECASE if ns.ignore_case else 0
    pattern = re.compile(ns.pattern.encode("latin-1") if ns.regex else re.escape(ns.pattern.encode("latin-1")), flags)
    items = iter_entries(archives, match) if ns.every else resolve(archives, match).values()
    hits = 0
    for a, e in items:
        if e.size_real == 0:
            continue
        data = a.read(e)
        if not pattern.search(data):
            continue
        for lineno, line in enumerate(data.split(b"\n"), 1):
            if pattern.search(line):
                hits += 1
                text = line.rstrip(b"\r").decode("cp1251", "replace")
                print(f"{path_display(a.path)}:{e.name}:{lineno}: {text}")
    return 0 if hits else 1


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description=__doc__.split("\n\n", 1)[0], formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("-q", "--quiet", action="store_true", help="do not print progress to stderr")
    sub = ap.add_subparsers(dest="cmd", required=True)

    def add_common(p, with_every: bool):
        p.add_argument("archives", nargs="+", help="archive files or directories (expanded sorted, in mount order)")
        p.add_argument("-m", "--match", help="case-insensitive substring of the stored path")
        p.add_argument("-g", "--glob", help="glob on the stored path or its basename, e.g. '*.script'")
        if with_every:
            p.add_argument(
                "--every", action="store_true", help="every copy from every archive, not just the last-mounted one"
            )

    p = sub.add_parser("info", help="show header ini and chunk layout")
    p.add_argument("archives", nargs="+")
    p.set_defaults(func=cmd_info)

    p = sub.add_parser("list", help="list entries")
    add_common(p, False)
    p.add_argument("-l", "--long", action="store_true", help="show sizes, compression, crc and offset")
    p.add_argument(
        "--resolve", action="store_true", help="collapse duplicates: last archive wins (engine Register semantics)"
    )
    p.set_defaults(func=cmd_list)

    p = sub.add_parser("extract", help="extract entries to a directory")
    add_common(p, True)
    p.add_argument("-o", "--outdir", required=True)
    p.add_argument("-n", "--name", dest="names", action="append", help="exact stored path or basename (repeatable)")
    p.set_defaults(func=cmd_extract)

    p = sub.add_parser("grep", help="search file contents inside archives")
    p.add_argument("pattern")
    add_common(p, True)
    p.add_argument("-i", "--ignore-case", action="store_true")
    p.add_argument("-E", "--regex", action="store_true", help="treat PATTERN as a Python regex (default: literal)")
    p.set_defaults(func=cmd_grep)

    ns = ap.parse_args(argv)
    return ns.func(ns)


if __name__ == "__main__":
    sys.exit(main())

"""Independently validate the document test's PNG using Python's standard-library codecs."""
import binascii
import struct
import sys
import zlib
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
assert data[:8] == b"\x89PNG\r\n\x1a\n"
offset = 8
compressed = bytearray()
width = height = 0
ended = False
while offset < len(data):
    size = struct.unpack_from(">I", data, offset)[0]
    kind = data[offset + 4:offset + 8]
    payload = data[offset + 8:offset + 8 + size]
    checksum = struct.unpack_from(">I", data, offset + 8 + size)[0]
    assert binascii.crc32(kind + payload) == checksum
    if kind == b"IHDR":
        width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
        assert (depth, color, compression, filtering, interlace) == (8, 6, 0, 0, 0)
    elif kind == b"IDAT":
        compressed.extend(payload)
    elif kind == b"IEND":
        assert size == 0
        ended = True
    offset += size + 12
assert ended and offset == len(data)
raw = zlib.decompress(compressed)
assert len(raw) == (width * 4 + 1) * height
assert all(raw[y * (width * 4 + 1)] == 0 for y in range(height))
pixel = 350 * (width * 4 + 1) + 1 + 20 * 4
assert raw[pixel:pixel + 4] == b"\xff\x00\x00\xff"
print(f"Validated {width}x{height} RGBA PNG, chunk CRCs, DEFLATE, Adler-32, and rendered red pixel")

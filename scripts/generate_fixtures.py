"""Regenerate project-owned test PDFs: uv run --with pypdf==6.21.4 python scripts/generate_fixtures.py"""
from pathlib import Path

from pypdf import PdfWriter
from pypdf.generic import ArrayObject, ByteStringObject

root = Path(__file__).resolve().parents[1] / "tests" / "fixtures"
root.mkdir(parents=True, exist_ok=True)


def stream(data: bytes, extra: bytes = b"") -> bytes:
    return b"<< /Length " + str(len(data)).encode() + b" " + extra + b" >>\nstream\n" + data + b"\nendstream"


subject = (b"\xfe\xff" + "中文 / 😀".encode("utf-16-be")).hex().encode()
objects = [
    b"<< /Type /Catalog /Pages 2 0 R >>",
    b"<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>",
    b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] /Resources << /Font << /F1 5 0 R /F2 11 0 R >> >> /Contents 6 0 R /Annots [9 0 R] >>",
    b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] /Rotate 90 /Resources << /Font << /F1 5 0 R >> >> /Contents 7 0 R >>",
    b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    stream(b"1 0 0 rg 10 10 80 80 re f\n0 0 0 rg BT /F1 14 Tf 20 340 Td (Hello PDF hello PDF helloWorld) Tj ET\nBT /F2 14 Tf 20 300 Td <000100020003> Tj ET"),
    stream(b"0 0.4 0.8 rg 10 10 80 80 re f\n0 0 0 rg BT /F1 20 Tf 20 300 Td (Second page) Tj ET"),
    b"<< /Title (HuxerUI PDF sample) /Author (HuxerUI) /Subject <" + subject + b"> >>",
    b"<< /Type /Annot /Subtype /Square /Rect [150 10 230 90] /F 4 /AP << /N 10 0 R >> >>",
    stream(b"0 0 1 rg 0 0 80 80 re f", b"/Type /XObject /Subtype /Form /BBox [0 0 80 80] /Resources << >>"),
    b"<< /Type /Font /Subtype /Type0 /BaseFont /Arial /Encoding /Identity-H /DescendantFonts [12 0 R] /ToUnicode 14 0 R >>",
    b"<< /Type /Font /Subtype /CIDFontType2 /BaseFont /Arial /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> /FontDescriptor 13 0 R /DW 1000 /CIDToGIDMap /Identity >>",
    b"<< /Type /FontDescriptor /FontName /Arial /Flags 32 /FontBBox [0 -200 1000 1000] /ItalicAngle 0 /Ascent 800 /Descent -200 /CapHeight 700 /StemV 80 >>",
    stream(b"/CIDInit /ProcSet findresource begin 12 dict begin begincmap\n/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n/CMapName /HuxerUI-Unicode def /CMapType 2 def\n1 begincodespacerange <0000> <FFFF> endcodespacerange\n3 beginbfchar <0001> <4E2D> <0002> <6587> <0003> <D83DDE00> endbfchar\nendcmap CMapName currentdict /CMap defineresource pop end end"),
]
pdf = bytearray(b"%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")
offsets = [0]
for number, obj in enumerate(objects, 1):
    offsets.append(len(pdf))
    pdf += f"{number} 0 obj\n".encode() + obj + b"\nendobj\n"
xref = len(pdf)
pdf += f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode()
for offset in offsets[1:]:
    pdf += f"{offset:010d} 00000 n \n".encode()
pdf += f"trailer\n<< /Size {len(offsets)} /Root 1 0 R /Info 8 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode()
(root / "sample.pdf").write_bytes(pdf)

writer = PdfWriter(clone_from=root / "sample.pdf")
writer._ID = ArrayObject([ByteStringObject(b"HuxerUI PDF 2026")] * 2)
writer.encrypt(user_password="huxerui", owner_password="owner", algorithm="RC4-128")
writer.write(root / "encrypted.pdf")
print(f"Generated fixtures in {root}")

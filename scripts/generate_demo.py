"""Generate the project-owned demo document with the Python standard library."""
from pathlib import Path

root = Path(__file__).resolve().parents[1] / "examples/preview/resources/raw"
root.mkdir(parents=True, exist_ok=True)
objects: list[bytes] = []


def obj(value: bytes) -> int:
    objects.append(value)
    return len(objects)


def stream(value: str, extra: str = "") -> bytes:
    data = value.encode("ascii")
    return f"<< /Length {len(data)} {extra} >>\nstream\n".encode() + data + b"\nendstream"


def text(x: float, y: float, size: int, value: str, bold: bool = False, color: str = "0.08 0.13 0.22") -> str:
    escaped = value.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")
    return f"{color} rg BT /F{2 if bold else 1} {size} Tf {x} {y} Td ({escaped}) Tj ET\n"


def box(x: float, y: float, w: float, h: float, color: str) -> str:
    return f"{color} rg {x} {y} {w} {h} re f\n"


catalog = obj(b"")
pages = obj(b"")
font = obj(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")
bold = obj(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>")
page_ids = []

for index, (width, height, title, subtitle) in enumerate([
    (595, 842, "The PDF field guide", "A native reading experience, built with HuxerUI."),
    (595, 842, "Read. Search. Explore.", "Every page has more than pixels."),
    (842, 595, "A wider perspective", "Landscape pages, crisp details, and room to explore."),
], 1):
    commands = box(0, 0, width, height, "1 1 1")
    commands += box(0, height - 16, width, 16, "0.05 0.48 0.46")
    commands += text(44, height - 60, 11, "HUXERUI  /  PDF", True, "0.05 0.48 0.46")
    commands += text(44, height - 116, 32, title, True)
    commands += text(44, height - 145, 12, subtitle)
    if index == 1:
        commands += box(44, 420, 507, 220, "0.07 0.14 0.24")
        commands += text(68, 590, 12, "ONE DOCUMENT. THREE WAYS TO EXPLORE.", True, "0.38 0.84 0.75")
        for y, number, label in [(535, "01", "View and navigate"), (485, "02", "Find the words that matter"), (435, "03", "Read the underlying text")]:
            commands += text(68, y, 18, number, True, "0.38 0.84 0.75")
            commands += text(112, y, 18, label, True, "1 1 1")
        commands += text(44, 370, 17, "Make yourself at home", True)
        for y, line in [(342, "Use Next and Previous to move through this PDF."),
                        (320, "Zoom in for detail, or rotate a page to change your perspective."),
                        (298, "Try searching for PDF, then open Page text to copy an excerpt."),
                        (276, "Open your own file, including a password-protected document.")]:
            commands += text(44, y, 12, line)
    elif index == 2:
        for y, heading, body in [(600, "PAGE IMAGES", "Scale the PDF page while keeping text and vector shapes crisp."),
                                  (490, "TEXT SEARCH", "Search this page for PDF. Matching is case-insensitive by default."),
                                  (380, "DOCUMENT DETAILS", "The side panel shows title, author, page count, and page size.")]:
            commands += box(44, y - 50, 507, 90, "0.94 0.97 0.97")
            commands += text(62, y + 10, 12, heading, True, "0.05 0.48 0.46")
            commands += text(62, y - 18, 11, body)
        commands += text(44, 250, 17, "A document you can inspect", True)
        commands += text(44, 220, 12, "This PDF contains real text, vector graphics, and an annotation.")
        commands += text(44, 198, 12, "The sample is bundled with the app. No network connection is needed.")
    else:
        commands += text(44, 370, 17, "A small set of shapes, at any scale", True)
        for x, y, w, h, color in [(44, 130, 140, 170, "0.05 0.48 0.46"),
                                    (204, 130, 140, 115, "0.23 0.65 0.60"),
                                    (364, 130, 140, 210, "0.07 0.14 0.24"),
                                    (524, 130, 140, 145, "0.45 0.77 0.72")]:
            commands += box(x, y, w, h, color)
        commands += text(44, 94, 12, "Try zooming to 200% or rotating this landscape PDF page.")
    commands += box(44, 55, width - 88, 1, "0.84 0.88 0.90")
    commands += text(44, 32, 10, "LIB-PDF  /  NATIVE DOCUMENTS", True, "0.35 0.42 0.48")
    commands += text(width - 100, 32, 10, f"0{index} / 03", False, "0.35 0.42 0.48")
    content = obj(stream(commands))
    annotation = ""
    if index == 1:
        appearance = box(0, 0, 507, 70, "1 0.94 0.70")
        appearance += text(18, 43, 12, "A NOTE IN THE MARGIN", True)
        appearance += text(18, 20, 11, "Toggle Annotations to hide or show this PDF annotation.")
        ap = obj(stream(appearance, "/Type /XObject /Subtype /Form /BBox [0 0 507 70] "
                        f"/Resources << /Font << /F1 {font} 0 R /F2 {bold} 0 R >> >>"))
        annot = obj(f"<< /Type /Annot /Subtype /Square /Rect [44 140 551 210] /F 4 /AP << /N {ap} 0 R >> >>".encode())
        annotation = f"/Annots [{annot} 0 R]"
    page_ids.append(obj(f"<< /Type /Page /Parent {pages} 0 R /MediaBox [0 0 {width} {height}] "
                        f"/Resources << /Font << /F1 {font} 0 R /F2 {bold} 0 R >> >> "
                        f"/Contents {content} 0 R {annotation} >>".encode()))

objects[catalog - 1] = f"<< /Type /Catalog /Pages {pages} 0 R >>".encode()
objects[pages - 1] = f"<< /Type /Pages /Count 3 /Kids [{' '.join(f'{page} 0 R' for page in page_ids)}] >>".encode()
info = obj(b"<< /Title (The PDF field guide) /Author (HuxerUI) /Subject (Native PDF preview and document APIs) >>")
pdf = bytearray(b"%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")
offsets = [0]
for number, value in enumerate(objects, 1):
    offsets.append(len(pdf))
    pdf += f"{number} 0 obj\n".encode() + value + b"\nendobj\n"
xref = len(pdf)
pdf += f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode()
for offset in offsets[1:]:
    pdf += f"{offset:010d} 00000 n \n".encode()
pdf += f"trailer\n<< /Size {len(offsets)} /Root {catalog} 0 R /Info {info} 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode()
(root / "guide.pdf").write_bytes(pdf)
print(f"Generated {root / 'guide.pdf'}")

#!/usr/bin/env python3
"""Convert every page of a PDF into the compact CGV format used by PDFView."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import sys

try:
    import fitz  # PyMuPDF
    from PIL import Image
except ImportError as exc:
    print("Missing dependency:", exc, file=sys.stderr)
    print("Install with: py -m pip install -r requirements.txt", file=sys.stderr)
    raise SystemExit(2)

MAGIC = b"CGPDF1\0\0"
HEADER_SIZE = 16
INDEX_RECORD_SIZE = 20
FIT_W = 384
FIT_H = 216
FIT_SIZE = ((FIT_W + 7) // 8) * FIT_H
MAX_ZOOM_BYTES = 128 * 1024


def pixmap_to_gray_image(pix: fitz.Pixmap) -> Image.Image:
    if pix.n != 1:
        raise ValueError("Expected a grayscale pixmap")
    return Image.frombytes("L", (pix.width, pix.height), pix.samples)


def render_gray(page: fitz.Page, max_w: int, max_h: int) -> Image.Image:
    rect = page.rect
    if rect.width <= 0 or rect.height <= 0:
        raise ValueError("PDF page has invalid dimensions")

    scale = min(max_w / rect.width, max_h / rect.height)
    pix = page.get_pixmap(
        matrix=fitz.Matrix(scale, scale),
        colorspace=fitz.csGRAY,
        alpha=False,
    )
    return pixmap_to_gray_image(pix)


def render_fit(page: fitz.Page) -> Image.Image:
    img = render_gray(page, FIT_W, FIT_H)
    canvas = Image.new("L", (FIT_W, FIT_H), 255)
    x = (FIT_W - img.width) // 2
    y = (FIT_H - img.height) // 2
    canvas.paste(img, (x, y))
    return canvas


def render_zoom(page: fitz.Page, max_side: int) -> Image.Image:
    return render_gray(page, max_side, max_side)


def pack_black_bits(img: Image.Image, threshold: int) -> bytes:
    """Return MSB-first 1-bpp rows, where bit 1 means black."""
    if img.mode != "L":
        img = img.convert("L")
    mask = img.point(lambda p: 255 if p < threshold else 0, mode="1")
    return mask.tobytes()


def convert(input_pdf: Path, output_cgv: Path, max_side: int, threshold: int) -> None:
    pdf = fitz.open(input_pdf)
    page_count = pdf.page_count

    if page_count < 1:
        raise ValueError("PDF has no pages")
    if page_count > 65535:
        raise ValueError("PDF has too many pages for CGV v1")

    print(f"PDF detected: {page_count} page(s). ALL pages will be converted.", flush=True)

    index_offset = HEADER_SIZE
    records: list[tuple[int, int, int, int, int, int]] = []

    output_cgv.parent.mkdir(parents=True, exist_ok=True)

    with output_cgv.open("wb+") as out:
        out.write(MAGIC)
        out.write(struct.pack(">HHI", page_count, 0, index_offset))
        out.write(b"\0" * (page_count * INDEX_RECORD_SIZE))

        for i in range(page_count):
            page = pdf.load_page(i)
            print(f"[{i + 1}/{page_count}] rendering...", flush=True)

            fit_img = render_fit(page)
            fit_data = pack_black_bits(fit_img, threshold)
            if len(fit_data) != FIT_SIZE:
                raise RuntimeError(f"Unexpected fit bitmap size: {len(fit_data)}")
            fit_offset = out.tell()
            out.write(fit_data)

            zoom_img = render_zoom(page, max_side)
            zoom_data = pack_black_bits(zoom_img, threshold)
            zoom_offset = out.tell()
            out.write(zoom_data)

            expected_zoom = ((zoom_img.width + 7) // 8) * zoom_img.height
            if len(zoom_data) != expected_zoom:
                raise RuntimeError(
                    f"Unexpected zoom bitmap size: {len(zoom_data)} != {expected_zoom}"
                )
            if len(zoom_data) > MAX_ZOOM_BYTES:
                raise ValueError(
                    f"Page {i + 1} needs {len(zoom_data)} bytes; lower --max-side"
                )

            records.append((
                zoom_img.width,
                zoom_img.height,
                zoom_offset,
                len(zoom_data),
                fit_offset,
                len(fit_data),
            ))

        out.seek(index_offset)
        for rec in records:
            out.write(struct.pack(">HHIIII", *rec))

    mib = output_cgv.stat().st_size / (1024 * 1024)
    print()
    print(f"DONE: {output_cgv}")
    print(f"Converted pages: {page_count}/{page_count}")
    print(f"Detailed bitmap max side: {max_side}px")
    print(f"CGV size: {mib:.2f} MiB")
    print("Copy it to the calculator root and name it PDFVIEW.CGV")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert every page of a PDF for PDFView on fx-CG50."
    )
    parser.add_argument("pdf", type=Path, help="Input PDF")
    parser.add_argument(
        "-o", "--output", type=Path, default=Path("PDFVIEW.CGV"),
        help="Output CGV file (default: PDFVIEW.CGV)",
    )
    parser.add_argument(
        "--max-side", type=int, default=1024,
        help="Detailed bitmap maximum side in pixels (default: 1024)",
    )
    parser.add_argument(
        "--threshold", type=int, default=190,
        help="Black/white threshold 0-255 (default: 190)",
    )
    args = parser.parse_args()

    if not args.pdf.exists():
        parser.error(f"Input PDF does not exist: {args.pdf}")
    if not 320 <= args.max_side <= 1024:
        parser.error("--max-side must be between 320 and 1024")
    if not 1 <= args.threshold <= 254:
        parser.error("--threshold must be between 1 and 254")

    try:
        convert(args.pdf, args.output, args.max_side, args.threshold)
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

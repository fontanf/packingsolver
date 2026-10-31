"""Figures of the detailed description of the bar relaxation of rectangle.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangle_bar_relaxation.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drawio
from drawio import BLUE, RED, GREEN

DRAWIO_DIR = "doc/internals"
PNG_DIR = "doc/img"
PREFIX = "rectangle_bar_relaxation_"

W, H = 6, 4
ITEMS = [("A", 0, 0, 4, 2, BLUE), ("B", 4, 0, 2, 3, RED), ("C", 0, 2, 3, 2, GREEN)]


def figure_bars():
    """A packing, and the row-bars and column-bars it gives."""
    fig = drawio.Figure(unit=30)
    for name, x, y, w, h, style in ITEMS:
        fig.rect(x, y, w, h, name, style)
    for r in range(1, H):
        fig.polyline([(0, r), (W, r)], "strokeColor=#999999;dashed=1;")
    for c in range(1, W):
        fig.polyline([(c, 0), (c, H)], "strokeColor=#999999;dashed=1;")
    fig.bin(0, 0, W, H)
    fig.text(W / 2, -0.6, "packing", w=W)
    # Row-bars: one-dimensional bins of capacity W, one per unit of height.
    ox = W + 1.5
    for r in range(H):
        x = 0
        for name, ix, iy, w, h, style in sorted(ITEMS, key=lambda i: i[1]):
            if iy <= r < iy + h:
                fig.rect(ox + x, r + 0.1, w, 0.8, name, style)
                x += w
        fig.rect(ox, r + 0.1, W, 0.8, "", "fillColor=none;strokeColor=#000000;strokeWidth=2;")
    fig.text(ox + W / 2, -0.6, "row-bars: capacity {}".format(W), w=W + 2)
    # Column-bars: one-dimensional bins of capacity H, one per unit of width.
    ox = 2 * W + 3
    for c in range(W):
        y = 0
        for name, ix, iy, w, h, style in sorted(ITEMS, key=lambda i: i[2]):
            if ix <= c < ix + w:
                fig.rect(ox + c + 0.1, y, 0.8, h, name, style)
                y += h
        fig.rect(ox + c + 0.1, 0, 0.8, H, "", "fillColor=none;strokeColor=#000000;strokeWidth=2;")
    fig.text(ox + W / 2, -0.6, "column-bars: capacity {}".format(H), w=W + 2)
    fig.write(DRAWIO_DIR + "/" + PREFIX + "bars.drawio",
              PNG_DIR + "/" + PREFIX + "bars.png")


if __name__ == "__main__":
    figure_bars()

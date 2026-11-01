"""Figures of the detailed description of the tree search on relative
positions of irregular.

Usage, from the root of the repository:

    python3 scripts/doc_figures/irregular_tree_search_relative_positions.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, GREEN, YELLOW, NONE

SVG_DIR = "doc/img"
PREFIX = "irregular_tree_search_relative_positions_"

DARK_GRAY = "fillColor=#cccccc;strokeColor=#666666;"
DOT = "fillColor=#000000;strokeColor=#000000;ellipse;"
RED_STROKE = "#b85450"


def dot(fig, x, y, r=0.08):
    fig.rect(x - r, y - r, 2 * r, 2 * r, "", DOT)


def u_shape(x, y):
    """Item i: a 4 x 3 rectangle with a 1 x 2 notch on its top side."""
    return [
        (x, y), (x + 4, y), (x + 4, y + 3), (x + 2.5, y + 3),
        (x + 2.5, y + 1), (x + 1.5, y + 1), (x + 1.5, y + 3), (x, y + 3)]


def figure_pieces():
    """No-fit polygon of a U-shaped item and of a 1 x 2 rectangle, and the
    convex pieces of the region outside of it.

    Bin 8 x 6. The inner-fit rectangles are [0, 4] x [0, 3] for the U and
    [0, 7] x [0, 4] for the rectangle, so R = [-4, 7] x [-3, 4]. The no-fit
    polygon is [-1, 4] x [-2, 3], with a slit at x = 1.5, 1 <= y <= 3: the
    rectangle slides exactly into the notch.
    """
    fig = figure.Figure(unit=28)

    # Left: a packing in which the rectangle is exactly in the notch.
    fig.text(4, 7, "positions in the bin", w=8)
    fig.bin(0, 0, 8, 6)
    pi = (1, 1)
    pj = (2.5, 2)
    fig.polygon(u_shape(*pi), "i", BLUE, label_position=(pi[0] + 0.75, pi[1] + 1.8))
    fig.rect(pj[0], pj[1], 1, 2, "j", GREEN)
    dot(fig, *pi)
    dot(fig, *pj)
    fig.text(pi[0] - 0.2, pi[1] - 0.35, "p_i", w=1, size=11, align="right")
    fig.text(pj[0] + 0.15, pj[1] - 0.35, "p_j", w=1, size=11, align="left")

    # Right: the relative positions d = p_j - p_i.
    ox, oy = 15, 3
    fig.text(ox + 1.5, oy + 5, "relative positions d = p_j - p_i", w=11)
    # Convex pieces of F = R \ int(NFP).
    pieces = [
        ((-4, -3), 3, 7, "F1"),
        ((4, -3), 3, 7, "F2"),
        ((-1, -3), 5, 1, "F3"),
        ((-1, 3), 5, 1, "F4"),
    ]
    for (x, y), w, h, label in pieces:
        fig.rect(ox + x, oy + y, w, h, label, GREEN)
    # No-fit polygon.
    fig.rect(ox - 1, oy - 2, 5, 5, "", DARK_GRAY)
    fig.text(ox + 3.0, oy - 1.3, "NFP", w=2, size=13)
    # R.
    fig.rect(ox - 4, oy - 3, 11, 7, "", NONE + "strokeWidth=2;dashed=1;")
    fig.text(ox + 7.2, oy + 3.7, "R", w=1, size=13, align="left")
    # Exact fit: the slit of the no-fit polygon.
    fig.polyline([(ox + 1.5, oy + 1), (ox + 1.5, oy + 3)],
                 "strokeColor={};strokeWidth=4;".format(RED_STROKE))
    fig.polyline([(ox + 2.35, oy + 2.2), (ox + 1.6, oy + 2.2)],
                 "strokeColor={};".format(RED_STROKE))
    fig.text(ox + 2.4, oy + 2.2, "exact fit", w=3, size=11, align="left",
             extra="fontColor={};".format(RED_STROKE))
    # The relative position of the packing on the left.
    dot(fig, ox + pj[0] - pi[0], oy + pj[1] - pi[1])
    fig.text(ox + 1.3, oy + 0.75, "d", w=1, size=11, align="right")
    # Origin.
    dot(fig, ox, oy, r=0.06)
    fig.text(ox - 0.15, oy - 0.3, "0", w=1, size=11, align="right")

    fig.write(os.path.join(SVG_DIR, PREFIX + "pieces.svg"))


def figure_core_widening():
    """Core of an item: a horizontal band across the bin which no other item
    can reach."""
    fig = figure.Figure(unit=30)
    W, H = 10, 5
    core = 1.5

    fig.rect(0, 0, W, core, "", YELLOW + "dashed=1;opacity=60;")
    fig.text(W - 0.2, core / 2, "core of i", w=3, size=11, align="right")
    fig.bin(0, 0, W, H)
    fig.polygon([(0.5, 0), (4, 0), (4, 1.5), (2, 1.5), (2, 3.5), (0.5, 3.5)], "i", BLUE,
                label_position=(1.25, 2.5))
    fig.polygon([(4.5, 1.8), (7, 1.8), (7, 4.6), (5.5, 4.6), (5.5, 3), (4.5, 3)], "j", GREEN,
                label_position=(6.25, 3.2))
    fig.polygon([(7.5, 1.6), (9.8, 1.6), (9.8, 4.8)], "k", GREEN, label_position=(9.1, 2.4))
    # Height of the core.
    fig.polyline([(-0.4, 0), (-0.4, core)])
    fig.polyline([(-0.55, 0), (-0.25, 0)])
    fig.polyline([(-0.55, core), (-0.25, core)])
    fig.text(-0.6, core / 2, "core\nheight", w=2, size=11, align="right")
    # Width of the bin.
    fig.polyline([(0, -0.4), (W, -0.4)])
    fig.polyline([(0, -0.55), (0, -0.25)])
    fig.polyline([(W, -0.55), (W, -0.25)])
    fig.text(W / 2, -0.75, "W", w=1, size=11)

    fig.write(os.path.join(SVG_DIR, PREFIX + "core_widening.svg"))


if __name__ == "__main__":
    figure_pieces()
    figure_core_widening()

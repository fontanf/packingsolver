"""Figures of the detailed description of the tree search of
rectangleguillotine.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangleguillotine_tree_search.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drawio
from drawio import BLUE, YELLOW

DRAWIO_DIR = "doc/internals"
PNG_DIR = "doc/img"
PREFIX = "rectangleguillotine_tree_search_"

W, H = 10, 6

CUT_1 = "strokeColor=#b85450;strokeWidth=3;"
CUT_2 = "strokeColor=#6c8ebf;strokeWidth=3;"
CUT_3 = "strokeColor=#82b366;strokeWidth=3;"


def current_area_outline(x1_prev, x1_curr, y2_prev, y2_curr, x3_curr):
    """Outline of the area 'consumed' by a node: the bins before, the closed
    1-level sub-plates, the closed 2-level sub-plates of the current 1-level
    sub-plate and the closed 3-level sub-plates of the current 2-level
    sub-plate."""
    return [(x1_prev, H), (x1_prev, y2_curr), (x3_curr, y2_curr), (x3_curr, y2_prev),
            (x1_curr, y2_prev), (x1_curr, 0)]


def figure_front():
    fig = drawio.Figure(unit=32)
    x1_prev, x1_curr, y2_prev, y2_curr, x3_curr = 4, 8, 2, 4, 6
    # Current area.
    fig.rect(0, 0, x1_prev, H, "", YELLOW + "strokeColor=none;")
    fig.rect(x1_prev, 0, x1_curr - x1_prev, y2_prev, "", YELLOW + "strokeColor=none;")
    fig.rect(x1_prev, y2_prev, x3_curr - x1_prev, y2_curr - y2_prev, "", YELLOW + "strokeColor=none;")
    # Items.
    items = [("A", 0, 0, 4, 3), ("B", 0, 3, 2, 2), ("C", 2, 3, 2, 1),
             ("D", 4, 0, 3, 2), ("E", 4, 2, 2, 2)]
    for name, x, y, w, h in items:
        fig.rect(x, y, w, h, name, BLUE)
    # Cuts.
    fig.polyline([(4, 0), (4, H)], CUT_1)
    fig.polyline([(x1_curr, 0), (x1_curr, H)], CUT_1 + "dashed=1;")
    fig.polyline([(0, 3), (4, 3)], CUT_2)
    fig.polyline([(0, 5), (4, 5)], CUT_2)
    fig.polyline([(4, y2_prev), (x1_curr, y2_prev)], CUT_2)
    fig.polyline([(4, y2_curr), (x1_curr, y2_curr)], CUT_2 + "dashed=1;")
    fig.polyline([(2, 3), (2, 5)], CUT_3)
    fig.polyline([(x3_curr, y2_prev), (x3_curr, y2_curr)], CUT_3 + "dashed=1;")
    fig.bin(0, 0, W, H)
    # Labels of the front.
    for x, label in ((x1_prev, "x1_prev"), (x3_curr, "x3_curr"), (x1_curr, "x1_curr")):
        fig.text(x, -0.4, label, w=2, size=11)
    for y, label in ((y2_prev, "y2_prev"), (y2_curr, "y2_curr")):
        fig.text(W + 0.2, y, label, w=2, size=11, align="left")
        fig.polyline([(x1_curr, y), (W + 0.1, y)], "strokeColor=#999999;dashed=1;")
    fig.text(W / 2, -1.1, "red: 1-cuts, blue: 2-cuts, green: 3-cuts, dashed: current cuts", w=W + 4, size=12)
    fig.text(W / 2, -1.7, "yellow: current area", w=W + 4, size=12)
    fig.write(DRAWIO_DIR + "/" + PREFIX + "front.drawio",
              PNG_DIR + "/" + PREFIX + "front.png")


def figure_dominance():
    fig = drawio.Figure(unit=32)
    f1 = (4, 8, 2, 4, 5)
    f2 = (4, 8, 2, 5, 6)
    fig.bin(0, 0, W, H)
    fig.polyline(current_area_outline(*f2), "strokeColor=#b85450;strokeWidth=5;")
    fig.polyline(current_area_outline(*f1), "strokeColor=#82b366;strokeWidth=3;")
    fig.text(W / 2, -0.5, "green: front of node 1; red: front of node 2", w=W + 2, size=12)
    fig.text(W / 2, -1.1, "node 1 dominates node 2", w=W + 2, size=12)
    fig.write(DRAWIO_DIR + "/" + PREFIX + "dominance.drawio",
              PNG_DIR + "/" + PREFIX + "dominance.png")


if __name__ == "__main__":
    figure_front()
    figure_dominance()

"""Figures of the detailed description of the tree search of
rectangleguillotine.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangleguillotine_tree_search.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, YELLOW

SVG_DIR = "doc/img"
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
    fig = figure.Figure(unit=32)
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
    fig.write(SVG_DIR + "/" + PREFIX + "front.svg")


def figure_dominance():
    fig = figure.Figure(unit=32)
    f1 = (4, 8, 2, 4, 5)
    f2 = (4, 8, 2, 5, 6)
    fig.bin(0, 0, W, H)
    fig.polyline(current_area_outline(*f2), "strokeColor=#b85450;strokeWidth=5;")
    fig.polyline(current_area_outline(*f1), "strokeColor=#82b366;strokeWidth=3;")
    fig.text(W / 2, -0.5, "green: front of node 1; red: front of node 2", w=W + 2, size=12)
    fig.text(W / 2, -1.1, "node 1 dominates node 2", w=W + 2, size=12)
    fig.write(SVG_DIR + "/" + PREFIX + "dominance.svg")


################################################################################
# Children of a node.
################################################################################

# Bin, item sizes, and the node of the example: the items inserted, each in a
# new third-level sub-plate of the given kind.
CHILDREN_BIN = (8, 6)
SIZES = {"1": (3, 2), "2": (2, 1.5), "3": (4, 2), "4": (2.5, 3)}
KINDS = {
    "same 2": "in the current second-level sub-plate",
    "new 2": "in a new second-level sub-plate",
    "new 1": "in a new first-level sub-plate",
}


def insert(plates, item, kind):
    """First-level sub-plates (x0, x1, second-level sub-plates (y0, y1,
    items (name, x, y)))) after inserting 'item' in a new third-level
    sub-plate of the given kind, as for the non-exact cuts: the current
    sub-plates grow to contain the new item. Returns None if the item sticks
    out of the bin."""
    bw, bh = CHILDREN_BIN
    w, h = SIZES[item]
    plates = [(x0, x1, [(y0, y1, list(items)) for y0, y1, items in rows]) for x0, x1, rows in plates]
    if kind == "new 1" or not plates:
        x = plates[-1][1] if plates else 0
        plates.append((x, x + w, [(0, h, [(item, x, 0)])]))
    else:
        x0, x1, rows = plates[-1]
        if kind == "new 2":
            y = rows[-1][1]
            rows.append((y, y + h, [(item, x0, y)]))
            x1 = max(x1, x0 + w)
        else:
            y0, y1, items = rows[-1]
            x = max(ix + SIZES[name][0] for name, ix, _ in items)
            items.append((item, x, y0))
            rows[-1] = (y0, max(y1, y0 + h), items)
            x1 = max(x1, x + w)
        plates[-1] = (x0, x1, rows)
    if plates[-1][1] > bw or plates[-1][2][-1][1] > bh:
        return None
    return plates


def draw_node(fig, plates, ox, oy, new=None):
    bw, bh = CHILDREN_BIN
    for x0, x1, rows in plates:
        fig.rect(ox + x0, oy, x1 - x0, bh, "", "fillColor=none;strokeColor=#000000;dashed=1;")
        for y0, y1, items in rows:
            fig.rect(ox + x0, oy + y0, x1 - x0, y1 - y0, "", "fillColor=none;strokeColor=#000000;dashed=1;")
            for name, x, y in items:
                w, h = SIZES[name]
                fig.rect(ox + x, oy + y, w, h, name, RED if name == new else BLUE)
    fig.rect(ox, oy, bw, bh, "", "fillColor=none;strokeColor=#000000;strokeWidth=2;")


def figure_children():
    node = insert(insert([], "1", "new 1"), "2", "new 2")
    children = []
    for item in ("3", "4"):
        for kind in KINDS:
            child = insert(node, item, kind)
            if child is not None:
                children.append((item, kind, child))
    bw, bh = CHILDREN_BIN
    fig = figure.Figure(unit=20)
    columns = 3
    width = columns * (bw + 3) - 3
    draw_node(fig, node, (width - bw) / 2, 0)
    fig.big_arrow(width / 2, -0.5, length=2.5)
    for k, (item, kind, child) in enumerate(children):
        row, column = divmod(k, columns)
        ox, oy = column * (bw + 3), -10 - row * (bh + 3)
        draw_node(fig, child, ox, oy, new=item)
        fig.text(ox + bw / 2, oy - 0.7, "{} {}".format(item, KINDS[kind]), w=bw + 3, size=11)
    fig.write(SVG_DIR + "/" + PREFIX + "children.svg")


if __name__ == "__main__":
    figure_children()
    figure_front()
    figure_dominance()

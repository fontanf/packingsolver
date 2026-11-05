"""Figures of the detailed description of the tree search on packing classes
of rectangle.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangle_tree_search_packing_classes.py
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, GREEN, YELLOW

SVG_DIR = "doc/img"
PREFIX = "rectangle_tree_search_packing_classes_"

STYLES = {"A": BLUE, "B": RED, "C": GREEN, "D": YELLOW}

NODE_RADIUS = 0.35


def disjoint(a, b, axis):
    """'True' iff the projections of two items on an axis are disjoint."""
    return a[axis] + a[2 + axis] <= b[axis] or b[axis] + b[2 + axis] <= a[axis]


def draw_graph(fig, ox, oy, items, node_positions, axis, title):
    """Graph of the pairs of items disjoint along an axis, each edge oriented
    from the item which comes first along the axis."""
    for name, (px, py) in node_positions.items():
        fig.rect(
                ox + px - NODE_RADIUS, oy + py - NODE_RADIUS,
                2 * NODE_RADIUS, 2 * NODE_RADIUS,
                name, "ellipse;" + STYLES[name], extra="ellipse;")
    names = sorted(items)
    for i, name_1 in enumerate(names):
        for name_2 in names[i + 1:]:
            a, b = items[name_1], items[name_2]
            if not disjoint(a, b, axis):
                continue
            first, second = (name_1, name_2) if a[axis] < b[axis] else (name_2, name_1)
            x1, y1 = node_positions[first]
            x2, y2 = node_positions[second]
            d = math.hypot(x2 - x1, y2 - y1)
            ux, uy = (x2 - x1) / d, (y2 - y1) / d
            fig.arrow(
                    ox + x1 + ux * NODE_RADIUS, oy + y1 + uy * NODE_RADIUS,
                    ox + x2 - ux * NODE_RADIUS, oy + y2 - uy * NODE_RADIUS)
    fig.text(ox + 1, oy - 1.0, title, w=5)


def figure_packing_class():
    """A packing, and the graphs of the pairs of items disjoint along each
    axis, oriented by the positions of the items."""
    fig = figure.Figure(unit=36)
    W, H = 6, 4
    # Item: (x, y, width, height).
    items = {
        "A": (0, 0, 3, 2),
        "B": (3, 0, 3, 1),
        "C": (3, 1, 2, 3),
        "D": (0, 2, 2, 2),
    }
    for name, (x, y, w, h) in items.items():
        fig.rect(x, y, w, h, name, STYLES[name])
    fig.bin(0, 0, W, H)
    fig.text(W / 2, -0.6, "packing", w=W + 2)
    node_positions = {"A": (0, 2), "B": (2, 2), "C": (2, 0), "D": (0, 0)}
    draw_graph(fig, W + 1.5, 1, items, node_positions, 0, "disjoint along x")
    draw_graph(fig, W + 5.5, 1, items, node_positions, 1, "disjoint along y")
    fig.write(SVG_DIR + "/" + PREFIX + "packing_class.svg")


def figure_obstruction():
    """An obstruction: three items whose pairs are not known to overlap along
    x form a chain longer than the bin, so one of these pairs overlaps along
    x."""
    fig = figure.Figure(unit=36)
    W = 6
    widths = {"A": 3, "B": 2, "C": 2}
    x = 0
    for name, w in widths.items():
        fig.rect(x, 0, w, 1, name, STYLES[name])
        x += w
    fig.bin(0, 0, W, 1)
    fig.text(W / 2, -0.6, "A, B and C side by side: 7 > W = 6", w=W + 4)
    fig.write(SVG_DIR + "/" + PREFIX + "obstruction.svg")


if __name__ == "__main__":
    figure_packing_class()
    figure_obstruction()

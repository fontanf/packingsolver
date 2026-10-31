"""Figures of the description of the tree searches common to all the problem
types (doc/internals/tree_search.rst).

Usage, from the root of the repository:

    python3 scripts/doc_figures/tree_search.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drawio
from drawio import GREEN, GRAY

DRAWIO_DIR = "doc/internals"
PNG_DIR = "doc/img"
PREFIX = "tree_search_"

DISCARDED = GRAY + "dashed=1;"


def figure_beam_search():
    """Beam search with a queue of size 2: at each depth, only the 2 nodes
    with the smallest guides are kept."""
    fig = drawio.Figure(unit=30)
    bw, bh = 1.6, 0.7

    def node(x, y, label, style):
        fig.rect(x - bw / 2, y - bh / 2, bw, bh, label, style)

    root = (6, 0)
    node(*root, "root", GRAY)
    depth_1 = [(1.5, "1.30", False), (4.5, "1.05", True), (7.5, "1.10", True), (10.5, "1.25", False)]
    depth_2 = {
        4.5: [(2.5, "1.04", True), (4.5, "1.12", False)],
        7.5: [(6.5, "1.06", "dominated"), (8.5, "1.08", True), (10.5, "1.20", False)],
    }
    for x, g, kept in depth_1:
        fig.arrow(root[0], root[1] - bh / 2, x, -2 + bh / 2, "strokeColor=#666666;")
        node(x, -2, g, GREEN if kept else DISCARDED)
    for x_parent, children in depth_2.items():
        for x, g, kept in children:
            fig.arrow(x_parent, -2 - bh / 2, x, -4 + bh / 2, "strokeColor=#666666;")
            node(x, -4, g, GREEN if kept is True else DISCARDED)
            if kept == "dominated":
                fig.text(x, -4.8, "dominated", w=3, size=11)
    for k, text in enumerate(("depth 0", "depth 1", "depth 2")):
        fig.text(-1.5, -2 * k, text, w=2.5, size=12)
    fig.text(6, -5.8, "green: kept in the queue of size 2; gray: discarded; values: guides", w=14, size=12)
    fig.write(DRAWIO_DIR + "/" + PREFIX + "beam_search.drawio",
              PNG_DIR + "/" + PREFIX + "beam_search.png")


def figure_restarts():
    """Nodes kept by the successive beam searches on a small tree, whose
    nodes are ordered by guide from left to right at each depth."""
    fig = drawio.Figure(unit=24)
    r = 0.35
    # Tree: 3 children at depth 1, 2 children each at depth 2.
    depth_1 = [0, 1, 2]
    depth_2 = [(p, c) for p in depth_1 for c in range(2)]

    def x1(p):
        return 1.5 + 2.5 * p

    def x2(k):
        return 0.25 + 1.25 * k

    for panel, (w, title) in enumerate((
            (1, "w = 1"),
            (2, "w = 2"),
            (6, "w = 6: no queue full,"))):
        ox = panel * 8.5
        kept_1 = depth_1[:w]
        children = [k for k, (p, c) in enumerate(depth_2) if p in kept_1]
        kept_2 = children[:w]

        def circle(x, y, kept):
            style = (GREEN if kept else DISCARDED) + "ellipse;"
            fig.rect(ox + x - r, y - r, 2 * r, 2 * r, "", style, extra="ellipse;")

        circle(3.75, 0, True)
        for p in depth_1:
            fig.polyline([(ox + 3.75, -r), (ox + x1(p), -2 + r)], "strokeColor=#999999;")
            circle(x1(p), -2, p in kept_1)
        for k, (p, c) in enumerate(depth_2):
            if p not in kept_1:
                continue
            fig.polyline([(ox + x1(p), -2 - r), (ox + x2(k), -4 + r)], "strokeColor=#999999;")
            circle(x2(k), -4, k in kept_2)
        fig.text(ox + 3.75, -5.0, title, w=8, size=12)
        if w == 6:
            fig.text(ox + 3.75, -5.6, "the tree is explored completely", w=8, size=12)
    fig.write(DRAWIO_DIR + "/" + PREFIX + "restarts.drawio",
              PNG_DIR + "/" + PREFIX + "restarts.png")


if __name__ == "__main__":
    figure_beam_search()
    figure_restarts()

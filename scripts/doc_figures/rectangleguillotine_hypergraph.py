"""Figures of the detailed description of the tree searches hypergraph of
rectangleguillotine: the states and the hyperarcs of a pattern, the merge of
the labels at a hyperarc, and, for the tree search hypergraph with infinite
copies, two blocks combined side by side or one above the other.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangleguillotine_hypergraph.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, GREEN, YELLOW, GRAY

SVG_DIR = "doc/img"
PREFIX = "rectangleguillotine_"

# Item types (width, height); the profit of an item is its area.
ITEMS = {"1": (8, 6), "2": (4, 5), "3": (3, 4), "4": (7, 2)}


class Block:
    """Guillotine pattern with the tight bounding box of its items."""

    def __init__(self, w, h, items):
        self.w, self.h, self.items = w, h, items

    def profit(self):
        return sum(ITEMS[n][0] * ITEMS[n][1] for n, _, _ in self.items)


def item_block(name):
    w, h = ITEMS[name]
    return Block(w, h, [(name, 0, 0)])


def side_by_side(a, b):
    return Block(a.w + b.w, max(a.h, b.h), a.items + [(n, x + a.w, y) for n, x, y in b.items])


def above(a, b):
    """'b' above 'a'."""
    return Block(max(a.w, b.w), a.h + b.h, a.items + [(n, x, y + a.h) for n, x, y in b.items])


# The two parent blocks of the figures.
PARENT_1 = above(item_block("1"), item_block("4"))
PARENT_2 = above(side_by_side(item_block("2"), item_block("3")), item_block("4"))


def draw_block(fig, block, ox, oy, style):
    for name, x, y in block.items:
        w, h = ITEMS[name]
        fig.rect(ox + x, oy + y, w, h, name, style)
    fig.rect(ox, oy, block.w, block.h, "", "fillColor=none;strokeColor=#000000;")


def label(block):
    return "w = {}\nh = {}\np = {}".format(block.w, block.h, block.profit())


def figure_combinations(with_items, name):
    """The two parents, and their two children: side by side, and one above
    the other. With 'with_items', the blocks are drawn with their items;
    otherwise, as for the dynamic programming of the tree search hypergraph
    with infinite copies, only with their dimensions and profits."""
    fig = figure.Figure(unit=12)
    children = [(side_by_side(PARENT_1, PARENT_2), GREEN, "side by side"),
                (above(PARENT_1, PARENT_2), YELLOW, "one above the other")]

    def draw(block, ox, oy, style, parts=None):
        if with_items:
            if parts is None:
                draw_block(fig, block, ox, oy, style)
            else:
                (b1, x1, y1, s1), (b2, x2, y2, s2) = parts
                draw_block(fig, b1, ox + x1, oy + y1, s1)
                draw_block(fig, b2, ox + x2, oy + y2, s2)
                fig.rect(ox, oy, block.w, block.h, "", "fillColor=none;strokeColor=#000000;strokeWidth=2;")
        else:
            fig.rect(ox, oy, block.w, block.h, label(block), style)

    draw(PARENT_1, 0, 0, BLUE)
    draw(PARENT_2, PARENT_1.w + 4, 0, RED)
    width = PARENT_1.w + PARENT_2.w + 4
    fig.big_arrow(width / 2, -1, length=3)
    x = 0
    oy = -6
    for child, style, title in children:
        if child.w == PARENT_1.w + PARENT_2.w:
            parts = ((PARENT_1, 0, 0, BLUE), (PARENT_2, PARENT_1.w, 0, RED))
        else:
            parts = ((PARENT_1, 0, 0, BLUE), (PARENT_2, 0, PARENT_1.h, RED))
        draw(child, x, oy - child.h, style, parts)
        fig.text(x + child.w / 2, oy - child.h - 1.2, title, w=child.w + 6, size=11)
        x += child.w + 5
    fig.write(SVG_DIR + "/" + PREFIX + name + ".svg")


################################################################################
# Tree search hypergraph: states and hyperarcs of a 2-staged pattern.
################################################################################

BIN = (12, 8)
# A 2-staged pattern, vertical first stage: three strips.
PATTERN = [
    ("A", 0, 0, 5, 3),
    ("B", 0, 3, 5, 4),
    ("C", 5, 0, 4, 8),
    ("D", 9, 0, 3, 5),
]
OUTLINE = "fillColor=none;strokeColor=#000000;strokeWidth=2;"
DASHED = "fillColor=none;strokeColor=#000000;dashed=1;"


def draw_pattern(fig, ox, oy, styles=None):
    for name, x, y, w, h in PATTERN:
        style = BLUE if styles is None else styles[name]
        fig.rect(ox + x, oy + y, w, h, name, style)
    fig.rect(ox, oy, BIN[0], BIN[1], "", OUTLINE)


def figure_states():
    """The pattern, and two hyperarcs: the first strip of the part S of the
    bin to cut, and the first piece of the part of the first strip."""
    fig = figure.Figure(unit=14)
    draw_pattern(fig, 0, 0)
    fig.text(BIN[0] / 2, -1.2, "a 2-staged pattern", w=12, size=11)

    def state(x, y, w, h, label, style):
        fig.rect(x, y, w, h, "", style)
        fig.text(x + w / 2, y - 0.9, label, w=10, size=11)

    # S(12, 8, V, 2) <- N(5, 8, H, 1) + S(7, 8, V, 2).
    ox = 16
    state(ox, 0, 12, 8, "S(12, 8, V, 2)", GRAY)
    fig.big_arrow(ox + 6, -2.2, length=2.2)
    state(ox - 1, -14, 5, 8, "N(5, 8, H, 1)", BLUE)
    state(ox + 6, -14, 7, 8, "S(7, 8, V, 2)", GRAY)
    fig.text(ox + 6, -16.6, "first strip, and the rest", w=14, size=11)

    # S(5, 8, H, 1) <- N(5, 3, V, 0) + S(5, 5, H, 1).
    ox = 34
    state(ox, 0, 5, 8, "S(5, 8, H, 1)", GRAY)
    fig.big_arrow(ox + 2.5, -2.2, length=2.2)
    state(ox - 3.5, -14, 5, 3, "N(5, 3, V, 0)", BLUE)
    state(ox + 3.5, -14, 5, 5, "S(5, 5, H, 1)", GRAY)
    fig.text(ox + 2.5, -16.6, "first piece, and the rest", w=14, size=11)
    fig.write(SVG_DIR + "/" + PREFIX + "tree_search_hypergraph_states.svg")


def figure_merge():
    """The labels merged at the hyperarc S(5, 8) <- N(5, 3) + S(5, 5): a
    forward label of each source, and a backward label of the target."""
    fig = figure.Figure(unit=16)
    styles = {"A": BLUE, "B": GREEN, "C": RED, "D": RED}
    draw_pattern(fig, 0, 0, styles)
    fig.rect(0, 0, 5, 8, "", "fillColor=none;strokeColor=#000000;strokeWidth=3;dashed=1;")
    legend = [
        (BLUE, "forward label of N(5, 3): {A}"),
        (GREEN, "forward label of S(5, 5): {B}"),
        (RED, "backward label of S(5, 8) (dashed): {C, D}"),
    ]
    for k, (style, text) in enumerate(legend):
        y = BIN[1] - 1.5 - 2.2 * k
        fig.rect(BIN[0] + 1.5, y, 1, 1, "", style)
        fig.text(BIN[0] + 3, y + 0.5, text, w=16, align="left", size=11)
    fig.write(SVG_DIR + "/" + PREFIX + "tree_search_hypergraph_merge.svg")


if __name__ == "__main__":
    figure_states()
    figure_merge()
    figure_combinations(False, "tree_search_hypergraph_infinite_copies_combinations")

"""Figures of the detailed description of the algorithms of boxstacks.

Usage, from the root of the repository:

    python3 scripts/doc_figures/boxstacks.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, GREEN, YELLOW

SVG_DIR = "doc/img"
PREFIX = "boxstacks_"

ORANGE = "fillColor=#ffe6cc;strokeColor=#d79b00;"

# Offset of the projection of a unit along y (the depth).
DEPTH = 0.5


def box3d(fig, x, y, z, dx, dy, dz, label="", style=BLUE):
    """Box [x, x + dx] x [y, y + dy] x [z, z + dz] in an oblique projection:
    x to the right, z up, y to the back."""
    def p(a, b, c):
        return (a + DEPTH * b, c + DEPTH * b)
    front = [p(x, y, z), p(x + dx, y, z), p(x + dx, y, z + dz), p(x, y, z + dz)]
    top = [p(x, y, z + dz), p(x + dx, y, z + dz), p(x + dx, y + dy, z + dz), p(x, y + dy, z + dz)]
    right = [p(x + dx, y, z), p(x + dx, y + dy, z), p(x + dx, y + dy, z + dz), p(x + dx, y, z + dz)]
    fig.polygon(front, label, style)
    fig.polygon(top, "", style)
    fig.polygon(top, "", "fillColor=#000000;strokeColor=none;opacity=5;")
    fig.polygon(right, "", style)
    fig.polygon(right, "", "fillColor=#000000;strokeColor=none;opacity=10;")


################################################################################
# Sequential one-dimensional / rectangle.
################################################################################

# Item types: name, (x, y, z) dimensions, copies, style. A and B can be
# stacked together, C and D too.
ITEM_TYPES = {
    "A": ((1, 1, 1), 4, BLUE),
    "B": ((1, 1, 1.5), 1, GREEN),
    "C": ((1.5, 1, 0.75), 5, ORANGE),
    "D": ((1.5, 1, 1), 2, YELLOW),
}
BIN = (6, 2, 3.5)
# Stacks of the one-dimensional phase (from the bottom), and the positions of
# their footprints in the rectangle phase.
STACKS = [
    (["D", "D", "C"], (0, 1)),
    (["A", "B"], (1.5, 1)),
    (["A", "A", "A"], (0, 0)),
    (["C", "C", "C", "C"], (1, 0)),
]


def draw_stack(fig, items, x, z):
    """Stack of the given items (from the bottom), its bottom at height z of
    the figure."""
    for name in items:
        (dx, dy, dz), _, style = ITEM_TYPES[name]
        box3d(fig, x, 0, z, dx, dy, dz, name, style)
        z += dz


def figure_sequential_onedimensional_rectangle():
    fig = figure.Figure(unit=36)
    # The stacks contain all the items, within the height of the bin.
    for name, ((_, _, _), copies, _) in ITEM_TYPES.items():
        assert sum(items.count(name) for items, _ in STACKS) == copies
    for items, _ in STACKS:
        assert sum(ITEM_TYPES[name][0][2] for name in items) <= BIN[2]

    # Items.
    x = 0
    for name, ((dx, dy, dz), copies, style) in ITEM_TYPES.items():
        box3d(fig, x, 0, 0, dx, dy, dz, "{} x{}".format(name, copies), style)
        x += dx + 1.2
    width = x - 1.2 + DEPTH
    fig.text(width + 0.5, 0.75, "items", w=3, align="left", extra="fontStyle=1;")
    fig.big_arrow(width / 2, -0.4, length=1.4)

    # Stacks.
    y0 = -6
    x = 0
    for items, _ in sorted(STACKS, key=lambda s: s[0][0] != "A"):
        draw_stack(fig, items, x, y0)
        x += ITEM_TYPES[items[0]][0][0] + 1.2
    fig.text(width + 0.5, y0 + 1.5, "1. stacks: one-dimensional problem", w=7, align="left", extra="fontStyle=1;")
    fig.big_arrow(width / 2, y0 - 0.4, length=1.4)

    # Bin: back stacks first.
    y0 = -13
    bl, bw, bh = BIN
    for items, (x, y) in sorted(STACKS, key=lambda s: -s[1][1]):
        z = y0
        for name in items:
            (dx, dy, dz), _, style = ITEM_TYPES[name]
            box3d(fig, x, y, z, dx, dy, dz, name, style)
            z += dz
    # Edges of the bin.
    def p(a, b, c):
        return (a + DEPTH * b, y0 + c + DEPTH * b)
    style = "strokeColor=#000000;strokeWidth=2;"
    fig.polyline([p(0, 0, 0), p(bl, 0, 0), p(bl, 0, bh), p(0, 0, bh), p(0, 0, 0)], style)
    fig.polyline([p(0, 0, bh), p(0, bw, bh), p(bl, bw, bh), p(bl, 0, bh)], style)
    fig.polyline([p(bl, 0, 0), p(bl, bw, 0), p(bl, bw, bh)], style)
    fig.text(width + 0.5, y0 + 1.5, "2. footprints: rectangle problem", w=7, align="left", extra="fontStyle=1;")
    fig.write(SVG_DIR + "/" + PREFIX + "sequential_onedimensional_rectangle.svg")


################################################################################
# Tree search.
################################################################################

# Bin of the floor, and stacks (name, x, y, width, depth) of the node.
FLOOR = (8, 6)
NODE = [("1", 0, 0, 4, 3), ("2", 0, 3, 1.5, 2), ("3", 1.5, 3, 1.5, 2)]
# Item to insert, and the stack on which it can be stacked (same stackability
# id).
ITEM = ("4", 1.5, 2)
STACKABLE_ON = "3"


def new_stack_insertions(stacks, w, h):
    """Positions of an item in a new stack: its lower y on the side y = 0 of
    the bin or on the upper y of a stack, then pushed along x until it doesn't
    overlap the stacks; discarded if it sticks out of the bin or doesn't touch
    the stack it rests on."""
    result = []
    for below in [None] + stacks:
        y = 0 if below is None else below[2] + below[4]
        if y + h > FLOOR[1]:
            continue
        x = 0
        for _, sx, sy, sw, sh in stacks:
            if sy < y + h and y < sy + sh:
                x = max(x, sx + sw)
        if x + w > FLOOR[0]:
            continue
        if below is not None and x >= below[1] + below[3]:
            continue
        result.append((x, y))
    return result


def figure_tree_search():
    fig = figure.Figure(unit=20)
    bw, bh = FLOOR
    name, w, h = ITEM
    children = [("new stack", x, y) for x, y in new_stack_insertions(NODE, w, h)]
    for stack in NODE:
        if stack[0] == STACKABLE_ON:
            children.append(("stacked on " + stack[0], stack[1], stack[2]))
    width = len(children) * (bw + 2) - 2

    def draw_node(ox, oy):
        for label, x, y, sw, sh in NODE:
            fig.rect(ox + x, oy + y, sw, sh, label, BLUE)

    ox = (width - bw) / 2
    draw_node(ox, 0)
    fig.bin(ox, 0, bw, bh)
    fig.big_arrow(width / 2, -0.5, length=2.5)
    for k, (where, x, y) in enumerate(children):
        ox = k * (bw + 2)
        draw_node(ox, -10)
        fig.rect(ox + x, -10 + y, w, h, name, RED + "opacity=80;")
        fig.bin(ox, -10, bw, bh)
        fig.text(ox + bw / 2, -10.8, where, w=bw + 1, size=11)
    fig.write(SVG_DIR + "/" + PREFIX + "tree_search.svg")


if __name__ == "__main__":
    figure_sequential_onedimensional_rectangle()
    figure_tree_search()

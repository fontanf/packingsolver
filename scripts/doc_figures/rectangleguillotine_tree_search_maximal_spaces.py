"""Figures of the detailed description of the tree search with maximal spaces
of rectangleguillotine.

The blocks and the unused spaces are computed with the rules of
'src/rectangleguillotine/tree_search_maximal_spaces.cpp' (without cut
thickness).

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangleguillotine_tree_search_maximal_spaces.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, GREEN

SVG_DIR = "doc/img"
PREFIX = "rectangleguillotine_tree_search_maximal_spaces_"

BIN = (8, 6)
# Item types (width, height), one copy each.
ITEMS = {"1": (4, 3), "2": (2, 2.5), "3": (1.5, 2), "4": (3.5, 1)}
UNIT = 20


################################################################################
# Blocks.
################################################################################

class Block:
    """Guillotine pattern: its size and its items (name, x, y)."""

    def __init__(self, w, h, items):
        self.w, self.h, self.items = w, h, items

    def names(self):
        return [name for name, _, _ in self.items]


def item_block(name):
    w, h = ITEMS[name]
    return Block(w, h, [(name, 0, 0)])


def side_by_side(a, b):
    return Block(a.w + b.w, max(a.h, b.h),
                 a.items + [(n, x + a.w, y) for n, x, y in b.items])


def above(a, b):
    """'b' above 'a'."""
    return Block(max(a.w, b.w), a.h + b.h,
                 a.items + [(n, x, y + a.h) for n, x, y in b.items])


def blocks():
    """The blocks of the example: the items, then combinations of two items,
    then of a block of two items and an item."""
    single = {name: item_block(name) for name in ITEMS}
    two = [side_by_side(single["1"], single["2"]),
           above(single["1"], single["4"]),
           side_by_side(single["2"], single["3"])]
    three = [above(two[2], single["1"]),
             above(two[2], single["4"])]
    return [list(single.values()), two, three]


################################################################################
# Unused spaces.
################################################################################

class Space:

    def __init__(self, x, y, w, h, depth):
        self.x, self.y, self.w, self.h, self.depth = x, y, w, h, depth

    def area(self):
        return self.w * self.h


def place(space, block, vertical_first):
    """The two unused spaces after placing 'block' at the bottom-left corner
    of 'space'."""
    if vertical_first:
        right = Space(space.x + block.w, space.y, space.w - block.w, space.h, space.depth + 1)
        top = Space(space.x, space.y + block.h, block.w, space.h - block.h, space.depth + 2)
    else:
        top = Space(space.x, space.y + block.h, space.w, space.h - block.h, space.depth + 1)
        right = Space(space.x + block.w, space.y, space.w - block.w, block.h, space.depth + 2)
    return [s for s in (right, top) if s.w > 0 and s.h > 0]


def vertical_first(space, root_vertical):
    """Direction of the first cut in a space: alternating with its depth."""
    return root_vertical == (space.depth % 2 == 0)


def fits(block, space):
    return block.w <= space.w and block.h <= space.h


################################################################################
# Drawing.
################################################################################

def draw_block(fig, block, ox, oy):
    for name, x, y in block.items:
        w, h = ITEMS[name]
        fig.rect(ox + x, oy + y, w, h, name, BLUE)


def draw_node(fig, ox, oy, placed, spaces, selected=None):
    """Bin with the blocks placed (block, x, y) and the unused spaces."""
    bw, bh = BIN
    for space in spaces:
        style = GREEN if space is selected else "fillColor=none;strokeColor=#000000;"
        fig.rect(ox + space.x, oy + space.y, space.w, space.h, "", style)
    for block, x, y in placed:
        draw_block(fig, block, ox + x, oy + y)
    fig.rect(ox, oy, bw, bh, "", "fillColor=none;strokeColor=#000000;strokeWidth=2;")


def all_blocks():
    return [block for level in blocks() for block in level]


def root_children():
    """Children of the root: each block, with a vertical and with a
    horizontal first cut (orientation of the first stage free)."""
    root = Space(0, 0, BIN[0], BIN[1], 0)
    children = []
    for block in all_blocks():
        if not fits(block, root):
            continue
        for vertical in (True, False):
            children.append((block, vertical, place(root, block, vertical)))
    return children


################################################################################
# Figures.
################################################################################

def figure_blocks():
    fig = figure.Figure(unit=UNIT)
    y = 0
    for k, level in enumerate(blocks()):
        x = 0
        height = max(block.h for block in level)
        for block in level:
            draw_block(fig, block, x, y - height)
            x += block.w + 1.5
        y -= height + 1.5
        if k + 1 < len(blocks()):
            fig.big_arrow(6, y + 1.2, length=1)
    fig.write(SVG_DIR + "/" + PREFIX + "blocks.svg")


def figure_root_children():
    fig = figure.Figure(unit=UNIT)
    bw, bh = BIN
    columns = 4
    width = columns * (bw + 2) - 2
    draw_node(fig, (width - bw) / 2, 0, [], [])
    fig.big_arrow(width / 2, -0.5, length=2.5)
    for k, (block, vertical, spaces) in enumerate(root_children()):
        row, column = divmod(k, columns)
        draw_node(fig, column * (bw + 2), -10 - row * (bh + 2), [(block, 0, 0)], spaces)
    fig.write(SVG_DIR + "/" + PREFIX + "root_children.svg")


def figure_spaces():
    """The two unused spaces of the first child of the root."""
    fig = figure.Figure(unit=UNIT)
    bw, bh = BIN
    block, vertical, spaces = root_children()[0]
    for k, space in enumerate(spaces):
        draw_node(fig, k * (bw + 2), 0, [(block, 0, 0)], [space], selected=space)
        fig.text(k * (bw + 2) + bw / 2, -0.7, "area {:g}, depth {}".format(space.area(), space.depth),
                 w=bw + 2, size=11)
    fig.write(SVG_DIR + "/" + PREFIX + "spaces.svg")


def figure_children():
    """Children of the first child of the root: the space of smallest area is
    selected, and each remaining block fitting in it gives a child, with the
    direction of the first cut of the space."""
    fig = figure.Figure(unit=UNIT)
    bw, bh = BIN
    block, root_vertical, spaces = root_children()[0]
    selected = min(spaces, key=lambda s: (s.area(), s.y, s.x))
    others = [s for s in spaces if s is not selected]
    used = set(block.names())
    candidates = [b for b in all_blocks()
                  if not used & set(b.names()) and fits(b, selected)]
    columns = 4
    width = columns * (bw + 2) - 2
    draw_node(fig, (width - bw) / 2, 0, [(block, 0, 0)], spaces, selected=selected)
    fig.big_arrow(width / 2, -0.5, length=2.5)
    vertical = vertical_first(selected, root_vertical)
    for k, child_block in enumerate(candidates):
        row, column = divmod(k, columns)
        new_spaces = others + place(selected, child_block, vertical)
        draw_node(fig, column * (bw + 2), -10 - row * (bh + 2),
                  [(block, 0, 0), (child_block, selected.x, selected.y)], new_spaces)
    fig.write(SVG_DIR + "/" + PREFIX + "children.svg")


if __name__ == "__main__":
    figure_blocks()
    figure_root_children()
    figure_spaces()
    figure_children()

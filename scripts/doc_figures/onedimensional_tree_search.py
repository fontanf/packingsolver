"""Figures of the detailed description of the tree search of onedimensional.

The insertions and the nodes are computed by a minimal reimplementation of
the rules of 'BranchingScheme' (src/onedimensional/tree_search.cpp), so that
the figures are consistent with the algorithm. The bins are drawn vertically,
filled from the bottom, as in the other figures of onedimensional.

Usage, from the root of the repository:

    python3 scripts/doc_figures/onedimensional_tree_search.py
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, GREEN, YELLOW, GRAY

SVG_DIR = "doc/img"
PREFIX = "onedimensional_tree_search_"

INF = math.inf
BW = 2  # Width of a drawn bin.


class ItemType:

    def __init__(self, name, length, weight=0, nesting=0, stackability=INF,
                 weight_after=INF, style=BLUE):
        self.name, self.length, self.weight = name, length, weight
        self.nesting, self.stackability, self.weight_after = nesting, stackability, weight_after
        self.style = style


class Bin:
    """Current bin of a node: its items and the quantities of the checks."""

    def __init__(self, length, maximum_weight):
        self.capacity, self.maximum_weight = length, maximum_weight
        self.items = []  # (item type, position)
        self.length = 0
        self.weight = 0
        self.maximum_number_of_items = INF
        self.remaining_weight = INF

    def check(self, t):
        """First check of 'insertion_item_same_bin' violated by 't', or None."""
        if self.length + t.length - t.nesting > self.capacity:
            return "length"
        if self.weight + t.weight > self.maximum_weight:
            return "maximum weight"
        if len(self.items) + 1 > min(self.maximum_number_of_items, t.stackability):
            return "maximum stackability"
        if t.weight > self.remaining_weight:
            return "maximum weight after"
        return None

    def add(self, t):
        position = self.length - (t.nesting if self.items else 0)
        if self.items:
            self.length += t.length - t.nesting
            self.remaining_weight = min(self.remaining_weight - t.weight, t.weight_after)
        else:
            self.length = t.length
            self.remaining_weight = t.weight_after
        self.items.append((t, position))
        self.weight += t.weight
        self.maximum_number_of_items = min(self.maximum_number_of_items, t.stackability)


def draw_bin(fig, b, ox, oy, scale=0.4, new=None, new_style=GREEN):
    for t, position in b.items:
        fig.rect(ox, oy + position * scale, BW, t.length * scale, t.name, t.style)
    if new is not None:
        position = b.length - new.nesting
        fig.rect(ox, oy + position * scale, BW, new.length * scale, new.name, new_style)
    fig.rect(ox, oy, BW, b.capacity * scale, "", "fillColor=none;strokeColor=#000000;strokeWidth=2;")


def figure_tree():
    """Beginning of the tree: items A, B, C and D with a single copy each,
    in bins of length 8; each child packs one more item in the bin of its
    parent, the nodes down to depth 3."""
    small = "fontSize=10;"
    item_types = [
            ItemType("A", 5, style=BLUE + small),
            ItemType("B", 4, style=GREEN + small),
            ItemType("C", 1, style=RED + small),
            ItemType("D", 2, style=YELLOW + small)]
    bin_length = 8
    depth_max = 3
    scale = 0.6

    def children(sequence):
        """Children of the node packing the item types of 'sequence', all in
        the same bin since a new bin is only opened when no item fits."""
        b = Bin(bin_length, INF)
        for t in sequence:
            b.add(t)
        return [sequence + [t] for t in item_types
                if t not in sequence and b.check(t) is None]

    # Tree, and the x of each node: the leaves are spaced evenly, a parent is
    # centered above its children.
    positions = {}
    leaves = [0]

    def layout(sequence):
        key = tuple(t.name for t in sequence)
        if len(sequence) == depth_max:
            positions[key] = leaves[0]
            leaves[0] += BW + 0.6
            return
        kids = children(sequence)
        for kid in kids:
            layout(kid)
        positions[key] = sum(positions[tuple(t.name for t in kid)] for kid in kids) / len(kids)

    layout([])
    fig = figure.Figure(unit=20)
    height = bin_length * scale
    level = height + 2

    def draw(sequence):
        key = tuple(t.name for t in sequence)
        x, y = positions[key], -len(sequence) * level
        b = Bin(bin_length, INF)
        for t in sequence:
            b.add(t)
        draw_bin(fig, b, x, y, scale)
        if len(sequence) == depth_max:
            return
        for kid in children(sequence):
            kx = positions[tuple(t.name for t in kid)]
            middle = y - 1
            fig.polyline([(x + BW / 2, y), (x + BW / 2, middle), (kx + BW / 2, middle),
                          (kx + BW / 2, y - level + height)], arrow=True)
            draw(kid)

    draw([])
    fig.write(SVG_DIR + "/" + PREFIX + "tree.svg")


def figure_insertions():
    fig = figure.Figure(unit=24)
    P = ItemType("P", 4, weight=2, style=BLUE)
    Q = ItemType("Q", 3, weight=2, weight_after=3, style=BLUE)
    candidates = [
        ItemType("R", 4, weight=1),
        ItemType("S", 1, weight=7),
        ItemType("T", 1, weight=1, stackability=2),
        ItemType("U", 1, weight=4),
        ItemType("V", 3, weight=1, nesting=1),
    ]
    b = Bin(10, 10)
    b.add(P)
    b.add(Q)
    scale = 0.4
    draw_bin(fig, b, 0, 0, scale)
    fig.text(BW / 2, -0.6, "node: bin of length 10, max weight 10", w=8, size=11)
    fig.text(BW / 2, -1.1, "P: len 4, w 2", w=8, size=11)
    fig.text(BW / 2, -1.6, "Q: len 3, w 2, max weight after 3", w=8, size=11)
    for k, t in enumerate(candidates):
        ox = 7 + k * (BW + 2.4)
        reason = b.check(t)
        draw_bin(fig, b, ox, 0, scale, new=t,
                 new_style=(GREEN if reason is None else RED + "dashed=1;opacity=80;"))
        fig.text(ox + BW / 2, -0.6, "{}: len {}, w {}".format(t.name, t.length, t.weight), w=4, size=11)
        detail = []
        if t.stackability != INF:
            detail.append("stackability {}".format(t.stackability))
        if t.nesting:
            detail.append("nesting {}".format(t.nesting))
        if detail:
            fig.text(ox + BW / 2, -1.1, ", ".join(detail), w=4.4, size=11)
        fig.text(ox + BW / 2, -2.0, "valid" if reason is None else reason, w=4.4, size=11,
                 extra="fontStyle=1;")
    fig.write(SVG_DIR + "/" + PREFIX + "insertions.svg")


def figure_dominance():
    """Two nodes packing the same items, with the same last item: the one
    with the smallest current length dominates the other one."""
    fig = figure.Figure(unit=24)
    A = ItemType("A", 6, style=BLUE)
    B = ItemType("B", 5, style=GREEN)
    C = ItemType("C", 4, style=YELLOW)
    D = ItemType("D", 2, style=RED)
    scale = 0.4
    nodes = [
        ("node 1", [[A, C], [B, D]]),
        ("node 2", [[B, C], [A, D]]),
    ]
    for k, (title, bins) in enumerate(nodes):
        ox = k * 7
        current_length = 0
        for i, items in enumerate(bins):
            b = Bin(10, INF)
            for t in items:
                b.add(t)
            draw_bin(fig, b, ox + i * (BW + 0.6), 0, scale)
            current_length = 10 * i + b.length
        fig.text(ox + BW + 0.3, -0.6, title, w=5, size=12)
        fig.text(ox + BW + 0.3, -1.2, "current length {}".format(current_length), w=5, size=11)
    fig.write(SVG_DIR + "/" + PREFIX + "dominance.svg")


if __name__ == "__main__":
    figure_tree()
    figure_insertions()
    figure_dominance()

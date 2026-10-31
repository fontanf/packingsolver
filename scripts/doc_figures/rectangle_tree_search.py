"""Figures of the detailed description of the tree search of rectangle.

The skylines and the insertions are computed by a minimal reimplementation of
the rules of 'BranchingScheme' (src/rectangle/tree_search.cpp), so that the
figures are consistent with the algorithm.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangle_tree_search.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, GREEN, YELLOW, DEFECT

SVG_DIR = "doc/img"
PREFIX = "rectangle_tree_search_"

W, H = 10, 6  # Bin.


################################################################################
# Minimal reimplementation of the branching scheme.
################################################################################

class Segment:
    """Uncovered item: band [ys, ye] of the skyline, whose right side is xe."""

    def __init__(self, item, xs, xe, ys, ye):
        self.item, self.xs, self.xe, self.ys, self.ye = item, xs, xe, ys, ye


def new_bin_skyline():
    return [Segment(None, 0, 0, 0, H)]


def insertion(segments, w, h, pos=None, defects=(), defect=None):
    """Position (xs, ys) of an item of size w x h inserted above the segment
    'pos - 1' (pos = 0: on the floor of the bin), or above the defect
    'defect'. Return (None, reason) if the insertion is discarded."""
    if defect is not None:
        ys = defect[1] + defect[3]
    elif pos > 0:
        ys = segments[pos].ys
    else:
        ys = 0
    ye = ys + h
    if ye > H:
        return None, "sticks out of the bin"
    xs = 0
    for s in segments:
        if s.ye <= ys or s.ys >= ye:
            continue
        xs = max(xs, s.xe)
    moved = True
    while moved:
        moved = False
        for (dx, dy, dw, dh) in defects:
            if dx >= xs + w or xs >= dx + dw or dy >= ye or ys >= dy + dh:
                continue
            xs = dx + dw
            moved = True
    xe = xs + w
    if xe > W:
        return None, "sticks out of the bin"
    if defect is not None:
        if xe <= defect[0] or xs >= defect[0] + defect[2]:
            return None, "doesn't rest on the defect"
    elif pos > 0 and xe <= segments[pos - 1].xs:
        return None, "doesn't rest on " + segments[pos - 1].item
    return (xs, ys), None


def child(segments, item, w, h, xs, ys):
    """Skyline after inserting 'item' at (xs, ys), as in 'child_tmp'."""
    ye = ys + h
    new = Segment(item, xs, xs + w, ys, ye)
    result = []
    for s in segments:
        if s.ye <= ys:
            result.append(s)
        elif s.ys <= ys:
            if s.ys < ys:
                result.append(Segment(s.item, s.xs, s.xe, s.ys, ys))
            result.append(new)
            if s.ye > ye:
                result.append(Segment(s.item, s.xs, s.xe, ye, s.ye))
        elif s.ys >= ye:
            result.append(s)
        elif s.ye > ye:
            result.append(Segment(s.item, s.xs, s.xe, ye, s.ye))
    return result


class Node:

    def __init__(self):
        self.items = []  # (label, x, y, w, h)
        self.segments = new_bin_skyline()

    def insert(self, label, w, h, pos):
        position, reason = insertion(self.segments, w, h, pos)
        if position is None:
            raise ValueError(label + ": " + reason)
        node = Node()
        node.items = self.items + [(label, position[0], position[1], w, h)]
        node.segments = child(self.segments, label, w, h, *position)
        return node


SIZES = {"A": (4, 3), "B": (3, 2), "C": (2, 4), "D": (2, 1), "E": (3, 2), "F": (1, 1)}


def build(sequence):
    node = Node()
    for label, pos in sequence:
        w, h = SIZES[label]
        node = node.insert(label, w, h, pos)
    return node


# Node used in most figures: A on the floor, B on A, C on the floor (to the
# right of A, which it covers), D on B.
NODE = build([("A", 0), ("B", 1), ("C", 0), ("D", 2)])


################################################################################
# Drawing helpers.
################################################################################

def draw_items(fig, node, ox, oy, style=BLUE):
    for label, x, y, w, h in node.items:
        fig.rect(ox + x, oy + y, w, h, label, style)


def draw_skyline(fig, segments, ox, oy, color="#b85450", width=3, dashed=False):
    """Boundary of the area to the left of the skyline."""
    points = []
    for s in segments:
        points.append((ox + s.xe, oy + s.ys))
        points.append((ox + s.xe, oy + s.ye))
    style = "strokeColor={};strokeWidth={};".format(color, width)
    if dashed:
        style += "dashed=1;"
    fig.polyline(points, style)


################################################################################
# Figures.
################################################################################

def figure_skyline():
    fig = figure.Figure(unit=30)
    draw_items(fig, NODE, 0, 0)
    fig.bin(0, 0, W, H)
    draw_skyline(fig, NODE.segments, 0, 0)
    # Bands of the skyline.
    for s in NODE.segments:
        fig.polyline(
                [(s.xe, s.ye), (W + 0.3, s.ye)],
                "strokeColor=#999999;dashed=1;")
        item = s.item if s.item is not None else "none (floor of the bin)"
        fig.text(
                W + 0.5, (s.ys + s.ye) / 2,
                "y from {} to {}: item {}, right side at x = {}".format(
                    s.ys, s.ye, item, s.xe),
                w=9, align="left")
    fig.polyline([(0, 0), (W + 0.3, 0)], "strokeColor=#999999;dashed=1;")
    fig.write(SVG_DIR + "/" + PREFIX + "skyline.svg")


def figure_insertions():
    fig = figure.Figure(unit=18)
    panels = []
    for label, rotate in (("E", False), ("E", True)):
        w, h = SIZES[label]
        if rotate:
            w, h = h, w
        for pos in range(len(NODE.segments)):
            position, reason = insertion(NODE.segments, w, h, pos)
            ys = 0 if pos == 0 else NODE.segments[pos].ys
            panels.append((label + (" rotated" if rotate else ""), w, h, pos, ys, position, reason))
    w, h = SIZES["F"]
    position, reason = insertion(NODE.segments, w, h, 1)
    panels.append(("F", w, h, 1, NODE.segments[1].ys, position, reason))

    columns = 4
    for k, (label, w, h, pos, ys, position, reason) in enumerate(panels):
        ox = (k % columns) * (W + 2)
        oy = -(k // columns) * (H + 5.5)
        draw_items(fig, NODE, ox, oy)
        if position is not None:
            fig.rect(ox + position[0], oy + position[1], w, h, label[0], GREEN)
            status = "valid: x = {}, y = {}".format(*position)
        else:
            # Show where the item would be.
            xs = 0
            for s in NODE.segments:
                if s.ye <= ys or s.ys >= ys + h:
                    continue
                xs = max(xs, s.xe)
            fig.rect(ox + xs, oy + ys, w, h, label[0], RED + "dashed=1;opacity=80;")
            status = "discarded: " + reason
        fig.bin(ox, oy, W, H)
        where = "on the floor" if pos == 0 else "above " + NODE.segments[pos - 1].item
        fig.text(ox + W / 2, oy - 0.9, "{}, {}".format(label, where), w=W + 1, size=11)
        fig.text(ox + W / 2, oy - 1.9, status, w=W + 1, size=11)
    fig.write(SVG_DIR + "/" + PREFIX + "insertions.svg")


def figure_child():
    fig = figure.Figure(unit=24)
    w, h = SIZES["E"]
    position, _ = insertion(NODE.segments, w, h, 1)
    node = NODE.insert("E", w, h, 1)
    # Parent.
    draw_items(fig, NODE, 0, 0)
    fig.rect(position[0], position[1], w, h, "E", RED + "dashed=1;")
    fig.bin(0, 0, W, H)
    draw_skyline(fig, NODE.segments, 0, 0)
    fig.big_arrow(W + 0.8, H / 2, length=2, direction="right")
    # Child.
    ox = W + 4
    draw_items(fig, node, ox, 0)
    fig.bin(ox, 0, W, H)
    draw_skyline(fig, node.segments, ox, 0)
    for k, (n, ox) in enumerate(((NODE, 0), (node, W + 4))):
        lines = ["y {}-{}: {}, x = {}".format(
            s.ys, s.ye, s.item if s.item is not None else "floor", s.xe) for s in n.segments]
        for i, line in enumerate(lines):
            fig.text(ox + W / 2, -1 - 0.8 * i, line, w=W, size=11)
    fig.write(SVG_DIR + "/" + PREFIX + "child.svg")


def figure_defects():
    fig = figure.Figure(unit=24)
    defect = (3, 0, 2, 2)
    w, h = 4, 2
    segments = new_bin_skyline()
    # On the floor: shifted to the right of the defect.
    position, _ = insertion(segments, w, h, 0, defects=[defect])
    fig.rect(*defect, "defect", DEFECT)
    fig.rect(0, 0, w, h, "G", RED + "dashed=1;opacity=60;")
    fig.arrow(w / 2 + 0.5, h / 2 - 0.3, position[0] + w / 2 - 0.5, h / 2 - 0.3, "strokeColor=#b85450;strokeWidth=2;")
    fig.rect(position[0], position[1], w, h, "G", GREEN)
    fig.bin(0, 0, W, H)
    fig.text(W / 2, -0.8, "on the floor: shifted to x = {}".format(position[0]), w=W + 2)
    # Above the defect.
    ox = W + 2
    position, _ = insertion(segments, w, h, defects=[defect], defect=defect)
    fig.rect(ox + defect[0], defect[1], defect[2], defect[3], "defect", DEFECT)
    fig.rect(ox + position[0], position[1], w, h, "G", GREEN)
    fig.bin(ox, 0, W, H)
    fig.text(ox + W / 2, -0.8, "above the defect: x = {}, y = {}".format(*position), w=W + 2)
    fig.write(SVG_DIR + "/" + PREFIX + "defects.svg")


def figure_guide_area():
    fig = figure.Figure(unit=24)
    node = NODE
    xs_max = max(x for _, x, _, _, _ in node.items)
    item_area = sum(w * h for _, _, _, w, h in node.items)
    # Guide area: to the left of max(xs_max, skyline).
    guide_area = 0
    for s in node.segments:
        x = max(xs_max, s.xe)
        fig.rect(0, s.ys, x, s.ye - s.ys, "", YELLOW + "strokeColor=none;")
        guide_area += x * (s.ye - s.ys)
    draw_items(fig, node, 0, 0)
    fig.bin(0, 0, W, H)
    fig.polyline([(xs_max, 0), (xs_max, H + 0.4)], "strokeColor=#d6b656;strokeWidth=2;dashed=1;")
    fig.text(xs_max, H + 0.7, "rightmost left side of the items", w=8, size=11)
    fig.text(W / 2, -0.8, "guide area = {}, item area = {}".format(guide_area, item_area), w=W + 2)
    fig.text(W / 2, -1.6, "guide 0 = {} / {} = {:.2f}".format(guide_area, item_area, guide_area / item_area), w=W + 2)
    # Area to the left of the skyline, and waste.
    ox = W + 2
    current_area = 0
    for s in node.segments:
        fig.rect(ox, s.ys, s.xe, s.ye - s.ys, "", RED + "strokeColor=none;")
        current_area += s.xe * (s.ye - s.ys)
    draw_items(fig, node, ox, 0)
    fig.bin(ox, 0, W, H)
    draw_skyline(fig, node.segments, ox, 0)
    fig.text(ox + W / 2, -0.8, "area to the left of the skyline = {}".format(current_area), w=W + 2)
    fig.text(ox + W / 2, -1.6, "waste = {} - {} = {}".format(current_area, item_area, current_area - item_area), w=W + 2)
    fig.write(SVG_DIR + "/" + PREFIX + "guide_area.svg")


def figure_dominance():
    fig = figure.Figure(unit=24)
    # Same items (A and C), packed in a different order.
    node_1 = build([("C", 0), ("A", 0)])
    node_2 = build([("A", 0), ("C", 0)])
    for k, (node, title) in enumerate((
            (node_1, "node 1: C, then A"),
            (node_2, "node 2: A, then C"))):
        ox = k * (W + 2)
        draw_items(fig, node, ox, 0)
        fig.bin(ox, 0, W, H)
        draw_skyline(fig, node.segments, ox, 0, color="#82b366" if k == 0 else "#b85450")
        fig.text(ox + W / 2, -0.8, title, w=W + 2)
    # Both skylines.
    ox = 2 * (W + 2)
    fig.bin(ox, 0, W, H)
    draw_skyline(fig, node_2.segments, ox, 0, color="#b85450", width=5)
    draw_skyline(fig, node_1.segments, ox, 0, color="#82b366", width=3)
    fig.text(ox + W / 2, -0.8, "the skyline of node 1 is never", w=W + 2)
    fig.text(ox + W / 2, -1.5, "to the right of the one of node 2", w=W + 2)
    fig.write(SVG_DIR + "/" + PREFIX + "dominance.svg")


if __name__ == "__main__":
    figure_skyline()
    figure_insertions()
    figure_child()
    figure_defects()
    figure_guide_area()
    figure_dominance()

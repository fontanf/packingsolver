"""Figures of the detailed description of the tree search with maximal spaces
of rectangle.

The blocks, the maximal spaces and the insertions are computed by a minimal
reimplementation of the algorithm (rectangle_maximal_spaces_model.py), so that
the figures are consistent with it.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangle_tree_search_maximal_spaces.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, GREEN, YELLOW, GRAY
from rectangle_maximal_spaces_model import Instance, Node, compute_blocks, anchor

SVG_DIR = "doc/img"
PREFIX = "rectangle_tree_search_maximal_spaces_"

INSTANCE = Instance(10, 6, {"A": (4, 3, 2, 12), "B": (2, 2, 4, 4), "C": (3, 1, 3, 3)})
W, H = INSTANCE.W, INSTANCE.H
DROPPED = []
BLOCKS = compute_blocks(INSTANCE, DROPPED)

SPACE_COLORS = ["#d79b00", "#9673a6", "#6c8ebf", "#82b366", "#b85450"]


def find_block(copies, w=None, h=None):
    for b in BLOCKS:
        if b.copies() == copies and (w is None or b.w == w) and (h is None or b.h == h):
            return b
    raise ValueError(copies)


def draw_block(fig, block, x, y, style=BLUE, outline=True):
    for t, ix, iy in block.items:
        w, h = INSTANCE.size(t)
        fig.rect(x + ix, y + iy, w, h, t, style)
    if outline and len(block.items) > 1:
        fig.rect(x, y, block.w, block.h, "", "fillColor=none;strokeColor=#000000;strokeWidth=2;")


def draw_node(fig, node, ox, oy, style=BLUE):
    for b, x, y in node.placed:
        draw_block(fig, b, ox + x, oy + y, style)


def draw_spaces(fig, node, ox, oy):
    """Each maximal space of the node in its own small copy of the bin, below
    the node."""
    scale = 0.5
    k = 0
    for k, s in enumerate(node.spaces):
        sx = ox + k * (W * scale + 0.5)
        sy = oy - 2.2 - H * scale
        for b, x, y in node.placed:
            fig.rect(sx + x * scale, sy + y * scale, b.w * scale, b.h * scale, "", BLUE)
        fig.rect(sx + s.x * scale, sy + s.y * scale, s.w * scale, s.h * scale, "", YELLOW)
        fig.bin(sx, sy, W * scale, H * scale)


def figure_blocks():
    fig = figure.Figure(unit=12)
    x, y, row_h = 0, 0, 0
    max_w = 62
    for b in BLOCKS:
        if x + b.w > max_w:
            x, y = 0, y - row_h - 2
            row_h = 0
        draw_block(fig, b, x, y - b.h)
        x += b.w + 2
        row_h = max(row_h, b.h)
    y -= row_h + 4
    fig.text(max_w / 2, y + 1.2, "Examples of blocks dropped because they contain waste (at most 3 item types)", w=60, size=12)
    x = 0
    examples = [b for b in DROPPED if len(b.items) == 2][:4]
    for b in examples:
        fig.rect(x, y - b.h, b.w, b.h, "", GRAY + "fillStyle=hatch;")
        draw_block(fig, b, x, y - b.h)
        x += b.w + 3
    fig.write(SVG_DIR + "/" + PREFIX + "blocks.svg")


def figure_spaces():
    fig = figure.Figure(unit=24)
    a = find_block({"A": 1})
    root = Node(INSTANCE)
    node_1 = root.insert(BLOCKS, a, 0, 0)
    # (a) Maximal spaces after placing A.
    draw_node(fig, node_1, 0, 0)
    fig.bin(0, 0, W, H)
    draw_spaces(fig, node_1, 0, 0)
    fig.text(W / 2, -0.8, "after placing A: {} maximal spaces".format(len(node_1.spaces)), w=W + 2)
    # (b) Selected space and corner.
    ox = W + 2
    space = node_1.best_space(BLOCKS)
    d, dir_x, dir_y = anchor(space, W, H)
    draw_node(fig, node_1, ox, 0)
    fig.rect(ox + space.x, space.y, space.w, space.h, "", YELLOW + "opacity=60;")
    fig.bin(ox, 0, W, H)
    cx = space.x + (space.w if dir_x else 0)
    cy = space.y + (space.h if dir_y else 0)
    fig.rect(ox + cx - 0.25, cy - 0.25, 0.5, 0.5, "", "ellipse;fillColor=#b85450;strokeColor=#b85450;", extra="ellipse;")
    fig.text(W / 2 + ox, -0.8, "selected space and its corner", w=W + 2)
    # (c) A block placed in this corner, and the new maximal spaces.
    ox = 2 * (W + 2)
    b = find_block({"B": 3}, w=2, h=6)
    x, y = node_1.position(space, b)
    node_2 = node_1.insert(BLOCKS, b, x, y)
    draw_node(fig, node_2, ox, 0)
    fig.bin(ox, 0, W, H)
    draw_spaces(fig, node_2, ox, 0)
    fig.text(W / 2 + ox, -0.8, "after placing 3 B: {} maximal spaces".format(len(node_2.spaces)), w=W + 2)
    fig.write(SVG_DIR + "/" + PREFIX + "spaces.svg")
    return node_1


def figure_fitness(node_1):
    fig = figure.Figure(unit=20)
    insertions, space = node_1.insertions(BLOCKS)
    chosen = [insertions[0], insertions[1], insertions[3]]
    # Add the insertion illustrating the area-loss factor.
    b = find_block({"B": 1, "C": 2})
    f = node_1.fitness(space, b)
    chosen.append((f[0], b) + f[1:])
    for k, (fit, b, x, y, v, c, l, n) in enumerate(chosen):
        ox = k * (W + 2)
        draw_node(fig, node_1, ox, 0)
        fig.rect(ox + space.x, space.y, space.w, space.h, "", YELLOW + "opacity=50;strokeColor=none;")
        draw_block(fig, b, ox + x, y, GREEN)
        if l < 1:
            # Part of the space which can't be used anymore.
            fig.rect(ox + space.x, space.y, x - space.x, space.h, "", RED + "opacity=60;fillStyle=hatch;")
        fig.bin(ox, 0, W, H)
        # Edges in contact.
        for (x1, y1), (x2, y2) in node_1.contact_segments(space, b, x, y, 0.0139):
            fig.polyline([(ox + x1, y1), (ox + x2, y2)], "strokeColor=#b85450;strokeWidth=4;")
        fig.text(ox + W / 2, -0.9, "V = {}, C = {:.2f}, L = {:.2f}, N = {}".format(v, c, l, n), w=W + 2, size=11)
        fig.text(ox + W / 2, -1.8, "fitness = {:.2f}".format(fit), w=W + 2, size=11)
    fig.write(SVG_DIR + "/" + PREFIX + "fitness.svg")


def figure_greedy(node_1):
    fig = figure.Figure(unit=24)
    insertions, space = node_1.insertions(BLOCKS)
    for k, ins in enumerate(insertions[:3]):
        ox = k * (W + 2)
        b, x, y = ins[1], ins[2], ins[3]
        child = node_1.insert(BLOCKS, b, x, y)
        greedy = child.greedy(BLOCKS)
        draw_node(fig, node_1, ox, 0)
        draw_block(fig, b, ox + x, y, GREEN)
        for gb, gx, gy in greedy.placed[len(child.placed):]:
            draw_block(fig, gb, ox + gx, gy, GRAY + "dashed=1;")
        fig.bin(ox, 0, W, H)
        fig.text(ox + W / 2, -0.8, "child {}: profit {}, greedy value {}".format(
            k + 1, child.profit, greedy.profit), w=W + 3)
    fig.write(SVG_DIR + "/" + PREFIX + "greedy.svg")


if __name__ == "__main__":
    figure_blocks()
    node_1 = figure_spaces()
    figure_fitness(node_1)
    figure_greedy(node_1)

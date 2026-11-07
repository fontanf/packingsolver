"""Figures of the detailed description of the algorithms of box.

The boxes are drawn in an isometric view, seen from the end of the x-axis,
the beginning of the y-axis and above: the visible faces of a box are its
faces at its end along x, at its beginning along y, and at its top.

Usage, from the root of the repository:

    python3 scripts/doc_figures/box.py
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED

SVG_DIR = "doc/img"
PREFIX = "box_"

COS = math.cos(math.radians(30))
SIN = math.sin(math.radians(30))

# Colors of the faces of a box: top, end along x, beginning along y.
BOX_BLUE = ("#dae8fc", "#b4cbe8", "#c7d9f1", "#6c8ebf")
BOX_RED = ("#f8cecc", "#e0a8a5", "#ecbbb8", "#b85450")
BOX_GREEN = ("#d5e8d4", "#b0cfae", "#c2dcc1", "#82b366")
BOX_YELLOW = ("#fff2cc", "#ead9a3", "#f4e6b8", "#d6b656")
WALL = "fillColor=#f5f5f5;strokeColor=#999999;"


def project(x, y, z):
    return ((x + y) * COS, z + (y - x) * SIN)


def draw_bin(fig, ox, oy, size):
    """The back walls and the floor of the bin."""
    lx, ly, lz = size

    def face(points):
        fig.polygon([(ox + px, oy + py) for px, py in (project(*point) for point in points)], "", WALL)

    face([(0, 0, 0), (lx, 0, 0), (lx, ly, 0), (0, ly, 0)])
    face([(0, 0, 0), (0, ly, 0), (0, ly, lz), (0, 0, lz)])
    face([(0, ly, 0), (lx, ly, 0), (lx, ly, lz), (0, ly, lz)])


def behind(box_1, box_2):
    """Whether 'box_1' must be drawn before 'box_2'."""
    (x1, y1, z1, lx1, ly1, lz1), (x2, y2, z2, lx2, ly2, lz2) = box_1[0], box_2[0]
    return (x1 + lx1 <= x2
            or y1 >= y2 + ly2
            or z1 + lz1 <= z2)


def draw_boxes(fig, ox, oy, boxes):
    """Draw boxes ((x, y, z, lx, ly, lz), colors, opacity) from the back to
    the front."""
    remaining = list(boxes)
    ordered = []
    while remaining:
        for box in remaining:
            if not any(other is not box and behind(other, box) for other in remaining):
                break
        remaining.remove(box)
        ordered.append(box)
    for (x, y, z, lx, ly, lz), colors, opacity in ordered:
        top, end_x, start_y, stroke = colors
        style_end = ";opacity={};".format(opacity) if opacity is not None else ";"
        faces = [
            ([(x, y, z + lz), (x + lx, y, z + lz), (x + lx, y + ly, z + lz), (x, y + ly, z + lz)], top),
            ([(x + lx, y, z), (x + lx, y + ly, z), (x + lx, y + ly, z + lz), (x + lx, y, z + lz)], end_x),
            ([(x, y, z), (x + lx, y, z), (x + lx, y, z + lz), (x, y, z + lz)], start_y),
        ]
        for points, color in faces:
            fig.polygon(
                    [(ox + px, oy + py) for px, py in (project(*point) for point in points)],
                    "",
                    "fillColor={};strokeColor={}".format(color, stroke) + style_end)


def axes(fig, ox, oy, size):
    """Labels of the axes, along the edges of the bin."""
    lx, ly, lz = size
    for label, point in (
            ("x", (lx / 2, -1, 0)),
            ("y", (lx + 1, ly / 2, 0)),
            ("z", (0, -0.7, lz / 2))):
        x, y = project(*point)
        fig.text(ox + x, oy + y, label, w=0.6)


################################################################################
# Tree search: skyline.
################################################################################

TREE_BIN = (10, 6, 5)
# Items packed, pushed towards the origin along x, and the position of their
# label, on the face at their end along x.
TREE_ITEMS = [
    ("A", (0, 0, 0, 4, 3, 5), (1.5, 2.5)),
    ("B", (0, 3, 0, 3, 3, 2), (5.2, 1)),
    ("C", (0, 3, 2, 2, 3, 3), (4.5, 4.5)),
]
# Item inserted: it starts at the end of A along y and at the end of B along
# z, and is pushed along x until it doesn't overlap the skyline: it rests
# against C.
TREE_INSERTION = (2, 3, 2, 3, 3, 2)


def figure_tree_search():
    fig = figure.Figure(unit=20)
    # Node and insertion, in 3D.
    draw_bin(fig, 0, 0, TREE_BIN)
    boxes = [(box, BOX_BLUE, None) for _, box, _ in TREE_ITEMS]
    boxes.append((TREE_INSERTION, BOX_RED, None))
    draw_boxes(fig, 0, 0, boxes)
    for name, (x, y, z, lx, ly, lz), (label_y, label_z) in TREE_ITEMS:
        px, py = project(x + lx, label_y, label_z)
        fig.text(px, py, name, w=0.6)
    axes(fig, 0, 0, TREE_BIN)
    fig.text(5.5, -5.6, "the items packed, and an item inserted (in red)", w=16)

    # Skyline, in the yz-plane.
    ox = 16
    oy = -1.5
    _, ly, lz = TREE_BIN
    for name, (x, y, z, dx, dy, dz), _ in TREE_ITEMS:
        fig.rect(ox + y, oy + z, dy, dz, "{}: x = {}".format(name, x + dx), BLUE)
    _, y, z, _, dy, dz = TREE_INSERTION
    fig.rect(ox + y, oy + z, dy, dz, "", "fillColor=none;strokeColor=#b85450;dashed=1;strokeWidth=2;")
    fig.rect(ox, oy, ly, lz, "", "fillColor=none;strokeColor=#000000;strokeWidth=2;")
    fig.text(ox + ly / 2, oy - 0.6, "y", w=0.6)
    fig.text(ox - 0.6, oy + lz / 2, "z", w=0.6)
    fig.text(ox + ly / 2, -5.6, "the skyline, seen from the end of the x-axis", w=14)
    fig.write(SVG_DIR + "/" + PREFIX + "tree_search_skyline.svg")


################################################################################
# Tree search with maximal spaces: maximal spaces.
################################################################################

SPACES_BIN = (10, 6, 5)
SPACES_BLOCK = (0, 0, 0, 4, 3, 2)
# The maximal spaces after the block is placed in the empty bin: the parts of
# the bin after the block along x, along y and along z.
SPACES = [
    ("after the block along x", (4, 0, 0, 6, 6, 5)),
    ("after the block along y", (0, 3, 0, 10, 3, 5)),
    ("after the block along z", (0, 0, 2, 10, 6, 3)),
]


def figure_maximal_spaces():
    fig = figure.Figure(unit=14)
    step = 17
    for k, (title, space) in enumerate(SPACES):
        ox = k * step
        draw_bin(fig, ox, 0, SPACES_BIN)
        draw_boxes(fig, ox, 0, [(SPACES_BLOCK, BOX_BLUE, None)])
        draw_boxes(fig, ox, 0, [(space, BOX_YELLOW, 60)])
        fig.text(ox + 6, -6, title, w=12)
    fig.write(SVG_DIR + "/" + PREFIX + "maximal_spaces.svg")


if __name__ == "__main__":
    figure_tree_search()
    figure_maximal_spaces()

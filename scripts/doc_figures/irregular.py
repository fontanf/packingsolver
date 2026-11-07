"""Figures of the detailed description of the algorithms of irregular.

The shapes are drawn on a grid: polyominoes (sets of unit cells) and a
triangle.

Usage, from the root of the repository:

    python3 scripts/doc_figures/irregular.py
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, GREEN, YELLOW, WHITE

SVG_DIR = "doc/img"
PREFIX = "irregular_"

BIN_STYLE = "fillColor=none;strokeColor=#000000;strokeWidth=2;"


################################################################################
# Shapes.
################################################################################

def outline(cells):
    """Boundary of a polyomino, as a list of vertices."""
    # Directed edges of the cells, counterclockwise; the edges shared by two
    # cells cancel out.
    edges = set()
    for x, y in cells:
        for edge in [((x, y), (x + 1, y)), ((x + 1, y), (x + 1, y + 1)),
                     ((x + 1, y + 1), (x, y + 1)), ((x, y + 1), (x, y))]:
            reverse = (edge[1], edge[0])
            if reverse in edges:
                edges.remove(reverse)
            else:
                edges.add(edge)
    following = dict(edges)
    start = min(following)
    points = [start]
    while True:
        point = following[points[-1]]
        if point == start:
            break
        points.append(point)
    # Remove the vertices in the middle of a straight side.
    result = []
    for k, (x, y) in enumerate(points):
        (xp, yp), (xn, yn) = points[k - 1], points[(k + 1) % len(points)]
        if (x - xp) * (yn - y) - (y - yp) * (xn - x) != 0:
            result.append((x, y))
    return result


class Shape:

    def __init__(self, cells=None, polygon=None):
        self.polygon = outline(cells) if cells is not None else polygon

    def translated(self, dx, dy):
        return [(x + dx, y + dy) for x, y in self.polygon]

    def contains(self, px, py):
        """Whether the point is strictly inside the shape (ray casting)."""
        inside = False
        points = self.polygon
        for k in range(len(points)):
            (x1, y1), (x2, y2) = points[k - 1], points[k]
            if (y1 > py) != (y2 > py):
                if px < x1 + (py - y1) * (x2 - x1) / (y2 - y1):
                    inside = not inside
        return inside

    def raster(self, samples=8):
        """Cells covered by a part of the shape, however small, as in the
        rasterization of the MILP: the points of a fine grid inside each cell
        are tested."""
        xs = [x for x, _ in self.polygon]
        ys = [y for _, y in self.polygon]
        cells = set()
        for cx in range(int(min(xs)), int(max(xs)) + 1):
            for cy in range(int(min(ys)), int(max(ys)) + 1):
                for i in range(samples):
                    for j in range(samples):
                        if self.contains(cx + (i + 0.5) / samples, cy + (j + 0.5) / samples):
                            cells.add((cx, cy))
        return cells


T = Shape(cells=[(0, 0), (0, 1), (0, 2), (1, 1)])
T_DOWN = Shape(cells=[(0, 1), (1, 1), (2, 1), (1, 0)])
L = Shape(cells=[(0, 0), (1, 0), (2, 0), (2, 1)])
L_SMALL = Shape(cells=[(0, 1), (1, 1), (1, 0)])
CROSS = Shape(cells=[(1, 0), (0, 1), (1, 1), (2, 1), (1, 2)])
TRIANGLE = Shape(polygon=[(0, 0), (3, 0), (0, 3)])


def draw(fig, shape, ox, oy, x, y, style=BLUE):
    fig.polygon([(ox + px, oy + py) for px, py in shape.translated(x, y)], "", style)


################################################################################
# Tree search.
################################################################################

TREE_BIN = (8, 6)
TREE_NODE = [(T, 0, 0), (L, 1, 0)]
# Children: the item inserted and its position.
TREE_CHILDREN = [
    (CROSS, 0, 2), (CROSS, 1, 1), (CROSS, 3, 1), (CROSS, 4, 0),
    (TRIANGLE, 0, 3), (TRIANGLE, 1, 2), (TRIANGLE, 4, 0)]


def figure_tree_search():
    fig = figure.Figure(unit=16)
    bw, bh = TREE_BIN
    columns = 4
    width = columns * (bw + 2) - 2

    def node(ox, oy, child=None):
        for shape, x, y in TREE_NODE:
            draw(fig, shape, ox, oy, x, y)
        if child is not None:
            shape, x, y = child
            draw(fig, shape, ox, oy, x, y, RED)
        fig.rect(ox, oy, bw, bh, "", BIN_STYLE)

    node((width - bw) / 2, 0)
    fig.big_arrow(width / 2, -0.5, length=2.5)
    for k, child in enumerate(TREE_CHILDREN):
        row, column = divmod(k, columns)
        offset = (columns - (len(TREE_CHILDREN) - row * columns)) * (bw + 2) / 2 if row > 0 else 0
        node(offset + column * (bw + 2), -10 - row * (bh + 2), child)
    fig.write(SVG_DIR + "/" + PREFIX + "tree_search.svg")


################################################################################
# Local search.
################################################################################

LOCAL_SEARCH_BIN = (6, 4)
# Final, feasible, layout: (shape, x, y, style).
LOCAL_SEARCH_FEASIBLE = [
    (TRIANGLE, 0, 0, BLUE), (T_DOWN, 0, 2, BLUE), (CROSS, 2, 1, RED),
    (L, 3, 0, GREEN), (L_SMALL, 4, 2, BLUE)]
# Initial layout: the cross and the L overlap other items.
LOCAL_SEARCH_INFEASIBLE = [
    (TRIANGLE, 0, 0, BLUE), (T_DOWN, 0, 2, BLUE), (CROSS, 1, 1, RED),
    (L, 3, 1, GREEN), (L_SMALL, 4, 2, BLUE)]


def figure_local_search():
    fig = figure.Figure(unit=24)
    bw, bh = LOCAL_SEARCH_BIN
    for k, (layout, title, opacity) in enumerate((
            (LOCAL_SEARCH_INFEASIBLE, "infeasible layout: the items overlap", "opacity=70;"),
            (LOCAL_SEARCH_FEASIBLE, "the overlap is minimized: feasible layout", ""))):
        oy = -k * (bh + 4)
        for shape, x, y, style in layout:
            draw(fig, shape, 0, oy, x, y, style + opacity)
        fig.rect(0, oy, bw, bh, "", BIN_STYLE)
        fig.text(bw + 0.5, oy + bh / 2, title, w=9, align="left")
    fig.big_arrow(bw / 2, -0.4, length=2.8)
    fig.write(SVG_DIR + "/" + PREFIX + "local_search.svg")


################################################################################
# MILP raster.
################################################################################

RASTER_BIN = (6, 4)
RASTER_ITEMS = [TRIANGLE, CROSS, T, L]
RASTER_STYLES = [BLUE, RED, GREEN, YELLOW]
OUTLINE = "fillColor=none;strokeColor=#000000;strokeWidth=2;"


def place(rasters, bin_size):
    """Positions of the rasters in the bin, without two of them covering the
    same cell (depth-first search), or None."""
    bw, bh = bin_size
    used = set()
    positions = []

    def rec(k):
        if k == len(rasters):
            return True
        for x in range(bw):
            for y in range(bh):
                cells = {(cx + x, cy + y) for cx, cy in rasters[k]}
                if any(not (0 <= cx < bw and 0 <= cy < bh) for cx, cy in cells):
                    continue
                if cells & used:
                    continue
                used.update(cells)
                positions.append((x, y))
                if rec(k + 1):
                    return True
                used.difference_update(cells)
                positions.pop()
        return False

    return list(positions) if rec(0) else None


def draw_grid(fig, ox, oy, w, h):
    for x in range(1, w):
        fig.polyline([(ox + x, oy), (ox + x, oy + h)], "strokeColor=#999999;")
    for y in range(1, h):
        fig.polyline([(ox, oy + y), (ox + w, oy + y)], "strokeColor=#999999;")


def figure_milp_raster():
    fig = figure.Figure(unit=24)
    rasters = [shape.raster() for shape in RASTER_ITEMS]
    positions = place(rasters, RASTER_BIN)
    # Items.
    ox = 0
    for shape, style in zip(RASTER_ITEMS, RASTER_STYLES):
        draw(fig, shape, ox, 0, 0, 0, style)
        ox += max(x for x, _ in shape.polygon) + 1
    width = ox - 1
    fig.text(width + 0.5, 1.5, "items", w=6, align="left", extra="fontStyle=1;")
    fig.big_arrow(width / 2, -0.5, length=1.5)
    # Rasterized items: the cells covered, however little.
    ox, oy = 0, -5
    for shape, cells, style in zip(RASTER_ITEMS, rasters, RASTER_STYLES):
        for cx, cy in cells:
            fig.rect(ox + cx, oy + cy, 1, 1, "", style)
        draw(fig, shape, ox, oy, 0, 0, OUTLINE)
        ox += max(x for x, _ in shape.polygon) + 1
    fig.text(width + 0.5, oy + 1.5, "cells covered", w=6, align="left", extra="fontStyle=1;")
    fig.big_arrow(width / 2, oy - 0.5, length=1.5)
    # Placements in the bin.
    bw, bh = RASTER_BIN
    ox, oy = (width - bw) / 2, oy - 2.5 - bh
    draw_grid(fig, ox, oy, bw, bh)
    for shape, cells, (x, y), style in zip(RASTER_ITEMS, rasters, positions, RASTER_STYLES):
        for cx, cy in cells:
            fig.rect(ox + cx + x, oy + cy + y, 1, 1, "", style)
    for shape, (x, y) in zip(RASTER_ITEMS, positions):
        draw(fig, shape, ox, oy, x, y, OUTLINE)
    fig.rect(ox, oy, bw, bh, "", BIN_STYLE)
    fig.text(width + 0.5, oy + bh / 2, "placements in the bin\nwithout common cell", w=6, align="left",
             extra="fontStyle=1;")
    fig.write(SVG_DIR + "/" + PREFIX + "milp_raster.svg")


################################################################################
# Tree search: insertions.
################################################################################

SQUARE = Shape(cells=[(0, 0), (1, 0), (0, 1), (1, 1)])
INSERTIONS_TRIANGLE_HEIGHT = 3.5
INSERTIONS_TRIANGLE = Shape(polygon=[(0, 0), (3, 0), (0, INSERTIONS_TRIANGLE_HEIGHT)])
INSERTIONS_BIN = (8, 5)
CANDIDATE = RED
PREVIOUS_POSITION = "fillColor=none;strokeColor=#b85450;dashed=1;"
SUPPORTING_PART = "strokeColor=#82b366;strokeWidth=4;"
# Insertions of the root node: each item, with its bottom resting on the
# bottom of the bin, slid to the left side of the bin. There is a single copy
# of the triangle.
ROOT_INSERTIONS = [(INSERTIONS_TRIANGLE, 0, 0), (SQUARE, 0, 0), (L, 0, 0)]
# Abscissa of the point of the hypotenuse of the triangle at height 3: an
# item of height 2 resting there touches the top of the bin.
X_SLID_DOWN = 3 * (1 - 3 / INSERTIONS_TRIANGLE_HEIGHT)
# Insertions of the child node in which the triangle is packed: the
# insertions of the parent node slid to the right until they don't overlap
# the triangle (the one of the triangle is discarded: there is no copy left),
# and the new insertions on the supporting part of the triangle, its
# hypotenuse: put at its top, they stick out of the bin, and slide down the
# hypotenuse until they are below the top of the bin.
# (shape, x, y, previous position, new insertion, caption)
CHILD_INSERTIONS = [
    (SQUARE, 3, 0, (0, 0), False, "from the parent node,\nslid to the right"),
    (L, 3, 0, (0, 0), False, "from the parent node,\nslid to the right"),
    (SQUARE, X_SLID_DOWN, 3, (0, INSERTIONS_TRIANGLE_HEIGHT), True,
     "new, on the hypotenuse,\nslid down"),
    (L, X_SLID_DOWN, 3, (0, INSERTIONS_TRIANGLE_HEIGHT), True,
     "new, on the hypotenuse,\nslid down"),
]


def figure_tree_search_insertions():
    fig = figure.Figure(unit=16)
    bw, bh = INSERTIONS_BIN
    step = bw + 2

    def caption(ox, oy, label):
        fig.text(ox + bw / 2, oy - 1.3, label, w=bw + 1.5)

    # Root node.
    width = len(CHILD_INSERTIONS) * step - 2
    ox0 = (width - len(ROOT_INSERTIONS) * step + 2) / 2
    fig.text(width / 2, bh + 1.2, "Root node: one insertion per item and supported part",
             w=width, extra="fontStyle=1;")
    for k, (shape, x, y) in enumerate(ROOT_INSERTIONS):
        ox = ox0 + k * step
        draw(fig, shape, ox, 0, x, y, CANDIDATE)
        fig.rect(ox, 0, bw, bh, "", BIN_STYLE)
    fig.big_arrow(ox0 + bw / 2, -0.5, length=3.5)

    # Child node, in which the triangle is packed.
    oy = -(bh + 6)
    fig.text(width / 2, oy + bh + 1.2,
             "Child node, with the triangle packed: its insertions "
             "(the one of the triangle is discarded: no copy left)",
             w=width, extra="fontStyle=1;")
    for k, (shape, x, y, previous, new, label) in enumerate(CHILD_INSERTIONS):
        ox = k * step
        draw(fig, INSERTIONS_TRIANGLE, ox, oy, 0, 0, BLUE)
        if new:
            fig.polyline([(ox, oy + INSERTIONS_TRIANGLE_HEIGHT), (ox + 3, oy)], SUPPORTING_PART)
        draw(fig, shape, ox, oy, x, y, CANDIDATE)
        draw(fig, shape, ox, oy, previous[0], previous[1], PREVIOUS_POSITION)
        fig.rect(ox, oy, bw, bh, "", BIN_STYLE)
        caption(ox, oy, label)
    fig.write(SVG_DIR + "/" + PREFIX + "tree_search_insertions.svg")


################################################################################
# Tree search: slide of a supported part on a supporting part.
################################################################################

# Item already packed, whose top is a supporting part (from left to right),
# and item to insert, whose bottom is a supported part (from right to left).
SLIDE_SUPPORTING_ITEM = [(0, -1), (6, -1), (6, 0.5), (4, 2), (2, 2), (0, 1)]
SLIDE_SUPPORTING_PART = [(0, 1), (2, 2), (4, 2), (6, 0.5)]
SLIDE_ITEM = [(0, 1), (1, 0), (2, 0), (3, 0.6), (3, 2.2), (0, 2.2)]
SLIDE_SUPPORTED_PART = [(3, 0.6), (2, 0), (1, 0), (0, 1)]


def slide_path(supporting_part, supported_part):
    """Positions of the item along its slide, without obstacles, as in
    'slide_trapezoid': the item follows the larger of the slopes of the
    current edges of both parts."""
    def slope(a, b):
        return (b[1] - a[1]) / (b[0] - a[0])

    def sub(a, b):
        return (a[0] - b[0], a[1] - b[1])

    n, m = len(supporting_part) - 1, len(supported_part) - 1
    i = j = 0
    path = [sub(supporting_part[0], supported_part[0])]
    while i < n or j < m:
        if j == m:
            path.append(sub(supporting_part[i + 1], supported_part[-1]))
            i += 1
        elif i == n:
            path.append(sub(supporting_part[-1], supported_part[j + 1]))
            j += 1
        else:
            supporting_slope = slope(supporting_part[i], supporting_part[i + 1])
            supported_slope = slope(supported_part[j], supported_part[j + 1])
            if abs(supporting_slope - supported_slope) < 1e-9:
                path.append(sub(supporting_part[i + 1], supported_part[j + 1]))
                i += 1
                j += 1
            elif supporting_slope > supported_slope:
                path.append(sub(supporting_part[i + 1], supported_part[j]))
                i += 1
            else:
                path.append(sub(supporting_part[i], supported_part[j + 1]))
                j += 1
    return path


def figure_tree_search_slide():
    fig = figure.Figure(unit=20)
    path = slide_path(SLIDE_SUPPORTING_PART, SLIDE_SUPPORTED_PART)
    columns = 3
    step_x, step_y = 12, 7
    for k, (x, y) in enumerate(path):
        row, column = divmod(k, columns)
        ox, oy = column * step_x + 3, -row * step_y
        fig.polygon([(ox + px, oy + py) for px, py in SLIDE_SUPPORTING_ITEM], "", BLUE)
        fig.polyline([(ox + px, oy + py) for px, py in SLIDE_SUPPORTING_PART],
                     SUPPORTING_PART)
        fig.polygon([(ox + x + px, oy + y + py) for px, py in SLIDE_ITEM], "", RED + "opacity=85;")
        fig.polyline([(ox + x + px, oy + y + py) for px, py in SLIDE_SUPPORTED_PART],
                     "strokeColor=#b85450;strokeWidth=4;")
        # Path of the reference point of the item so far.
        if k > 0:
            fig.polyline([(ox + px, oy + py) for px, py in path[:k + 1]],
                         "strokeColor=#000000;dashed=1;")
        dot(fig, ox + x, oy + y)
        fig.text(ox + 1.5, oy - 1.8, "position {}".format(k + 1), w=4)
    fig.write(SVG_DIR + "/" + PREFIX + "tree_search_slide.svg")


################################################################################
# Periodic packings.
################################################################################

HEXAGON = Shape(polygon=[(1, 0), (3, 0), (4, 2), (3, 4), (1, 4), (0, 2)])
L_LONG = Shape(polygon=[(0, 0), (6, 0), (6, 1), (1, 1), (1, 5), (0, 5)])
RECTANGLE = Shape(polygon=[(0, 0), (5, 0), (5, 3), (0, 3)])


def rotated_180(shape):
    return Shape(polygon=[(-x, -y) for x, y in shape.polygon])


# The periodic packings computed by 'compute_periodic_packings' for these
# shapes: the items of the unit cell (shape, position, style) and the two
# vectors of the lattice, and the number of copies of the cell drawn along
# each vector.
PERIODIC_PACKINGS = [
    ("a single item, horizontal lattice",
     [(HEXAGON, 0, 0, RED)], (4, 0), (0, 4), 3),
    ("a single item, vertical lattice",
     [(HEXAGON, 0, 0, RED)], (0, 4), (3, 2), 3),
    ("two items, in rotations 0\u00b0 and 180\u00b0",
     [(TRIANGLE, 0, 0, RED), (rotated_180(TRIANGLE), 3, 3, YELLOW)], (3, 0), (0, 3), 3),
    ("two L-shapes, in rotations 0\u00b0 and 180\u00b0, wrapping around a rectangle",
     [(RECTANGLE, 0, 0, GREEN), (L_LONG, -1, -1, RED), (rotated_180(L_LONG), 6, 4, YELLOW)],
     (7, 0), (0, 5), 2),
]


def figure_periodic_packings():
    fig = figure.Figure(unit=12)
    columns = 2
    panel_width, panel_height = 17, 17
    for k, (title, cell, vector_1, vector_2, number_of_copies) in enumerate(PERIODIC_PACKINGS):
        row, column = divmod(k, columns)
        ox = column * (panel_width + 2)
        oy = -row * (panel_height + 4)
        # The copies of the unit cell, lighter, and the unit cell itself last,
        # on top.
        copies = [(n, m) for n in range(number_of_copies) for m in range(number_of_copies)]
        copies.sort(key=lambda copy: copy == (0, 0))
        for n, m in copies:
            dx = n * vector_1[0] + m * vector_2[0]
            dy = n * vector_1[1] + m * vector_2[1]
            for shape, x, y, style in cell:
                draw(fig, shape, ox, oy, x + dx, y + dy,
                     style if (n, m) == (0, 0) else style + "opacity=55;")
        # The vectors of the lattice, from the reference point of the first
        # item of the cell.
        _, x, y, _ = cell[0]
        for vector in (vector_1, vector_2):
            fig.arrow(ox + x, oy + y, ox + x + vector[0], oy + y + vector[1],
                      "strokeColor=#000000;strokeWidth=2;")
        fig.text(ox + panel_width / 2 - 2, oy - 2.5, title, w=panel_width)
    fig.write(SVG_DIR + "/" + PREFIX + "periodic_packings.svg")


################################################################################
# Nonlinear programming: separating line.
################################################################################

SEPARATION_P = [(0, 0), (3, 0), (3.5, 2), (1, 3)]
SEPARATION_Q = [(5, 1), (7, 0.5), (8, 3), (6, 4)]
SEPARATION_ANGLE = math.radians(10)
SEPARATION_OFFSET = 3.85
SEPARATION_SPACING = 1.1


def figure_separating_line():
    fig = figure.Figure(unit=36)
    normal = (math.cos(SEPARATION_ANGLE), math.sin(SEPARATION_ANGLE))

    def line(offset, y):
        """Point of the line {u: n . u = offset} of ordinate y."""
        return ((offset - normal[1] * y) / normal[0], y)

    fig.polygon(SEPARATION_P, "P", BLUE)
    fig.polygon(SEPARATION_Q, "Q", GREEN)
    y_min, y_max = -0.8, 4.5
    fig.polyline([line(SEPARATION_OFFSET, y_min), line(SEPARATION_OFFSET, y_max)],
                 "strokeColor=#000000;strokeWidth=2;")
    fig.polyline([line(SEPARATION_OFFSET + SEPARATION_SPACING, y_min),
                  line(SEPARATION_OFFSET + SEPARATION_SPACING, y_max)],
                 "strokeColor=#000000;dashed=1;")
    # Normal of the line.
    x0, y0 = line(SEPARATION_OFFSET, 3.2)
    fig.arrow(x0, y0, x0 + 0.8 * normal[0], y0 + 0.8 * normal[1],
              "strokeColor=#b85450;strokeWidth=2;")
    fig.text(x0 + 0.4, y0 + 0.55, "n(\u03b1)", w=1.5, extra="fontColor=#b85450;")
    # Spacing.
    x1, y1 = line(SEPARATION_OFFSET, 0.3)
    fig.arrow(x1, y1, x1 + SEPARATION_SPACING * normal[0], y1 + SEPARATION_SPACING * normal[1])
    fig.text(x1 + SEPARATION_SPACING / 2, y1 + 0.4, "s", w=1)
    fig.text(line(SEPARATION_OFFSET, y_min)[0], y_min - 0.4,
             "n(\u03b1) \u00b7 u = \u03b2", w=3)
    fig.text(line(SEPARATION_OFFSET + SEPARATION_SPACING, y_max)[0], y_max + 0.4,
             "n(\u03b1) \u00b7 u = \u03b2 + s", w=3.4)
    fig.write(SVG_DIR + "/" + PREFIX + "separating_line.svg")


################################################################################
# Geometry helpers.
################################################################################

def convex_hull(points):
    """Convex hull of a set of points, counterclockwise (monotone chain)."""
    points = sorted(set(points))

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower, upper = [], []
    for point in points:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], point) <= 0:
            lower.pop()
        lower.append(point)
    for point in reversed(points):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], point) <= 0:
            upper.pop()
        upper.append(point)
    return lower[:-1] + upper[:-1]


def closest_boundary_point(polygon, point):
    """Closest point to 'point' on the boundary of 'polygon', and its distance."""
    best = None
    for k in range(len(polygon)):
        (x1, y1), (x2, y2) = polygon[k - 1], polygon[k]
        dx, dy = x2 - x1, y2 - y1
        t = ((point[0] - x1) * dx + (point[1] - y1) * dy) / (dx * dx + dy * dy)
        t = max(0.0, min(1.0, t))
        q = (x1 + t * dx, y1 + t * dy)
        d = math.hypot(point[0] - q[0], point[1] - q[1])
        if best is None or d < best[1]:
            best = (q, d)
    return best


def contains(polygon, x, y):
    return Shape(polygon=polygon).contains(x, y)


def dot(fig, x, y, style="fillColor=#000000;strokeColor=#000000;"):
    radius = 0.08
    fig.rect(x - radius, y - radius, 2 * radius, 2 * radius, "", style + "ellipse=1;")


def circle(fig, x, y, radius, style):
    fig.rect(x - radius, y - radius, 2 * radius, 2 * radius, "", style + "ellipse=1;")


################################################################################
# Black-box optimization: no-fit polygon and penetration depth.
################################################################################

NFP_FIXED = [(0, 0), (4, 0), (4, 2), (0, 2)]
NFP_ORBITING = [(0, 0), (2, 0), (1, 2)]
# Position of the reference point of the orbiting item, relatively to the
# reference point of the fixed item: the items overlap.
NFP_POSITION = (2.6, 1.2)


def figure_no_fit_polygon():
    fig = figure.Figure(unit=26)
    no_fit_polygon = convex_hull([
        (a[0] - b[0], a[1] - b[1]) for a in NFP_FIXED for b in NFP_ORBITING])
    closest, depth = closest_boundary_point(no_fit_polygon, NFP_POSITION)
    px, py = NFP_POSITION
    step = 11
    panels = [
        "the items overlap",
        "no-fit polygon: the reference point of B is inside, at the penetration depth from its boundary",
        "B moved by the penetration depth: the items touch",
    ]
    for k, title in enumerate(panels):
        ox = k * step
        if k == 1:
            fig.polygon([(ox + x, y) for x, y in no_fit_polygon], "", YELLOW)
            fig.polygon([(ox + x, y) for x, y in NFP_FIXED], "A", "fillColor=none;strokeColor=#6c8ebf;dashed=1;")
            fig.arrow(ox + px, py, ox + closest[0], closest[1],
                      "strokeColor=#b85450;strokeWidth=2;")
            dot(fig, ox + px, py)
        else:
            x, y = (px, py) if k == 0 else closest
            fig.polygon([(ox + vx, vy) for vx, vy in NFP_FIXED], "A", BLUE)
            fig.polygon([(ox + x + vx, y + vy) for vx, vy in NFP_ORBITING], "B", RED + "opacity=80;")
            dot(fig, ox + x, y)
        fig.text(ox + 2, -3, title, w=9)
    fig.write(SVG_DIR + "/" + PREFIX + "no_fit_polygon.svg")


################################################################################
# Direct trigonometry: poles.
################################################################################

POLES_POLYGON = [(0, 0), (6, 0), (6, 1.4), (2.6, 1.9), (2.2, 4), (0, 4)]
POLES_NUMBER = 10


def compute_poles(polygon, number_of_poles, step=0.04):
    """Disjoint circles inscribed in the polygon, each one the largest circle
    inscribed in the polygon minus the previous ones (by grid search)."""
    xs = [x for x, _ in polygon]
    ys = [y for _, y in polygon]
    candidates = []
    x = min(xs) + step / 2
    while x < max(xs):
        y = min(ys) + step / 2
        while y < max(ys):
            if contains(polygon, x, y):
                candidates.append((x, y, closest_boundary_point(polygon, (x, y))[1]))
            y += step
        x += step
    poles = []
    for _ in range(number_of_poles):
        best = None
        for x, y, radius in candidates:
            for cx, cy, cr in poles:
                radius = min(radius, math.hypot(x - cx, y - cy) - cr)
            if best is None or radius > best[2]:
                best = (x, y, radius)
        poles.append(best)
    return poles


def figure_poles():
    fig = figure.Figure(unit=40)
    fig.polygon(POLES_POLYGON, "", BLUE)
    poles = compute_poles(POLES_POLYGON, POLES_NUMBER)
    for k, (x, y, radius) in enumerate(poles):
        circle(fig, x, y, radius, RED + "opacity=80;")
        fig.text(x, y, str(k + 1), w=0.6, size=10)
    fig.write(SVG_DIR + "/" + PREFIX + "poles.svg")


################################################################################
# Tree search with maximal spaces: maximal empty spaces.
################################################################################

SPACES_BIN = (10, 6)
# Items packed and the rectangles covering them.
SPACES_ITEMS = [
    ([(0, 0), (4, 0), (4, 1), (1, 1), (1, 4), (0, 4)], [(0, 0, 4, 1), (0, 1, 1, 4)]),
    ([(5, 0), (8, 0), (5, 3)], [(5, 0, 8, 1), (5, 1, 7, 2), (5, 2, 6, 3)]),
]


def maximal_empty_spaces(bin_rectangle, rectangles):
    """Maximal empty spaces of a bin, cut by each rectangle in turn: an empty
    space overlapping a rectangle is replaced by its maximal parts on the
    left, on the right, below and above it."""
    def cut(space, rectangle):
        x1, y1, x2, y2 = space
        a1, b1, a2, b2 = rectangle
        if a2 <= x1 or a1 >= x2 or b2 <= y1 or b1 >= y2:
            return [space]
        parts = []
        if a1 > x1:
            parts.append((x1, y1, a1, y2))
        if a2 < x2:
            parts.append((a2, y1, x2, y2))
        if b1 > y1:
            parts.append((x1, y1, x2, b1))
        if b2 < y2:
            parts.append((x1, b2, x2, y2))
        return parts

    def contained(a, b):
        return b[0] <= a[0] and b[1] <= a[1] and a[2] <= b[2] and a[3] <= b[3]

    spaces = [bin_rectangle]
    for rectangle in rectangles:
        parts = sorted(set(part for space in spaces for part in cut(space, rectangle)))
        spaces = [a for a in parts if not any(b != a and contained(a, b) for b in parts)]
    return spaces


def figure_maximal_empty_spaces():
    fig = figure.Figure(unit=12)
    bw, bh = SPACES_BIN
    rectangles = [r for _, item_rectangles in SPACES_ITEMS for r in item_rectangles]
    spaces = maximal_empty_spaces((0, 0, bw, bh), rectangles)
    # Selected: the closest bottom-left corner to the one of the bin
    # (Manhattan distance), then the largest.
    selected = min(spaces, key=lambda r: (r[0] + r[1], -(r[2] - r[0]) * (r[3] - r[1])))

    def node(ox, oy):
        for polygon, item_rectangles in SPACES_ITEMS:
            fig.polygon([(ox + x, oy + y) for x, y in polygon], "", BLUE)
            for x1, y1, x2, y2 in item_rectangles:
                fig.rect(ox + x1, oy + y1, x2 - x1, y2 - y1, "",
                         "fillColor=none;strokeColor=#6c8ebf;dashed=1;")

    columns = 4
    step_x, step_y = bw + 2, bh + 3.5
    width = columns * step_x - 2
    ox = (width - bw) / 2
    node(ox, 0)
    fig.rect(ox, 0, bw, bh, "", BIN_STYLE)
    fig.text(ox + bw / 2, -1.2, "items packed and the rectangles covering them", w=3 * bw)
    for k, space in enumerate(spaces):
        row, column = divmod(k, columns)
        ox = column * step_x
        oy = -(row + 1) * step_y - 1.5
        x1, y1, x2, y2 = space
        style = RED if space == selected else YELLOW
        fig.rect(ox + x1, oy + y1, x2 - x1, y2 - y1, "", style + "opacity=80;")
        node(ox, oy)
        fig.rect(ox, oy, bw, bh, "", BIN_STYLE)
        if space == selected:
            fig.text(ox + bw / 2, oy - 1.2, "selected", w=bw, extra="fontStyle=1;")
    fig.write(SVG_DIR + "/" + PREFIX + "maximal_empty_spaces.svg")


################################################################################
# Linear programming: shrunk items and separating edge.
################################################################################

# An item decomposed into two convex parts.
SHRUNK_PARTS = [
    [(0, 0), (3, 0), (3, 2), (0, 3)],
    [(3, 0), (5, 0), (5, 1.5), (3, 2)],
]
SHRUNK_SCALE = 0.6
# Two parts P and Q, the selected edge of P, and the spacing.
EDGE_P = [(0, 0), (2.5, 0.3), (3, 2.2), (0.5, 2.8)]
EDGE_Q = [(4.6, 0.6), (6.6, 0.2), (7, 2.5), (5.8, 3)]
EDGE_EDGE = 1
EDGE_SPACING = 0.9


def centroid(polygon):
    """Average of the vertices."""
    return (sum(x for x, _ in polygon) / len(polygon),
            sum(y for _, y in polygon) / len(polygon))


def figure_shrunk_items():
    fig = figure.Figure(unit=34)
    # Shrunk item.
    for part in SHRUNK_PARTS:
        cx, cy = centroid(part)
        fig.polygon(part, "", "fillColor=none;strokeColor=#b85450;dashed=1;")
        fig.polygon([(cx + SHRUNK_SCALE * (x - cx), cy + SHRUNK_SCALE * (y - cy)) for x, y in part],
                    "", RED)
        dot(fig, cx, cy)
    fig.text(2.5, -0.8, "convex parts scaled by \u03bb = {} around their centers".format(SHRUNK_SCALE), w=7)
    # Separating edge.
    ox = 8
    p = [(ox + x, y) for x, y in EDGE_P]
    q = [(ox + x, y) for x, y in EDGE_Q]
    fig.polygon(p, "P", BLUE)
    fig.polygon(q, "Q", GREEN)
    (x1, y1), (x2, y2) = p[EDGE_EDGE], p[EDGE_EDGE + 1]
    length = math.hypot(x2 - x1, y2 - y1)
    tx, ty = (x2 - x1) / length, (y2 - y1) / length
    # Outward normal (the polygon is counterclockwise).
    nx, ny = ty, -tx
    extension = 1.2
    fig.polyline([(x1 - extension * tx, y1 - extension * ty), (x2 + extension * tx, y2 + extension * ty)],
                 "strokeColor=#000000;")
    fig.polyline([(x1 - extension * tx + EDGE_SPACING * nx, y1 - extension * ty + EDGE_SPACING * ny),
                  (x2 + extension * tx + EDGE_SPACING * nx, y2 + extension * ty + EDGE_SPACING * ny)],
                 "strokeColor=#000000;dashed=1;")
    fig.polyline([(x1, y1), (x2, y2)], "strokeColor=#6c8ebf;strokeWidth=4;")
    fig.text((x1 + x2) / 2 - 0.35, (y1 + y2) / 2 + 0.1, "e", w=0.5)
    mx, my = (x1 + x2) / 2, (y1 + y2) / 2 - 0.5
    fig.arrow(mx, my, mx + 0.8 * nx, my + 0.8 * ny, "strokeColor=#b85450;strokeWidth=2;")
    fig.text(mx + 0.8 * nx + 0.3, my + 0.8 * ny - 0.25, "n", w=0.5, extra="fontColor=#b85450;")
    # Closest vertex of Q to the line of the edge.
    v = min(q, key=lambda point: (point[0] - x1) * nx + (point[1] - y1) * ny)
    dot(fig, v[0], v[1])
    fig.text(v[0] + 0.35, v[1] - 0.3, "v", w=0.5)
    fig.text(ox + 3.5, -0.8, "the closest vertex of Q beyond the edge of P, by the spacing", w=7)
    fig.write(SVG_DIR + "/" + PREFIX + "shrunk_items.svg")


################################################################################
# Direct trigonometry: moves.
################################################################################

MOVES_BIN = (10, 6)
MOVES_PACKED = [(0, 0, 4, 3), (0, 3, 3, 6), (5.5, 0, 10, 2), (6.5, 4, 10, 6), (3.5, 4.5, 6, 6)]
MOVES_ITEM = (2.4, 2)
MOVES_CURRENT = (2.6, 1.6)  # Bottom-left corner of the item, colliding.
MOVES_SEED = 3


def overlap(rectangle_1, rectangle_2):
    x1, y1, x2, y2 = rectangle_1
    a1, b1, a2, b2 = rectangle_2
    return max(0, min(x2, a2) - max(x1, a1)) * max(0, min(y2, b2) - max(y1, b1))


def moves_loss(x, y):
    """Overlap area of the item at (x, y) with the packed items."""
    w, h = MOVES_ITEM
    return sum(overlap((x, y, x + w, y + h), r) for r in MOVES_PACKED)


def coordinate_descent(x, y, step_initial, step_limit, generator):
    """Coordinate descent of 'Separator::refine': moves along the axes and
    the diagonals, the step growing after a success and shrinking after a
    failure."""
    bw, bh = MOVES_BIN
    w, h = MOVES_ITEM
    directions = [(1, 0), (0, 1), (1, 1), (-1, 1)]
    step_x = step_y = step_initial
    axis = generator.randrange(4)
    path = [(x, y)]
    loss = moves_loss(x, y)
    for _ in range(1000):
        if step_x < step_limit and step_y < step_limit:
            break
        dx, dy = directions[axis]
        best = None
        for sign in (-1, 1):
            nx = min(max(x + sign * dx * step_x, 0), bw - w)
            ny = min(max(y + sign * dy * step_y, 0), bh - h)
            neighbor = (moves_loss(nx, ny), nx, ny)
            if best is None or neighbor[0] < best[0]:
                best = neighbor
        better = best[0] < loss
        if best[0] <= loss:
            loss, x, y = best
            if (x, y) != path[-1]:
                path.append((x, y))
        multiplier = 1.1 if better else 0.5
        if axis == 0:
            step_x *= multiplier
        elif axis == 1:
            step_y *= multiplier
        else:
            step_x *= math.sqrt(multiplier)
            step_y *= math.sqrt(multiplier)
        if not better:
            axis = generator.randrange(4)
    return path, loss


def figure_moves():
    import random
    generator = random.Random(MOVES_SEED)
    fig = figure.Figure(unit=36)
    bw, bh = MOVES_BIN
    w, h = MOVES_ITEM
    for x1, y1, x2, y2 in MOVES_PACKED:
        fig.rect(x1, y1, x2 - x1, y2 - y1, "", BLUE)
    cx, cy = MOVES_CURRENT
    # Samples, given by the bottom-left corner of the item: around the
    # current placement (the center of the item in its current bounding box),
    # and in the whole bin.
    focused = [(cx + generator.uniform(-w / 2, w / 2), cy + generator.uniform(-h / 2, h / 2))
               for _ in range(25)]
    focused = [(min(max(x, 0), bw - w), min(max(y, 0), bh - h)) for x, y in focused]
    container = [(generator.uniform(0, bw - w), generator.uniform(0, bh - h)) for _ in range(50)]
    candidates = [(moves_loss(x, y), x, y) for x, y in [MOVES_CURRENT] + focused + container]
    # The 3 best distinct samples.
    best = []
    for loss, x, y in sorted(candidates):
        if all(abs(x - bx) >= 0.5 or abs(y - by) >= 0.5 for _, bx, by in best):
            best.append((loss, x, y))
        if len(best) == 3:
            break
    minimum_dimension = min(w, h)
    results = []
    for _, x, y in best:
        path, loss = coordinate_descent(x, y, 0.25 * minimum_dimension, 0.02 * minimum_dimension, generator)
        results.append((loss, path))
    final_loss, final_path = min(results, key=lambda result: result[0])
    fx, fy = final_path[-1]
    fig.rect(fx, fy, w, h, "", RED)
    fig.rect(cx, cy, w, h, "", "fillColor=none;strokeColor=#b85450;dashed=1;strokeWidth=2;")
    # The samples are drawn at the center of the item.
    for x, y in container:
        dot(fig, x + w / 2, y + h / 2, "fillColor=#999999;strokeColor=#999999;")
    for x, y in focused:
        dot(fig, x + w / 2, y + h / 2, "fillColor=#b85450;strokeColor=#b85450;")
    for _, path in results:
        fig.polyline([(x + w / 2, y + h / 2) for x, y in path], "strokeColor=#000000;strokeWidth=2;", arrow=True)
    fig.rect(0, 0, bw, bh, "", BIN_STYLE)
    fig.write(SVG_DIR + "/" + PREFIX + "moves.svg")


if __name__ == "__main__":
    figure_tree_search()
    figure_local_search()
    figure_milp_raster()
    figure_tree_search_insertions()
    figure_tree_search_slide()
    figure_periodic_packings()
    figure_separating_line()
    figure_no_fit_polygon()
    figure_poles()
    figure_maximal_empty_spaces()
    figure_shrunk_items()
    figure_moves()

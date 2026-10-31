"""Figures of the detailed description of the algorithms of irregular.

The shapes are drawn on a grid: polyominoes (sets of unit cells) and a
triangle.

Usage, from the root of the repository:

    python3 scripts/doc_figures/irregular.py
"""

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


if __name__ == "__main__":
    figure_tree_search()
    figure_local_search()
    figure_milp_raster()

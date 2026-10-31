"""Figures of the detailed description of the Benders decomposition with
contiguity of rectangle.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangle_benders_decomposition_contiguity.py
"""

import itertools
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, GREEN, YELLOW, GRAY

SVG_DIR = "doc/img"
PREFIX = "rectangle_benders_decomposition_contiguity_"

STYLES = {"A": BLUE, "B": RED, "C": GREEN, "D": YELLOW}


def figure_relaxation():
    """A solution of the master problem, where the slices of an item may be
    at different heights in different columns, and the packing found by the
    y-check from its x-positions."""
    fig = figure.Figure(unit=36)
    W, H = 4, 4
    # Item: (x, width, height).
    items = {"A": (0, 2, 2), "B": (1, 2, 1), "C": (2, 2, 2), "D": (1, 2, 1)}
    # Master solution: order of the slices in each column, from the bottom.
    stacks = [["A"], ["B", "A", "D"], ["C", "B", "D"], ["C"]]
    for c, stack in enumerate(stacks):
        y = 0
        for name in stack:
            h = items[name][2]
            fig.rect(c, y, 1, h, name, STYLES[name])
            y += h
    for c in range(1, W):
        fig.polyline([(c, 0), (c, H)], "strokeColor=#999999;dashed=1;")
    fig.bin(0, 0, W, H)
    fig.text(W / 2, -0.5, "master problem: x-positions", w=W + 3)
    fig.text(W / 2, -1.0, "and column heights only", w=W + 3)
    fig.big_arrow(W + 0.6, H / 2, length=1.2, direction="right")
    # y-check.
    ox = W + 2.5
    ys = y_check(items, W, H)
    for name, (x, w, h) in items.items():
        fig.rect(ox + x, ys[name], w, h, name, STYLES[name])
    fig.bin(ox, 0, W, H)
    fig.text(ox + W / 2, -0.5, "y-check: y-positions", w=W + 3)
    fig.text(ox + W / 2, -1.0, "for the same x-positions", w=W + 3)
    fig.write(SVG_DIR + "/" + PREFIX + "relaxation.svg")


def y_check(items, W, H):
    """Brute-force y-check."""
    names = list(items)
    for ys in itertools.product(*[range(H - items[n][2] + 1) for n in names]):
        ok = True
        for i, j in itertools.combinations(range(len(names)), 2):
            xi, wi, hi = items[names[i]]
            xj, wj, hj = items[names[j]]
            if xi < xj + wj and xj < xi + wi and ys[i] < ys[j] + hj and ys[j] < ys[i] + hi:
                ok = False
                break
        if ok:
            return dict(zip(names, ys))
    return None


def figure_niche():
    """A branching step of the y-check: the niche, a candidate, the closed
    gap."""
    fig = figure.Figure(unit=28)
    W, H = 8, 5
    skyline = [3, 3, 1, 1, 1, 2, 4, 4]

    def draw_skyline(ox, heights):
        for c, h in enumerate(heights):
            if h > 0:
                fig.rect(ox + c, 0, 1, h, "", GRAY + "strokeColor=#bbbbbb;")
        points = []
        for c, h in enumerate(heights):
            points += [(ox + c, h), (ox + c + 1, h)]
        fig.polyline(points, "strokeColor=#b85450;strokeWidth=3;")
        fig.bin(ox, 0, W, H)

    draw_skyline(0, skyline)
    fig.rect(2, 1, 3, 0.25, "", YELLOW + "strokeColor=#d6b656;")
    fig.text(3.5, 1.6, "niche", w=3, size=12)
    fig.text(W / 2, -0.7, "niche: lowest columns, leftmost", w=W + 2)
    # Candidate: x = 3, width 2, height 2 (entirely in the niche).
    ox = W + 2
    after = list(skyline)
    x, w, h = 3, 2, 2
    for c in range(2, x):
        after[c] = min(skyline[1], 1 + h)
    for c in range(x, x + w):
        after[c] = 1 + h
    draw_skyline(ox, after)
    fig.rect(ox + x, 1, w, h, "item", BLUE)
    fig.rect(ox + 2, 1, x - 2, after[2] - 1, "", RED + "fillStyle=hatch;")
    fig.text(ox + W / 2, -0.7, "item placed at x = 3: the gap on its left is closed", w=W + 4)
    fig.write(SVG_DIR + "/" + PREFIX + "niche.svg")


def lifted_windows(units, W):
    """Windows [l_i, r_i] of the LP of the cut strengthening, solved by brute
    force on integers."""
    n = len(units)
    overlaps = [(i, j) for i in range(n) for j in range(n)
                if i != j and units[i][0] < units[j][0] + units[j][1]
                and units[j][0] < units[i][0] + units[i][1]]
    best, best_value = None, -1
    ranges = []
    for x, w in units:
        ranges.append([(l, r) for l in range(0, x + 1) for r in range(x, W - w + 1)])
    for choice in itertools.product(*ranges):
        if all(choice[i][1] - choice[j][0] <= units[j][1] - 1 for i, j in overlaps):
            value = sum(r - l for l, r in choice)
            if value > best_value:
                best, best_value = choice, value
    return best


def figure_lifted_windows():
    fig = figure.Figure(unit=30)
    W = 12
    units = [(1, 4), (4, 3), (6, 4)]
    names = ["A", "B", "C"]
    windows = lifted_windows(units, W)
    for k, ((x, w), (l, r), name) in enumerate(zip(units, windows, names)):
        y = -1.6 * k
        # Positions covered by the unit for x in [l, r].
        fig.rect(l, y, r + w - l, 0.8, "", STYLES[name] + "opacity=35;dashed=1;")
        fig.rect(x, y, w, 0.8, name, STYLES[name])
        fig.text(W + 0.3, y + 0.4, "x in [{}, {}]".format(l, r), w=3, size=12, align="left")
    for c in range(W + 1):
        fig.polyline([(c, 1), (c, 1.2)], "strokeColor=#666666;")
    fig.polyline([(0, 1), (W, 1)], "strokeColor=#666666;")
    fig.text(0, 1.6, "0", w=1, size=11)
    fig.text(W, 1.6, str(W), w=1, size=11)
    fig.write(SVG_DIR + "/" + PREFIX + "lifted_windows.svg")
    return windows


if __name__ == "__main__":
    figure_relaxation()
    figure_niche()
    print(figure_lifted_windows())

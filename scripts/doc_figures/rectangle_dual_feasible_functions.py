"""Figures of the detailed description of the dual feasible functions of
rectangle.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangle_dual_feasible_functions.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure

SVG_DIR = "doc/img"
PREFIX = "rectangle_dual_feasible_functions_"

C, K = 10, 3


def f0(x):
    if x > C - K:
        return C
    if x < K:
        return 0
    return x


def f1(x):
    # Medium items: all of length K, so that MC(c) = c // K.
    if x > C / 2:
        return C // K - (C - x) // K
    if x < K:
        return 0
    return 1


def f2(x):
    if x > C / 2:
        return 2 * (C // K - (C - x) // K)
    if x == C / 2 and C % 2 == 0:
        return C // K
    return 2 * (x // K)


def figure_functions():
    fig = figure.Figure(unit=24)
    size = 6
    for k, (name, f) in enumerate((("f0", f0), ("f1", f1), ("f2", f2))):
        ox = k * (size + 2.5)
        top = f(C)
        # Axes.
        fig.arrow(ox, 0, ox + size + 0.5, 0, "strokeColor=#000000;")
        fig.arrow(ox, 0, ox, size + 0.5, "strokeColor=#000000;")
        # Identity (scaled), for reference.
        fig.polyline([(ox, 0), (ox + size, size)], "strokeColor=#bbbbbb;dashed=1;")
        # Function, as steps over the integers 0..C.
        points = []
        for x in range(C + 1):
            y = f(x) / top * size
            points.append((ox + x / C * size, y))
        for x in range(C + 1):
            px, py = points[x]
            fig.rect(px - 0.08, py - 0.08, 0.16, 0.16, "", "ellipse;fillColor=#6c8ebf;strokeColor=#6c8ebf;", extra="ellipse;")
        fig.polyline(points, "strokeColor=#6c8ebf;strokeWidth=2;")
        for x in (K, C // 2, C - K, C):
            fig.text(ox + x / C * size, -0.45, str(x), w=1, size=10)
        fig.text(ox - 0.5, size, str(top), w=1, size=10)
        fig.text(ox + size / 2, -1.2, "{}, C = {}, k = {}".format(name, C, K), w=size + 2, size=12)
    fig.write(SVG_DIR + "/" + PREFIX + "functions.svg")


if __name__ == "__main__":
    figure_functions()

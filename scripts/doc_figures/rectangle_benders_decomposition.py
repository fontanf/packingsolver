"""Figures of the detailed description of the Benders decomposition of
rectangle.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangle_benders_decomposition.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drawio
from drawio import BLUE, RED, GREEN, YELLOW, GRAY

DRAWIO_DIR = "doc/internals"
PNG_DIR = "doc/img"
PREFIX = "rectangle_benders_decomposition_"


def figure_iteration():
    """Flow of an iteration."""
    fig = drawio.Figure(unit=30)
    bw, bh = 7, 1.2

    def box(x, y, label, style=BLUE):
        fig.rect(x - bw / 2, y - bh / 2, bw, bh, label, style + "rounded=1;")

    def decision(x, y, label):
        fig.rect(x - bw / 2, y - bh / 2, bw, bh, label, YELLOW + "rhombus;", extra="rhombus;")

    x = 0
    steps = [
        (0, "solve the master problem", BLUE, None),
        (-2.2, "can its solution beat the best one?", None, "no: stop"),
        (-4.4, "dual feasible function cut violated by a bin?", None, "yes: add the cuts"),
        (-6.6, "incompatible triplet in a bin?", None, "yes: add the cuts"),
        (-8.8, "solve the feasibility subproblem of each bin", BLUE, None),
        (-11.0, "does every bin fit?", None, "yes: stop"),
        (-13.2, "add the no-good cuts of the infeasible bins", BLUE, None),
    ]
    for k, (y, label, style, side) in enumerate(steps):
        if style is None:
            decision(x, y, label)
        else:
            box(x, y, label, style)
        if k + 1 < len(steps):
            next_y = steps[k + 1][0]
            fig.arrow(x, y - bh / 2, x, next_y + bh / 2, "strokeColor=#333333;")
            if style is None:
                answer = "yes" if "beat" in label else "no"
                fig.text(x + 0.5, (y + next_y) / 2, answer, w=1, size=11, align="left")
        if side is not None:
            fig.arrow(x + bw / 2, y, x + bw / 2 + 2, y, "strokeColor=#333333;")
            answer, action = side.split(": ")
            fig.text(x + bw / 2 + 1, y + 0.35, answer, w=1.5, size=11)
            style = RED if action == "stop" else GREEN
            fig.rect(x + bw / 2 + 2, y - bh / 2, 3.6, bh, action, style + "rounded=1;")
            if action != "stop":
                # Back to the master problem.
                fig.polyline([(x + bw / 2 + 5.6, y), (x + bw / 2 + 6.4, y),
                              (x + bw / 2 + 6.4, 0), (x + bw / 2, 0)],
                             "strokeColor=#333333;", arrow=True)
    # Last box back to the master problem.
    y = steps[-1][0]
    fig.polyline([(x - bw / 2, y), (x - bw / 2 - 1, y), (x - bw / 2 - 1, 0), (x - bw / 2, 0)],
                 "strokeColor=#333333;", arrow=True)
    fig.write(DRAWIO_DIR + "/" + PREFIX + "iteration.drawio",
              PNG_DIR + "/" + PREFIX + "iteration.png")


def figure_triplet_shapes():
    """The 8 guillotine arrangements of three items."""
    fig = drawio.Figure(unit=16)
    sizes = {1: (4, 3), 2: (3, 2), 3: (2, 2)}

    def row(items):
        x, placed = 0, []
        for i in items:
            placed.append((i, x, 0))
            x += sizes[i][0]
        return placed

    def column(items):
        y, placed = 0, []
        for i in items:
            placed.append((i, 0, y))
            y += sizes[i][1]
        return placed

    shapes = [("row", row([1, 2, 3])), ("column", column([1, 2, 3]))]
    for odd in (3, 2, 1):
        a, b = [i for i in (1, 2, 3) if i != odd]
        # Pair side by side, then the odd one above it.
        pair_h = max(sizes[a][1], sizes[b][1])
        shapes.append(("{} | {}, then {} above".format(a, b, odd),
                       [(a, 0, 0), (b, sizes[a][0], 0), (odd, 0, pair_h)]))
        # Pair one above the other, then the odd one to its right.
        pair_w = max(sizes[a][0], sizes[b][0])
        shapes.append(("{} / {}, then {} on the right".format(a, b, odd),
                       [(a, 0, 0), (b, 0, sizes[a][1]), (odd, pair_w, 0)]))
    columns = 4
    for k, (title, placed) in enumerate(shapes):
        ox = (k % columns) * 13
        oy = -(k // columns) * 9.5
        for i, x, y in placed:
            w, h = sizes[i]
            fig.rect(ox + x, oy + y, w, h, str(i), BLUE)
        width = max(x + sizes[i][0] for i, x, y in placed)
        height = max(y + sizes[i][1] for i, x, y in placed)
        fig.rect(ox, oy, width, height, "", "fillColor=none;strokeColor=#000000;dashed=1;")
        fig.text(ox + 5, oy - 1, title, w=12, size=11)
    fig.write(DRAWIO_DIR + "/" + PREFIX + "triplet_shapes.drawio",
              PNG_DIR + "/" + PREFIX + "triplet_shapes.png")


def figure_minimal_subsets():
    """Depth-first search of the infeasible subsets of a selection."""
    fig = drawio.Figure(unit=28)
    bw, bh = 2.6, 0.8
    # (label, x, y, status, parent): status 'infeasible', 'feasible', 'cut'.
    nodes = [
        ("abcde", 7, 0, "infeasible", None),
        ("bcde", 1.5, -2, "cut", 0),
        ("acde", 4.5, -2, "feasible", 0),
        ("abde", 7.5, -2, "cut", 0),
        ("abce", 10.5, -2, "feasible", 0),
        ("abcd", 13.5, -2, "feasible", 0),
        ("cde", -0.75, -4, "small", 1),
        ("bde", 0.75, -4, "small", 1),
        ("bce", 2.25, -4, "small", 1),
        ("bcd", 3.75, -4, "small", 1),
        ("abe", 6.75, -4, "small", 3),
        ("abd", 8.25, -4, "small", 3),
    ]
    styles = {
        "infeasible": RED,
        "feasible": GREEN,
        "cut": RED + "strokeWidth=3;",
        "small": GREEN + "dashed=1;",
    }
    for label, x, y, status, parent in nodes:
        w = bw if len(label) > 3 else 1.3
        fig.rect(x - w / 2, y - bh / 2, w, bh, label, styles[status])
        if parent is not None:
            px, py = nodes[parent][1], nodes[parent][2]
            fig.arrow(px, py - bh / 2, x, y + bh / 2, "strokeColor=#666666;")
    fig.text(7, -5.3, "red: doesn't fit; green: fits; dashed: 3 items, known to fit; thick: no-good cut", w=16, size=12)
    fig.write(DRAWIO_DIR + "/" + PREFIX + "minimal_subsets.drawio",
              PNG_DIR + "/" + PREFIX + "minimal_subsets.png")


if __name__ == "__main__":
    figure_iteration()
    figure_triplet_shapes()
    figure_minimal_subsets()

"""Figures of the detailed description of the Benders decomposition of
rectangle.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangle_benders_decomposition.py
"""

import itertools
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, GREEN, YELLOW, GRAY

SVG_DIR = "doc/img"
PREFIX = "rectangle_benders_decomposition_"


def figure_iteration():
    """Flow of an iteration."""
    fig = figure.Figure(unit=30)
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
    fig.write(SVG_DIR + "/" + PREFIX + "iteration.svg")


def figure_triplet_shapes():
    """The 8 guillotine arrangements of three items."""
    fig = figure.Figure(unit=16)
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
    fig.write(SVG_DIR + "/" + PREFIX + "triplet_shapes.svg")


def figure_minimal_subsets():
    """Depth-first search of the infeasible subsets of a selection."""
    fig = figure.Figure(unit=28)
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
    fig.write(SVG_DIR + "/" + PREFIX + "minimal_subsets.svg")


################################################################################
# Example of the principle.
################################################################################

# Bin and items (name, width, height) of the example; the items can't be
# rotated, and the profit of an item is its area.
EXAMPLE_BIN = (5, 3)
EXAMPLE_ITEMS = [("1", 3, 2), ("2", 2, 3), ("3", 2, 1.5)]
# Step of the positions tried by 'pack'.
STEP = 0.5


def pack(items, bin_size):
    """Positions of the items in the bin, found by enumerating the positions
    on a grid, or None if they don't fit."""
    bw, bh = bin_size
    positions = []

    def overlap(a, b):
        (xa, ya, wa, ha), (xb, yb, wb, hb) = a, b
        return xa < xb + wb and xb < xa + wa and ya < yb + hb and yb < ya + ha

    def rec(k):
        if k == len(items):
            return True
        _, w, h = items[k]
        x = 0
        while x + w <= bw:
            y = 0
            while y + h <= bh:
                rect = (x, y, w, h)
                if not any(overlap(rect, other) for other in positions):
                    positions.append(rect)
                    if rec(k + 1):
                        return True
                    positions.pop()
                y += STEP
            x += STEP
        return False

    return list(positions) if rec(0) else None


def benders_example():
    """Iterations: the selection of the master problem (the subset of items of
    largest area fitting in the area of the bin, and not containing a subset
    forbidden by a cut), and the packing of the subproblem."""
    bw, bh = EXAMPLE_BIN
    cuts = []
    iterations = []
    while True:
        best = None
        for r in range(len(EXAMPLE_ITEMS), 0, -1):
            for subset in itertools.combinations(EXAMPLE_ITEMS, r):
                area = sum(w * h for _, w, h in subset)
                if area > bw * bh:
                    continue
                names = set(name for name, _, _ in subset)
                if any(cut <= names for cut in cuts):
                    continue
                if best is None or area > best[1]:
                    best = (subset, area)
        subset, area = best
        positions = pack(list(subset), EXAMPLE_BIN)
        iterations.append((subset, area, positions))
        if positions is not None:
            return iterations
        cuts.append(set(name for name, _, _ in subset))


def figure_example():
    fig = figure.Figure(unit=36)
    bw, bh = EXAMPLE_BIN
    # Instance.
    fig.rect(0, 0, bw, bh, "", "fillColor=none;strokeColor=#000000;")
    fig.text(bw / 2, -0.4, "bin, area {}".format(bw * bh), w=4, size=11)
    x = bw + 1
    for name, w, h in EXAMPLE_ITEMS:
        fig.rect(x, 0, w, h, name, BLUE)
        fig.text(x + w / 2, -0.4, "area {:g}".format(w * h), w=3, size=11)
        x += w + 1
    y = -1.5
    for k, (subset, area, positions) in enumerate(benders_example()):
        y -= bh + 1.8
        fig.text(0, y + bh + 0.7, "iteration {}: the master problem selects {} (area {:g})".format(
            k + 1, ", ".join(name for name, _, _ in subset), area), w=14, align="left", extra="fontStyle=1;")
        x = 0
        for name, w, h in subset:
            fig.rect(x, y, w, h, name, BLUE)
            x += w + 0.5
        fig.big_arrow(x + 0.3, y + bh / 2, length=1.4, direction="right")
        ox = x + 2.4
        if positions is not None:
            for (name, _, _), (px, py, w, h) in zip(subset, positions):
                fig.rect(ox + px, y + py, w, h, name, BLUE)
            verdict = "the items fit"
        else:
            # The largest items which fit, and the others sticking out.
            fitting = pack(list(subset[:-1]), EXAMPLE_BIN)
            for (name, _, _), (px, py, w, h) in zip(subset, fitting):
                fig.rect(ox + px, y + py, w, h, name, BLUE)
            name, w, h = subset[-1]
            fig.rect(ox, y + bh - h / 2, w, h, name, RED + "dashed=1;opacity=80;")
            verdict = "no room for " + name + ": a cut forbids " + ", ".join(n for n, _, _ in subset)
        fig.rect(ox, y, bw, bh, "", "fillColor=none;strokeColor=#000000;strokeWidth=2;")
        fig.text(ox + bw + 0.4, y + bh / 2, verdict, w=7, align="left")
    fig.write(SVG_DIR + "/" + PREFIX + "example.svg")


if __name__ == "__main__":
    figure_iteration()
    figure_example()
    figure_triplet_shapes()
    figure_minimal_subsets()

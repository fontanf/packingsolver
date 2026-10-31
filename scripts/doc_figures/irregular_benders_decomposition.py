"""Figures of the detailed description of the Benders decomposition of
irregular.

Usage, from the root of the repository:

    python3 scripts/doc_figures/irregular_benders_decomposition.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, YELLOW

SVG_DIR = "doc/img"
PREFIX = "irregular_benders_decomposition_"


def figure_iteration():
    """Flow of an iteration, with the MILP NFP algorithm in the
    subproblems."""
    fig = figure.Figure(unit=30)
    bw, bh = 11, 1.2
    step = 2.0

    steps = [
        ("solve the master problem", BLUE, None),
        ("can its solution beat the best one?", YELLOW, ("no", "stop")),
        ("for each bin, add its items, largest first,\nuntil MILP NFP proves that they don't fit", BLUE, None),
        ("if none is proven infeasible,\npack the items of the bin", BLUE, None),
        ("does every bin fit?", YELLOW, ("yes", "stop")),
        ("minimal infeasible subset\nof each bin which doesn't fit", BLUE, None),
        ("lift the proven cuts,\nadd the cuts to the master problem", BLUE, None),
    ]
    x = 0
    for k, (label, style, side) in enumerate(steps):
        y = -k * step
        fig.rect(x - bw / 2, y - bh / 2, bw, bh, label, style + "rounded=1;fontSize=11;")
        if k + 1 < len(steps):
            fig.arrow(x, y - bh / 2, x, y - step + bh / 2, "strokeColor=#333333;")
            if style == YELLOW:
                answer = "yes" if side[0] == "no" else "no"
                fig.text(x + 0.3, y - step / 2, answer, w=1, size=11, align="left")
        if side is not None:
            answer, action = side
            fig.arrow(x + bw / 2, y, x + bw / 2 + 2, y, "strokeColor=#333333;")
            fig.text(x + bw / 2 + 1, y + 0.35, answer, w=1.5, size=11)
            fig.rect(x + bw / 2 + 2, y - bh / 2, 2.4, bh, action, RED + "rounded=1;fontSize=11;")

    # Back to the master problem.
    y_last = -(len(steps) - 1) * step
    fig.polyline(
        [(x - bw / 2, y_last), (x - bw / 2 - 1, y_last), (x - bw / 2 - 1, 0), (x - bw / 2, 0)],
        "strokeColor=#333333;", arrow=True)

    fig.write(os.path.join(SVG_DIR, PREFIX + "iteration.svg"))


if __name__ == "__main__":
    figure_iteration()

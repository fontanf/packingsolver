"""Figures of the detailed description of the column generation strips and
sequential strips / one-dimensional algorithms of rectangleguillotine.

The column generation is computed exactly, on 2-staged strips (items stacked
one above the other), with the linear programs of
'generic_algorithms_model.py'.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangleguillotine_strips.py
"""

import itertools
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, RED, GREEN
import generic_algorithms_model as model

SVG_DIR = "doc/img"
PREFIX = "rectangleguillotine_"

BIN_STYLE = "fillColor=none;strokeColor=#000000;strokeWidth=2;"


################################################################################
# Column generation strips.
################################################################################

CG_BIN = (16, 12)
# Item types (width, height), one copy each; the profit of an item is its
# area.
CG_ITEMS = {"1": (8, 10), "2": (9, 8), "3": (6, 8), "4": (8, 4), "5": (7, 2)}


def strip_width(strip):
    return max(CG_ITEMS[name][0] for name in strip)


def strip_profit(strip):
    return sum(CG_ITEMS[name][0] * CG_ITEMS[name][1] for name in strip)


def strips_column_generation(available, width, columns, tabu):
    """Column generation of the knapsack objective with the items 'available'
    and a bin of the given width; returns the value and the solution (strip,
    value) of the linear relaxation, or None if it is infeasible."""
    if width < 0:
        return None, None
    names = [name for name in CG_ITEMS if name in available]
    if not names:
        return 0, []
    while True:
        usable = [s for s in columns if set(s) <= available and s not in tabu]
        # Minimize the opposite of the profit; slack variables for the
        # constraints, which are inequalities.
        columns_lp = [[strip_width(s)] + [int(n in s) for n in names] for s in usable]
        costs = [-strip_profit(s) for s in usable]
        number_of_rows = 1 + len(names)
        for r in range(number_of_rows):
            columns_lp.append([int(i == r) for i in range(number_of_rows)])
            costs.append(0)
        value, x, y, _ = model.simplex(costs, columns_lp, [width] + [1] * len(names))
        u = -y[0]
        v = {name: -y[1 + i] for i, name in enumerate(names)}
        # Pricing: the strip of largest reduced profit, by enumeration.
        best = None
        for r in range(1, len(names) + 1):
            for strip in itertools.combinations(names, r):
                if strip in tabu:
                    continue
                if sum(CG_ITEMS[n][1] for n in strip) > CG_BIN[1] or strip_width(strip) > width:
                    continue
                reduced_profit = strip_profit(strip) - sum(v[n] for n in strip) - strip_width(strip) * u
                if best is None or reduced_profit > best[0]:
                    best = (reduced_profit, strip)
        if best is not None and best[0] > 0 and best[1] not in columns:
            columns.append(best[1])
            continue
        return -value, [(s, x[k]) for k, s in enumerate(usable) if x[k] > 0]


def strips_search():
    """Nodes of the limited discrepancy search down to an integer solution:
    the strip of the relaxation whose rounding is the most confident is
    fixed; if the child is infeasible, the strip is fixed to one less and
    becomes tabu."""
    columns = []
    fixed = []
    tabu = set()
    nodes = []
    while True:
        available = set(CG_ITEMS) - {n for s, k in fixed for n in s if k > 0}
        width = CG_BIN[0] - sum(strip_width(s) * k for s, k in fixed)
        value, solution = strips_column_generation(available, width, columns, tabu)
        nodes.append({"fixed": list(fixed), "tabu": set(tabu), "value": value, "solution": solution})
        if all(v.denominator == 1 for _, v in solution):
            return nodes
        best = None
        for strip, v in solution:
            fixed_value = sum(k for s, k in fixed if s == strip)
            main = v.numerator // v.denominator
            if main <= fixed_value:
                main = fixed_value + 1
            if best is None or main - v < best[2]:
                best = (strip, main, main - v)
        strip, main, _ = best
        # Child of discrepancy 0, or, if it is infeasible, of discrepancy 1.
        child = [(s, k) for s, k in fixed if s != strip] + [(strip, main)]
        child_width = CG_BIN[0] - sum(strip_width(s) * k for s, k in child)
        if child_width >= 0:
            fixed = child
        else:
            nodes[-1]["infeasible"] = (strip, main)
            if main - 1 > 0:
                fixed = [(s, k) for s, k in fixed if s != strip] + [(strip, main - 1)]
            tabu.add(strip)


def draw_strip(fig, strip, ox, oy, style=BLUE):
    y = 0
    for name in strip:
        w, h = CG_ITEMS[name]
        fig.rect(ox, oy + y, w, h, name, style)
        y += h


def figure_column_generation_strips():
    fig = figure.Figure(unit=10)
    bw, bh = CG_BIN
    fig.rect(0, 0, bw, bh, "bin {} x {}".format(bw, bh), "fillColor=none;strokeColor=#000000;")
    x = bw + 2
    for name, (w, h) in CG_ITEMS.items():
        fig.rect(x, 0, w, h, "{}\n{} x {}".format(name, w, h), BLUE)
        x += w + 2
    y = -6
    for k, node in enumerate(strips_search()):
        if k == 0:
            title = "root: linear relaxation, value {:.1f}".format(float(node["value"]))
        else:
            title = "fixed: " + ", ".join(
                    "strip {} x {}".format("+".join(s), v) for s, v in node["fixed"])
            if node["tabu"]:
                title += "; tabu: " + ", ".join("strip " + "+".join(s) for s in node["tabu"])
        fig.text(0, y, title, w=60, align="left", extra="fontStyle=1;")
        y -= 2 + bh
        # Bin with the fixed strips, and the strips of the relaxation.
        x = 0
        for strip, v in node["fixed"]:
            for _ in range(v):
                draw_strip(fig, strip, x, y)
                x += strip_width(strip)
        fig.rect(0, y, bw, bh, "", BIN_STYLE)
        x = bw + 3
        for strip, v in node["solution"]:
            draw_strip(fig, strip, x, y, GREEN)
            fig.text(x + strip_width(strip) / 2, y - 1.5, "x = {}".format(v), w=10, size=11)
            x += strip_width(strip) + 3
        notes = []
        if "infeasible" in node:
            strip, main = node["infeasible"]
            notes.append("fixing strip {} to {} exceeds the width of the bin".format("+".join(strip), main))
        if all(v.denominator == 1 for _, v in node["solution"]):
            notes.append("the relaxation is integer: solution of profit {}".format(
                sum(strip_profit(s) * v for s, v in node["fixed"]) + node["value"]))
        for note in notes:
            fig.text(x, y + bh / 2, note, w=40, align="left")
            y -= 1.5
        y -= 5
    fig.write(SVG_DIR + "/" + PREFIX + "column_generation_strips.svg")


################################################################################
# Sequential strips / one-dimensional.
################################################################################

SS_BIN = (16, 12)
SS_ITEMS = {"1": (8, 8), "2": (2, 8), "3": (4, 7), "4": (7, 6), "5": (6, 6), "6": (5, 5),
            "7": (5, 5), "8": (4, 5), "9": (3, 5), "10": (2, 5), "11": (7, 4)}
# Strips of the strip packing phase (items from the bottom).
SS_STRIPS = [["1", "11"], ["4", "6"], ["5", "7"], ["8", "9"], ["3", "10"], ["2"]]


def ss_strip_width(strip):
    return max(SS_ITEMS[name][0] for name in strip)


def draw_ss_strip(fig, strip, ox, oy):
    y = 0
    for name in strip:
        w, h = SS_ITEMS[name]
        fig.rect(ox, oy + y, w, h, name, BLUE)
        y += h


def figure_sequential_strips_onedimensional():
    bw, bh = SS_BIN
    # The strips contain all the items, once, within the height of the bin.
    assert sorted(n for s in SS_STRIPS for n in s) == sorted(SS_ITEMS)
    assert all(sum(SS_ITEMS[n][1] for n in s) <= bh for s in SS_STRIPS)
    # One-dimensional bin packing of the widths of the strips: the smallest
    # number of bins.
    widths = [ss_strip_width(s) for s in SS_STRIPS]
    for number_of_bins in range(1, len(widths) + 1):
        contents = model.fits([bw] * number_of_bins, widths)
        if contents is not None:
            break

    fig = figure.Figure(unit=10)
    fig.rect(0, 0, bw, bh, "bin {} x {}".format(bw, bh), "fillColor=none;strokeColor=#000000;")
    x = bw + 2
    for name, (w, h) in SS_ITEMS.items():
        fig.rect(x, 0, w, h, name, BLUE)
        x += w + 1.5
    width = x
    fig.big_arrow(width / 2, -1, length=3)
    fig.text(width / 2 + 2, -2.5, "1. strip packing", w=20, align="left", extra="fontStyle=1;")
    x = 0
    for strip in SS_STRIPS:
        draw_ss_strip(fig, strip, x, -6 - bh)
        x += ss_strip_width(strip) + 2
    fig.big_arrow(width / 2, -8 - bh, length=3)
    fig.text(width / 2 + 2, -9.5 - bh, "2. one-dimensional bin packing of the widths", w=40, align="left",
             extra="fontStyle=1;")
    ox = 0
    remaining = list(SS_STRIPS)
    for content in contents:
        x = 0
        for w in content:
            strip = next(s for s in remaining if ss_strip_width(s) == w)
            remaining.remove(strip)
            draw_ss_strip(fig, strip, ox + x, -13 - 2 * bh)
            x += w
        fig.rect(ox, -13 - 2 * bh, bw, bh, "", BIN_STYLE)
        ox += bw + 3
    fig.write(SVG_DIR + "/" + PREFIX + "sequential_strips_onedimensional.svg")


if __name__ == "__main__":
    figure_column_generation_strips()
    figure_sequential_strips_onedimensional()

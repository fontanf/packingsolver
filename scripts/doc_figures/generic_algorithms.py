"""Figures of the detailed description of the generic algorithms.

The examples are one-dimensional, and computed with the rules of the
algorithms, reimplemented in 'generic_algorithms_model.py'.

Usage, from the root of the repository:

    python3 scripts/doc_figures/generic_algorithms.py
"""

import os
import sys
from fractions import Fraction

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, GREEN, YELLOW, RED, GRAY, WHITE
import generic_algorithms_model as model

SVG_DIR = "doc/img"
PREFIX = "generic_algorithms_"

STYLES = [BLUE, GREEN, YELLOW, RED]
NAMES = ["A", "B", "C", "D"]

# Length of a unit of length of the items, in figure units.
SCALE = 0.45
# Height of a bin.
HEIGHT = 0.8


def write(fig, name):
    fig.write(SVG_DIR + "/" + PREFIX + name + ".svg")


def draw_bin(fig, x, y, bin_length, items, label=None, scale=SCALE):
    """Bin of length 'bin_length' with its bottom-left corner at (x, y);
    'items' is a list of (name, length, style)."""
    fig.rect(x, y, bin_length * scale, HEIGHT, "", WHITE)
    position = 0
    for name, length, style in items:
        fig.rect(x + position * scale, y, length * scale, HEIGHT, name, style)
        position += length
    if label is not None:
        fig.text(x + bin_length * scale + 0.2, y + HEIGHT / 2, label, w=2.4, align="left")


def pattern_items(pattern, item_types):
    return [(NAMES[j], item_types[j][0], STYLES[j])
            for j, xj in enumerate(pattern) for _ in range(xj)]


def draw_item_types(fig, x, y, item_types, scale=SCALE):
    """Item types, with their lengths and numbers of copies."""
    for j, (length, copies) in enumerate(item_types):
        fig.rect(x, y, length * scale, HEIGHT, NAMES[j], STYLES[j])
        fig.text(x + length * scale + 0.15, y + HEIGHT / 2,
                 "length {}, {} copies".format(length, copies), w=4, align="left")
        y -= HEIGHT + 0.3
    return y


def fmt(value, digits=2):
    return ("{:." + str(digits) + "f}").format(value)


################################################################################
# Sequential single knapsack
################################################################################

def figure_sequential_single_knapsack():
    bin_length = 10
    item_types = [(6, 4), (4, 6), (3, 5)]
    bins = model.sequential_single_knapsack(bin_length, 20, item_types)

    fig = figure.Figure(unit=34)
    fig.text(0, 0.5, "Bins of length {}".format(bin_length), w=6, align="left", extra="fontStyle=1;")
    y = draw_item_types(fig, 0, -0.7, item_types)

    y -= 0.5
    packed = [0] * len(item_types)
    for k, (pattern, copies) in enumerate(bins):
        remaining = ", ".join(
                "{} {}".format(NAMES[j], item_types[j][1] - packed[j])
                for j in range(len(item_types)) if item_types[j][1] - packed[j] > 0)
        fig.text(0, y + HEIGHT / 2, "remaining: " + remaining, w=5.5, align="left", size=11)
        draw_bin(fig, 6, y, bin_length, pattern_items(pattern, item_types),
                 label="x {}".format(copies))
        for j, xj in enumerate(pattern):
            packed[j] += copies * xj
        y -= HEIGHT + 0.5
    fig.text(0, y + 0.2, "{} bins".format(sum(c for _, c in bins)), w=4, align="left", extra="fontStyle=1;")
    write(fig, "sequential_single_knapsack")


################################################################################
# Sequential value correction
################################################################################

def figure_sequential_value_correction():
    bin_length = 10
    item_types = [(7, 5), (2, 5)]
    passes = model.sequential_value_correction(bin_length, 20, item_types, 2)

    fig = figure.Figure(unit=34)
    fig.text(0, 0.5, "Bins of length {}".format(bin_length), w=6, align="left", extra="fontStyle=1;")
    y = draw_item_types(fig, 0, -0.7, item_types)

    # Columns of the table.
    x_profits = 0
    x_bins = 3.6
    x_adjusted = 10.6
    x_new = 14.6
    y -= 0.4
    for x, label in [
            (x_profits, "profits"),
            (x_bins, "bins"),
            (x_adjusted, "adjusted space\nof a copy"),
            (x_new, "corrected\nprofits")]:
        fig.text(x, y, label, w=3.6, h=1, align="left", extra="fontStyle=1;")
    y -= 1.0

    for p, data in enumerate(passes):
        top = y
        fig.text(x_profits, y + 0.3, "pass {}".format(p + 1), w=3, align="left", extra="fontStyle=1;")
        for j in range(len(item_types)):
            fig.text(x_profits, y - 0.35 - 0.5 * j,
                     "{}: {}".format(NAMES[j], fmt(data["profits"][j])), w=3, align="left")
        number_of_bins = 0
        for pattern, copies in data["bins"]:
            draw_bin(fig, x_bins, y - HEIGHT, bin_length, pattern_items(pattern, item_types),
                     label="x {}".format(copies))
            number_of_bins += copies
            y -= HEIGHT + 0.3
        fig.text(x_bins, y - 0.1, "{} bins".format(number_of_bins), w=3, align="left")
        for j, (length, copies) in enumerate(item_types):
            fig.text(x_adjusted, top - 0.35 - 0.5 * j,
                     "{}: {}".format(NAMES[j], fmt(data["adjusted_space"][j] / copies)),
                     w=3.6, align="left")
            fig.text(x_new, top - 0.35 - 0.5 * j,
                     "{}: ({} + {}) / 2 = {}".format(
                         NAMES[j],
                         fmt(data["profits"][j]),
                         fmt(data["new_profits"][j]),
                         fmt(data["corrected_profits"][j])),
                     w=5.5, align="left")
        y -= 1.0
        if p + 1 < len(passes):
            fig.polyline([(0, y + 0.3), (20, y + 0.3)], "strokeColor=#999999;dashed=1;")
            y -= 0.4
    write(fig, "sequential_value_correction")


################################################################################
# Column generation
################################################################################

def draw_pattern(fig, x, y, bin_length, pattern, item_types, label=None):
    draw_bin(fig, x, y, bin_length, pattern_items(pattern, item_types), label=label)


def figure_column_generation():
    bin_length = 10
    item_types = [(6, 2), (5, 3), (4, 4), (2, 3)]
    log = []
    model.column_generation(bin_length, item_types, columns=[], log=log)

    fig = figure.Figure(unit=34)
    fig.text(0, 0.5, "Bins of length {}".format(bin_length), w=6, align="left", extra="fontStyle=1;")
    y = draw_item_types(fig, 0, -0.7, item_types)

    x_phase = 0
    x_value = 1.8
    x_duals = 5.2
    x_pattern = 9.6
    x_rc = 15.0
    y -= 0.4
    for x, label in [
            (x_phase, "phase"),
            (x_value, "value of the\nlinear program"),
            (x_duals, "duals of the items\n(A, B, C, D)"),
            (x_pattern, "column found\nby the pricing"),
            (x_rc, "reduced\ncost")]:
        fig.text(x, y, label, w=4, h=1, align="left", extra="fontStyle=1;")
    y -= 1.5
    for entry in log:
        fig.text(x_phase, y + HEIGHT / 2, str(entry["phase"]), w=1, align="left")
        fig.text(x_value, y + HEIGHT / 2, str(entry["value"]), w=2, align="left")
        fig.text(x_duals, y + HEIGHT / 2,
                 ", ".join(str(d) for d in entry["duals"]), w=6.5, align="left")
        if entry["reduced_cost"] < 0:
            draw_pattern(fig, x_pattern, y, bin_length, entry["pattern"], item_types)
        else:
            fig.text(x_pattern, y + HEIGHT / 2, "none", w=4, align="left")
        fig.text(x_rc, y + HEIGHT / 2, str(entry["reduced_cost"]), w=2, align="left")
        y -= HEIGHT + 0.35
    write(fig, "column_generation")


def figure_column_generation_search():
    bin_length = 10
    item_types = [(6, 2), (5, 3), (4, 4), (2, 3)]
    nodes = model.dive(bin_length, item_types)

    fig = figure.Figure(unit=34)
    y = 0
    for k, node in enumerate(nodes):
        fixed_value = sum(v for _, v in node["fixed"])
        title = "root node" if k == 0 else "child: {} fixed".format(
                ", ".join("pattern {} x {}".format(
                    "+".join(NAMES[j] for j, xj in enumerate(p) for _ in range(xj)), v)
                    for p, v in node["fixed"]))
        fig.text(0, y, title, w=12, align="left", extra="fontStyle=1;")
        y -= 0.8
        for pattern, value in node["fixed"]:
            draw_pattern(fig, 0, y - HEIGHT, bin_length, pattern, item_types,
                         label="fixed x {}".format(value))
            y -= HEIGHT + 0.3
        for pattern, value in node["solution"]:
            draw_pattern(fig, 0, y - HEIGHT, bin_length, pattern, item_types,
                         label="x = {}".format(value))
            y -= HEIGHT + 0.3
        total = fixed_value + node["value"]
        if all(v.denominator == 1 for _, v in node["solution"]):
            text = "{} bins: the solution is integer".format(total)
        else:
            text = "value {} = {}: the bound is {} bins".format(
                    total, float(total), -(-total.numerator // total.denominator))
        fig.text(0, y - 0.1, text, w=12, align="left")
        y -= 1.2
        if k + 1 < len(nodes):
            fig.big_arrow(2.2, y + 0.5, length=0.8)
            y -= 0.8
    write(fig, "column_generation_search")


################################################################################
# Dichotomic search
################################################################################

def figure_dichotomic_search():
    bin_types = [(10, 10, 3), (8, 8, 3), (6, 6, 3)]
    items = [7, 5, 5, 5, 4, 2, 2]
    iterations = model.dichotomic_search(bin_types, items)
    item_styles = {7: BLUE, 5: GREEN, 4: YELLOW, 2: RED}

    fig = figure.Figure(unit=34)
    fig.text(0, 0.5, "Bin types (cost = length, 3 copies each)", w=10, align="left", extra="fontStyle=1;")
    x = 0
    for length, _, _ in bin_types:
        fig.rect(x, -0.6, length * SCALE, HEIGHT, str(length), WHITE)
        x += length * SCALE + 0.5
    fig.text(0, -1.4, "Items (total length {})".format(sum(items)), w=10, align="left", extra="fontStyle=1;")
    x = 0
    for length in items:
        fig.rect(x, -2.6, length * SCALE, HEIGHT, str(length), item_styles[length])
        x += length * SCALE + 0.2

    y = -3.7
    seen = []
    for k, iteration in enumerate(iterations):
        if iteration["chosen"] in seen:
            continue
        seen.append(iteration["chosen"])
        cost = sum(c * l for c, (l, _, _) in zip(iteration["chosen"], bin_types))
        fig.text(0, y, "iteration {}: waste estimate {:g}%, space {} x {:g} = {} at least".format(
            k + 1, 100 * iteration["waste"], sum(items), 1 + iteration["waste"], iteration["required"]),
            w=16, align="left", extra="fontStyle=1;")
        y -= 0.5
        fig.text(0, y - 0.1, "cheapest bins: {} (cost {})".format(
            " + ".join(str(l) for (l, _, _), c in zip(bin_types, iteration["chosen"]) for _ in range(c)),
            cost), w=14, align="left")
        y -= 0.5
        bins = [l for (l, _, _), c in zip(bin_types, iteration["chosen"]) for _ in range(c)]
        contents = iteration["contents"]
        x = 0
        for b, length in enumerate(bins):
            content = [] if contents is None else [(str(i), i, item_styles[i]) for i in contents[b]]
            draw_bin(fig, x, y - HEIGHT, length, content)
            x += length * SCALE + 0.4
        result = ("the items fit: the estimate is decreased" if contents is not None
                  else "the items don't fit: the estimate is increased")
        fig.text(x + 0.2, y - HEIGHT / 2, result, w=11, align="left",
                 extra="fontColor=#82b366;" if contents is not None else "fontColor=#b85450;")
        y -= HEIGHT + 0.8
    last = iterations[-1]
    fig.text(0, y + 0.2, "next iterations: the estimate stays between {:g}% and {:g}%, where the same bins "
             "are chosen, which aren't solved again; the search stops after {} iterations".format(
                 100 * iterations[2]["waste"], 100 * iterations[1]["waste"], len(iterations)),
             w=24, align="left")
    write(fig, "dichotomic_search")


################################################################################
# Sequential feasibility
################################################################################

def figure_sequential_feasibility():
    """Open dimension x: a strip of width 2, items of width 1, packed in two
    rows by a feasibility solver."""
    items = [5, 4, 3, 3, 3]
    strip_height = 2
    iterations = model.sequential_feasibility_open_dimension(items, strip_height)
    styles = {5: BLUE, 4: GREEN, 3: YELLOW, 2: RED}
    scale = 0.55

    fig = figure.Figure(unit=34)
    fig.text(0, 0.5, "Items of height 1 and lengths {}; strip of height {}".format(
        ", ".join(str(i) for i in items), strip_height), w=14, align="left", extra="fontStyle=1;")
    y = -0.5
    for k, iteration in enumerate(iterations):
        length = iteration["length"]
        title = "sub-problem {}: length {}".format(k + 1, length)
        if k == 0:
            title += " (twice the area of the items, divided by the height of the strip)"
        fig.text(0, y, title, w=16, align="left", extra="fontStyle=1;")
        y -= 0.4
        fig.rect(0, y - strip_height * HEIGHT, length * scale, strip_height * HEIGHT, "", GRAY)
        if iteration["rows"] is not None:
            for r, row in enumerate(iteration["rows"]):
                position = 0
                for i in row:
                    fig.rect(position * scale, y - (r + 1) * HEIGHT, i * scale, HEIGHT, str(i), styles[i])
                    position += i
            x_max = iteration["x_max"]
            fig.polyline([(x_max * scale, y + 0.1), (x_max * scale, y - strip_height * HEIGHT - 0.1)],
                         "strokeColor=#b85450;dashed=1;strokeWidth=2;")
            text = "found, length used {}: next length min(0.99 x {}, {} - 1) = {}".format(
                    x_max, x_max, x_max, iteration["next_length"])
        else:
            text = "no solution: stop, the previous solution is returned"
        fig.text(0, y - strip_height * HEIGHT - 0.35, text, w=16, align="left")
        y -= strip_height * HEIGHT + 1.2
    write(fig, "sequential_feasibility")


if __name__ == "__main__":
    figure_sequential_single_knapsack()
    figure_sequential_value_correction()
    figure_column_generation()
    figure_column_generation_search()
    figure_dichotomic_search()
    figure_sequential_feasibility()

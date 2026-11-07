"""Figure of the detailed description of the conservative scales of
rectangle.

Usage, from the root of the repository:

    python3 scripts/doc_figures/rectangle_conservative_scales.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure
from figure import BLUE, GREEN

SVG_DIR = "doc/img"
PREFIX = "rectangle_conservative_scales_"

W, H = 10, 4
# Items: three 4 x 4 items and one 7 x 4 item. Any two 4-wide items fit side
# by side in the width of the bin, but not three of them, and the 7-wide item
# fits with none of them. The conservative widths maximizing the transformed
# area are 5 for the 4-wide items (any two of them must still fit) and 10 for
# the 7-wide item (it must still fit alone).
ITEMS = [("1", 4, 5, BLUE), ("2", 4, 5, BLUE), ("3", 4, 5, BLUE), ("4", 7, 10, GREEN)]
BIN_STYLE = "fillColor=none;strokeColor=#000000;strokeWidth=2;"


def figure_widths():
    fig = figure.Figure(unit=16)
    rows = [
        ("original widths", 1),
        ("conservative widths", 2),
    ]
    # The bins are aligned, after the longest row of items.
    ox = max(sum(item[column] + 0.5 for item in ITEMS) for _, column in rows) + 2
    oy = 0
    for title, column in rows:
        area = sum(item[column] * H for item in ITEMS)
        bound = -(-area // (W * H))
        fig.text(-0.5, oy + H / 2, title, w=8, align="right", extra="fontStyle=1;")
        # The items, one after the other.
        x = 0
        for name, *widths, style in ITEMS:
            width = widths[column - 1]
            fig.rect(x, oy, width, H, "{} ({})".format(name, width), style)
            x += width + 0.5
        # Two 4-wide items in a bin.
        width = ITEMS[0][column]
        fig.rect(ox, oy, width, H, "1", BLUE)
        fig.rect(ox + width, oy, width, H, "2", BLUE)
        fig.rect(ox, oy, W, H, "", BIN_STYLE)
        fig.text(ox + W + 0.5, oy + H / 2,
                 "area bound: ⌈{} / {}⌉ = {}".format(area, W * H, bound),
                 w=11, align="left")
        oy -= H + 2
    fig.write(SVG_DIR + "/" + PREFIX + "widths.svg")


if __name__ == "__main__":
    figure_widths()

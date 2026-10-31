"""Minimal SVG writer for the figures of the documentation.

Coordinates are given in a mathematical frame (y going up) in arbitrary units;
a Figure converts them into pixels.

The style of an element is a string of 'key=value;' pairs (the keys of
draw.io, e.g. 'fillColor=#dae8fc;strokeColor=#6c8ebf;dashed=1;').
"""

import html

# Colors of the figures of the documentation.
BLUE = "fillColor=#dae8fc;strokeColor=#6c8ebf;"
RED = "fillColor=#f8cecc;strokeColor=#b85450;"
GREEN = "fillColor=#d5e8d4;strokeColor=#82b366;"
YELLOW = "fillColor=#fff2cc;strokeColor=#d6b656;"
GRAY = "fillColor=#f5f5f5;strokeColor=#666666;fontColor=#333333;"
DEFECT = "fillColor=#999999;strokeColor=#333333;fontColor=#000000;fillStyle=hatch;labelBackgroundColor=#ffffff;"
WHITE = "fillColor=#ffffff;strokeColor=#000000;"
NONE = "fillColor=none;"

FONT_FAMILY = "Helvetica, Arial, sans-serif"
# Mean width of a character, relative to the font size, used to wrap the
# texts and to size the backgrounds of the labels.
CHARACTER_WIDTH = 0.47
# Margin around the figure, in pixels.
MARGIN = 10


def parse_style(style):
    """Dictionary of the 'key=value;' pairs of a style; a key alone (e.g.
    'ellipse;') is mapped to '1'."""
    result = {}
    for entry in style.split(";"):
        entry = entry.strip()
        if not entry:
            continue
        if "=" in entry:
            key, value = entry.split("=", 1)
            result[key] = value
        else:
            result[entry] = "1"
    return result


def wrap(label, width, size):
    """Lines of a label: its own lines, each wrapped to fit in 'width'
    pixels."""
    maximum = max(1, int(width / (CHARACTER_WIDTH * size)))
    lines = []
    for paragraph in str(label).split("\n"):
        line = ""
        for word in paragraph.split(" "):
            candidate = word if not line else line + " " + word
            if len(candidate) <= maximum or not line:
                line = candidate
            else:
                lines.append(line)
                line = word
        lines.append(line)
    return lines


class Figure:

    def __init__(self, unit=40):
        self.unit = unit
        self.elements = []
        self.patterns = {}
        self.x_min = self.y_min = float("inf")
        self.x_max = self.y_max = -float("inf")

    def _x(self, x):
        return round(x * self.unit, 2)

    def _y(self, y):
        # SVG's y axis goes down.
        return round(-y * self.unit, 2)

    def _extend(self, x1, y1, x2, y2):
        self.x_min = min(self.x_min, x1, x2)
        self.x_max = max(self.x_max, x1, x2)
        self.y_min = min(self.y_min, y1, y2)
        self.y_max = max(self.y_max, y1, y2)

    def _hatch(self, color):
        """Id of a hatching pattern of the given color."""
        if color not in self.patterns:
            pattern_id = "hatch" + str(len(self.patterns))
            self.patterns[color] = (
                    '<pattern id="{}" width="6" height="6" patternUnits="userSpaceOnUse" '
                    'patternTransform="rotate(45)">'
                    '<line x1="0" y1="0" x2="0" y2="6" stroke="{}" stroke-width="2"/>'
                    '</pattern>'.format(pattern_id, color))
        return self.patterns[color].split('"')[1]

    def _shape_attributes(self, style):
        s = parse_style(style)
        fill = s.get("fillColor", "#ffffff")
        stroke = s.get("strokeColor", "#000000")
        attributes = 'fill="{}" stroke="{}" stroke-width="{}"'.format(
                fill, stroke, s.get("strokeWidth", "1"))
        if s.get("dashed") == "1":
            attributes += ' stroke-dasharray="3 3"'
        if "opacity" in s:
            attributes += ' opacity="{}"'.format(int(s["opacity"]) / 100)
        return s, attributes

    def _label(self, cx, cy, width, label, size=12, color="#000000", bold=False,
               anchor="middle", background=None):
        """Text centered vertically on (cx, cy) pixels."""
        if label == "":
            return
        lines = wrap(label, width, size)
        line_height = 1.2 * size
        top = cy - line_height * (len(lines) - 1) / 2
        weight = ' font-weight="bold"' if bold else ""
        if background is not None:
            text_width = max(len(line) for line in lines) * CHARACTER_WIDTH * size + 4
            x0 = {"middle": cx - text_width / 2, "start": cx - 2, "end": cx - text_width + 2}[anchor]
            self.elements.append(
                    '<rect x="{}" y="{}" width="{}" height="{}" fill="{}"/>'.format(
                        round(x0, 2), round(top - line_height / 2, 2),
                        round(text_width, 2), round(line_height * len(lines), 2), background))
        # The baseline of a line is below its center by about a third of the
        # font size (instead of 'dominant-baseline', which not all the
        # renderers support).
        tspans = "".join(
                '<tspan x="{}" y="{}">{}</tspan>'.format(
                    round(cx, 2), round(top + k * line_height + 0.35 * size, 2), html.escape(line))
                for k, line in enumerate(lines))
        self.elements.append(
                '<text font-family="{}" font-size="{}" fill="{}" text-anchor="{}"{}>{}</text>'.format(
                    FONT_FAMILY, size, color, anchor, weight, tspans))

    def rect(self, x, y, w, h, label="", style=BLUE, extra=""):
        """Rectangle whose bottom-left corner is (x, y)."""
        s, attributes = self._shape_attributes(style + extra)
        px, py = self._x(x), self._y(y + h)
        pw, ph = round(w * self.unit, 2), round(h * self.unit, 2)
        self._extend(px, py, px + pw, py + ph)
        if s.get("fillStyle") == "hatch":
            fill = s.get("fillColor", "#ffffff")
            # Background, then hatching of the color of the stroke.
            self.elements.append('<rect x="{}" y="{}" width="{}" height="{}" fill="{}"{}/>'.format(
                px, py, pw, ph, fill,
                ' opacity="{}"'.format(int(s["opacity"]) / 100) if "opacity" in s else ""))
            attributes = attributes.replace(
                    'fill="{}"'.format(fill),
                    'fill="url(#{})"'.format(self._hatch(s.get("strokeColor", "#000000"))), 1)
        if s.get("ellipse") == "1":
            self.elements.append('<ellipse cx="{}" cy="{}" rx="{}" ry="{}" {}/>'.format(
                round(px + pw / 2, 2), round(py + ph / 2, 2), round(pw / 2, 2), round(ph / 2, 2),
                attributes))
        elif s.get("rhombus") == "1":
            points = [(px + pw / 2, py), (px + pw, py + ph / 2), (px + pw / 2, py + ph), (px, py + ph / 2)]
            self.elements.append('<polygon points="{}" {}/>'.format(
                " ".join("{},{}".format(round(a, 2), round(b, 2)) for a, b in points), attributes))
        else:
            radius = ' rx="{}"'.format(round(min(pw, ph) * 0.15, 2)) if s.get("rounded") == "1" else ""
            self.elements.append('<rect x="{}" y="{}" width="{}" height="{}"{} {}/>'.format(
                px, py, pw, ph, radius, attributes))
        self._label(px + pw / 2, py + ph / 2, pw, label,
                    size=int(s.get("fontSize", "12")),
                    color=s.get("fontColor", "#000000"),
                    bold=(s.get("fontStyle") == "1"),
                    background=s.get("labelBackgroundColor"))

    def polygon(self, points, label="", style=BLUE, label_position=None):
        """Polygon through 'points'; its label is centered on the mean of
        the points, or on 'label_position'."""
        s, attributes = self._shape_attributes(style)
        pixels = [(self._x(x), self._y(y)) for x, y in points]
        for px, py in pixels:
            self._extend(px, py, px, py)
        self.elements.append('<polygon points="{}" {}/>'.format(
            " ".join("{},{}".format(a, b) for a, b in pixels), attributes))
        if label_position is None:
            label_position = (
                    sum(x for x, _ in points) / len(points),
                    sum(y for _, y in points) / len(points))
        xs = [px for px, _ in pixels]
        self._label(self._x(label_position[0]), self._y(label_position[1]),
                    max(xs) - min(xs), label,
                    size=int(s.get("fontSize", "12")),
                    color=s.get("fontColor", "#000000"),
                    bold=(s.get("fontStyle") == "1"))

    def bin(self, x, y, w, h):
        self.rect(x, y, w, h, "", WHITE + "strokeWidth=2;fillColor=none;")

    def text(self, x, y, label, w=4, h=0.6, align="center", size=12, extra=""):
        """Text box centered on (x, y), or starting at x if align is 'left',
        or ending at x if align is 'right'."""
        s = parse_style(extra)
        x0 = x - w / 2 if align == "center" else (x - w if align == "right" else x)
        px0, pw = self._x(x0), w * self.unit
        py = self._y(y)
        lines = wrap(label, pw, size)
        half_height = max(h * self.unit, 1.2 * size * len(lines)) / 2
        self._extend(px0, py - half_height, px0 + pw, py + half_height)
        anchor = {"center": "middle", "left": "start", "right": "end"}[align]
        cx = {"center": px0 + pw / 2, "left": px0, "right": px0 + pw}[align]
        self._label(cx, py, pw, label, size=size,
                    color=s.get("fontColor", "#000000"),
                    bold=(s.get("fontStyle") == "1"),
                    anchor=anchor,
                    background=s.get("labelBackgroundColor"))

    def polyline(self, points, style="strokeColor=#000000;", arrow=False):
        """Polyline through 'points'; with an arrow at its end if 'arrow'."""
        s = parse_style(style)
        pixels = [(self._x(x), self._y(y)) for x, y in points]
        for px, py in pixels:
            self._extend(px, py, px, py)
        stroke = s.get("strokeColor", "#000000")
        attributes = 'fill="none" stroke="{}" stroke-width="{}"'.format(stroke, s.get("strokeWidth", "1"))
        if s.get("dashed") == "1":
            attributes += ' stroke-dasharray="3 3"'
        if "opacity" in s:
            attributes += ' opacity="{}"'.format(int(s["opacity"]) / 100)
        if arrow:
            attributes += ' marker-end="url(#{})"'.format(self._arrow_head(stroke))
        self.elements.append('<polyline points="{}" {}/>'.format(
            " ".join("{},{}".format(a, b) for a, b in pixels), attributes))

    def _arrow_head(self, color):
        key = "arrow" + color
        if key not in self.patterns:
            marker_id = "arrow" + str(len(self.patterns))
            self.patterns[key] = (
                    '<marker id="{}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" '
                    'markerHeight="8" markerUnits="userSpaceOnUse" orient="auto">'
                    '<path d="M 0 0 L 10 5 L 0 10 z" fill="{}"/></marker>'.format(marker_id, color))
        return self.patterns[key].split('"')[1]

    def arrow(self, x1, y1, x2, y2, style="strokeColor=#000000;"):
        self.polyline([(x1, y1), (x2, y2)], style, arrow=True)

    def big_arrow(self, x, y, length=1.2, direction="down"):
        """Block arrow starting at (x, y)."""
        px, py = self._x(x), self._y(y)
        pl = length * self.unit
        shaft, head, head_length = 5, 11, min(14, pl / 2)
        if direction == "down":
            points = [(px - shaft, py), (px + shaft, py), (px + shaft, py + pl - head_length),
                      (px + head, py + pl - head_length), (px, py + pl),
                      (px - head, py + pl - head_length), (px - shaft, py + pl - head_length)]
        else:
            points = [(px, py - shaft), (px + pl - head_length, py - shaft),
                      (px + pl - head_length, py - head), (px + pl, py),
                      (px + pl - head_length, py + head), (px + pl - head_length, py + shaft),
                      (px, py + shaft)]
        for a, b in points:
            self._extend(a, b, a, b)
        self.elements.append('<polygon points="{}" fill="#ffffff" stroke="#000000"/>'.format(
            " ".join("{},{}".format(round(a, 2), round(b, 2)) for a, b in points)))

    def write(self, svg_path):
        x0, y0 = self.x_min - MARGIN, self.y_min - MARGIN
        width = self.x_max - self.x_min + 2 * MARGIN
        height = self.y_max - self.y_min + 2 * MARGIN
        content = (
                '<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
                'viewBox="{x} {y} {w} {h}">\n'.format(
                    x=round(x0, 2), y=round(y0, 2), w=round(width, 2), h=round(height, 2))
                + ("<defs>" + "".join(self.patterns.values()) + "</defs>\n" if self.patterns else "")
                + '<rect x="{}" y="{}" width="{}" height="{}" fill="#ffffff"/>\n'.format(
                    round(x0, 2), round(y0, 2), round(width, 2), round(height, 2))
                + "".join(element + "\n" for element in self.elements)
                + "</svg>\n")
        with open(svg_path, "w") as f:
            f.write(content)

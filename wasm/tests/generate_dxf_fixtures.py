"""Generate the DXF files of the tests of the DXF reader ('wasm/web/dxf.js').

Each file 'fixtures/dxf/<name>.dxf' comes with 'fixtures/dxf/<name>.json',
the parts the reader must find: for each part (in reading order: top to
bottom, then left to right), its area, number of holes and bounding box size,
computed analytically. And the number of warnings.

Requires ezdxf. Usage:

    python3 wasm/tests/generate_dxf_fixtures.py
"""

import json
import math
import os

import ezdxf

DIRECTORY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "dxf")


def save(document, name, parts, warnings=0):
    os.makedirs(DIRECTORY, exist_ok=True)
    document.saveas(os.path.join(DIRECTORY, name + ".dxf"))
    with open(os.path.join(DIRECTORY, name + ".json"), "w") as file:
        json.dump({"parts": parts, "warnings": warnings}, file, indent=4)
        file.write("\n")
    print(name)


def part(area, holes, width, height):
    return {"area": area, "holes": holes, "width": width, "height": height}


def lines(msp, points, order=None, reverse=()):
    """Closed contour of LINE entities, in the given order, some reversed."""
    n = len(points)
    for i in (order or range(n)):
        p, q = points[i], points[(i + 1) % n]
        if i in reverse:
            p, q = q, p
        msp.add_line(p, q)


def plate_with_hole():
    """A rectangle of LINE entities, out of order and some reversed, with a
    circular hole, in millimeters."""
    document = ezdxf.new("R2000")
    document.header["$INSUNITS"] = 4
    msp = document.modelspace()
    lines(msp, [(10, 20), (110, 20), (110, 80), (10, 80)], order=[2, 0, 3, 1], reverse={0, 3})
    msp.add_circle((40, 50), 10)
    save(document, "plate_with_hole", [part(100 * 60 - math.pi * 100, 1, 100, 60)])


def rounded_rectangle():
    """A closed LWPOLYLINE with arcs (bulges): a rectangle with rounded
    corners, with a slot hole made of two half circles, drawn clockwise."""
    document = ezdxf.new("R2000")
    msp = document.modelspace()
    r = 5
    b = math.tan(math.pi / 8)  # Quarter turn.
    msp.add_lwpolyline([
        (r, 0, 0, 0, 0), (80 - r, 0, 0, 0, b), (80, r, 0, 0, 0), (80, 40 - r, 0, 0, b),
        (80 - r, 40, 0, 0, 0), (r, 40, 0, 0, b), (0, 40 - r, 0, 0, 0), (0, r, 0, 0, b),
    ], format="xyseb", close=True)
    # Slot: length 30 between the centers of its ends, width 10, clockwise.
    msp.add_lwpolyline([(25, 20, 0, 0, -1), (25, 30, 0, 0, 0), (55, 30, 0, 0, -1), (55, 20, 0, 0, 0)],
                       format="xyseb", close=True)
    outer = 80 * 40 - (4 - math.pi) * r * r
    slot = 30 * 10 + math.pi * 25
    save(document, "rounded_rectangle", [part(outer - slot, 1, 80, 40)])


def r12_polyline():
    """An R12 POLYLINE with an arc larger than a half turn (bulge 2)."""
    document = ezdxf.new("R12")
    msp = document.modelspace()
    # Square of side 20 whose top side is replaced by an arc bulging upward.
    msp.add_polyline2d([(0, 0), (20, 0), (20, 20), (0, 20)], close=True)
    polyline = msp.query("POLYLINE")[0]
    polyline.vertices[2].dxf.bulge = 2  # From (20, 20) to (0, 20), anticlockwise.
    angle = 4 * math.atan(2)
    radius = 20 / (2 * math.sin(angle / 2))
    segment = radius * radius / 2 * (angle - math.sin(angle))
    # Height of the arc above the chord.
    sagitta = radius + radius * math.cos(math.pi - angle / 2)
    save(document, "r12_polyline", [part(400 + segment, 0, 2 * radius, 20 + sagitta)])


def multiple_parts():
    """Several parts in one file: a square with a square hole and a part
    inside the hole, a circle, a slot made of LINE and ARC entities.
    Annotations are ignored; a SPLINE and an open contour give warnings."""
    document = ezdxf.new("R2000")
    msp = document.modelspace()
    # Square with a hole, and a disk in the hole.
    lines(msp, [(0, 100), (40, 100), (40, 140), (0, 140)])
    lines(msp, [(10, 110), (30, 110), (30, 130), (10, 130)], reverse={1, 2})
    msp.add_circle((20, 120), 4)
    # A circle.
    msp.add_circle((100, 120), 10)
    # A slot of LINE and ARC entities: two half circles of radius 5 and two
    # lines, the bottom one reversed.
    msp.add_line((70, 0), (40, 0))
    msp.add_arc((70, 5), 5, -90, 90)
    msp.add_line((70, 10), (40, 10))
    msp.add_arc((40, 5), 5, 90, 270)
    # Ignored, or reported.
    msp.add_text("PART A").set_placement((0, 150))
    msp.add_point((200, 200))
    msp.add_spline([(150, 0), (160, 10), (170, 0), (180, 10)])
    msp.add_line((150, 50), (180, 50))
    save(document, "multiple_parts", [
        part(1600 - 400, 1, 40, 40),
        part(math.pi * 100, 0, 20, 20),
        part(math.pi * 16, 0, 8, 8),
        part(30 * 10 + math.pi * 25, 0, 40, 10),
    ], warnings=2)


def blocks():
    """A block (a triangle with a circular hole) inserted twice: rotated by
    90 degrees and scaled by 2, and mirrored."""
    document = ezdxf.new("R2000")
    block = document.blocks.new(name="PART", base_point=(10, 10))
    block.add_lwpolyline([(10, 10), (40, 10), (10, 40)], close=True)
    block.add_circle((18, 18), 3)
    msp = document.modelspace()
    msp.add_blockref("PART", (100, 0), dxfattribs={"rotation": 90, "xscale": 2, "yscale": 2})
    msp.add_blockref("PART", (0, 0), dxfattribs={"xscale": -1})
    area = 30 * 30 / 2 - math.pi * 9
    save(document, "blocks", [part(4 * area, 1, 60, 60), part(area, 1, 30, 30)])


def mirrored_arcs():
    """A 'D' shape whose arc has the extrusion direction (0, 0, -1): its
    coordinates are mirrored (x -> -x)."""
    document = ezdxf.new("R2000")
    msp = document.modelspace()
    # In world coordinates: the half disk of center (-5, 0) and radius 10 on
    # the right side (x >= -5), closed by a line. In the mirrored object
    # coordinate system, the center is at (5, 0) and the arc goes from 90 to
    # 270 degrees (the left side). Without the mirroring, the ends of the
    # arc wouldn't meet the line.
    msp.add_arc((5, 0), 10, 90, 270, dxfattribs={"extrusion": (0, 0, -1)})
    msp.add_line((-5, -10), (-5, 10))
    save(document, "mirrored_arcs", [part(math.pi * 50, 0, 10, 20)])


if __name__ == "__main__":
    plate_with_hole()
    rounded_rectangle()
    r12_polyline()
    multiple_parts()
    blocks()
    mirrored_arcs()

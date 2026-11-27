"""
Build the instances of "Heuristic approaches to large-scale periodic packing
of irregular shapes on a rectangular sheet" (Costa, Gomes and Oliveira, 2009,
https://doi.org/10.1016/j.ejor.2007.09.012).

Each instance asks to pack as many copies as possible of a single shape in a
rectangular sheet, the shape being allowed to rotate by any angle:

* Cheng: the pattern of table 1 of "Concepts of neighbourhood and universal
  compact yield towards achieving best pattern layouts" (Cheng and Rao, 1999,
  https://doi.org/10.1080/002075499189961), in a 200 x 200 sheet
* Stoyan: the footwear piece of the example of "Regular packing of congruent
  polygons on the rectangular sheet" (Stoyan and Pankratov, 1999,
  https://doi.org/10.1016/S0377-2217(98)00050-2), in a 900 x 700 sheet
* Swim1 to Swim5: pieces of the SWIM data set (Oliveira et al., 2000), in the
  sheets of the paper (5752 high)

Usage (from the root of the repository, the data being downloaded):

    python3 scripts/convert_irregular_costa2009.py
"""

import json
import math
import os
import os.path


SWIM_PATH = os.path.join(
        "data", "irregular_raw", "oliveira2000", "swim_2007-05-15", "swim.txt")

OUTPUT_DIRECTORY = os.path.join("data", "irregular", "costa2009")

# Cheng and Rao (1999), table 1.
CHENG_VERTICES = [
    (8.69, 1.82), (9.67, 1.85), (10.76, 2.17), (11.94, 2.69), (12.80, 3.45),
    (13.46, 4.57), (13.79, 6.12), (13.75, 7.94), (13.05, 12.19),
    (13.10, 12.65), (13.57, 12.96), (13.69, 13.40), (13.08, 13.76),
    (12.44, 14.08), (12.41, 14.69), (12.97, 15.44), (13.09, 15.91),
    (12.67, 16.07), (12.10, 15.98), (11.71, 15.52), (10.90, 14.15),
    (10.61, 13.94), (9.10, 13.79), (8.61, 13.58), (8.66, 13.29),
    (9.16, 13.04), (9.57, 12.92), (9.30, 12.58), (9.17, 11.78), (8.01, 9.54),
    (6.00, 7.28), (5.55, 6.06), (5.66, 4.22), (6.45, 2.79), (7.67, 2.11),
]

# Stoyan and Pankratov (1999), example of section 5.
STOYAN_VERTICES = [
    (-73.15, 7.50), (-69.65, -3.30), (-59.35, -10.60), (-5.25, -16.00),
    (34.65, -30.50), (55.75, -31.60), (65.45, -27.70), (71.85, -16.00),
    (72.85, -1.00), (69.25, 9.00), (56.75, 22.00), (37.75, 29.50),
    (12.75, 31.80), (-57.25, 29.10), (-68.35, 22.10), (-72.85, 13.00),
    (-73.25, 10.00),
]

# Height of the sheets of the Swim instances.
SWIM_SHEET_HEIGHT = 5752

# Swim instances: name, SWIM piece, sheet width.
# The paper doesn't give the piece of each instance; they are identified from
# the layouts of its figures 14 to 18 and from the utilisations it reports.
SWIM_INSTANCES = [
    ("swim1", 5, 25000),
    ("swim2", 6, 20000),
    ("swim3", 7, 22000),
    ("swim4", 8, 17000),
    ("swim5", 9, 7000),
]


def read_swim_pieces(path):
    """Return the vertices of the pieces of the SWIM data set, by piece id."""
    pieces = {}
    with open(path, "r") as f:
        lines = [line.strip() for line in f]
    pos = 0
    while pos < len(lines):
        if not lines[pos].startswith("PIECE"):
            pos += 1
            continue
        piece_id = int(lines[pos].split()[1])
        # PIECE, QUANTITY, <quantity>, NUMBER OF VERTICES, <n>, VERTICES (X,Y)
        number_of_vertices = int(lines[pos + 4])
        vertices = []
        for line in lines[pos + 6: pos + 6 + number_of_vertices]:
            x, y = line.split()
            vertices.append((float(x), float(y)))
        pieces[piece_id] = vertices
        pos += 6 + number_of_vertices
    return pieces


def area(vertices):
    a = 0
    for (x1, y1), (x2, y2) in zip(vertices, vertices[1:] + vertices[:1]):
        a += x1 * y2 - x2 * y1
    return abs(a) / 2


def write_instance(name, vertices, sheet_width, sheet_height):
    elements = []
    for (x1, y1), (x2, y2) in zip(vertices, vertices[1:] + vertices[:1]):
        elements.append({
            "type": "line_segment",
            "start": {"x": x1, "y": y1},
            "end": {"x": x2, "y": y2}})
    # As many copies as the area of the sheet allows.
    copies = math.floor(sheet_width * sheet_height / area(vertices))
    instance = {
        "objective": "knapsack",
        "bin_types": [{
            "type": "rectangle",
            "width": sheet_width,
            "height": sheet_height,
            "copies": 1}],
        "item_types": [{
            "type": "general",
            "elements": elements,
            "copies": copies,
            "profit": 1,
            # No orientation constraint.
            "allowed_rotations": [
                {"start": 0.0, "end": 360.0, "mirror": False}]}],
    }
    path = os.path.join(OUTPUT_DIRECTORY, name + ".json")
    if not os.path.exists(OUTPUT_DIRECTORY):
        os.makedirs(OUTPUT_DIRECTORY)
    print("Create " + path)
    with open(path, "w") as f:
        json.dump(instance, f, indent=4)


if __name__ == "__main__":
    write_instance("cheng", CHENG_VERTICES, 200, 200)
    write_instance("stoyan", STOYAN_VERTICES, 900, 700)
    pieces = read_swim_pieces(SWIM_PATH)
    for name, piece_id, sheet_width in SWIM_INSTANCES:
        write_instance(
                name,
                pieces[piece_id],
                sheet_width,
                SWIM_SHEET_HEIGHT)

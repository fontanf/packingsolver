import os
import os.path
import json


def write_dict(dic, filename):
    p = os.path.join("data", "irregular", filename.replace(" ", "_") + ".json")
    d = os.path.dirname(p)
    if not os.path.exists(d):
        os.makedirs(d)
    print("Create " + p)
    with open(p, "w") as f:
        json.dump(dic, f, indent=4)


def vertices_to_elements(vertices):
    n = len(vertices)
    return [
        {
            "type": "line_segment",
            "start": {"x": float(vertices[i][0]), "y": float(vertices[i][1])},
            "end": {"x": float(vertices[(i + 1) % n][0]), "y": float(vertices[(i + 1) % n][1])},
        }
        for i in range(n)
    ]


def convert_han2012_jotika(dat_filename):
    brd_path = os.path.join("data", "irregular_raw", "han2012_jotika", "stocksheet.brd")
    with open(brd_path) as f:
        brd_tokens = f.read().split()
    bin_width = float(brd_tokens[0])
    bin_height = float(brd_tokens[1])

    dat_path = os.path.join("data", "irregular_raw", "han2012_jotika", dat_filename)
    polygons = {}
    with open(dat_path) as f:
        for line in f:
            parts = line.split()
            if len(parts) < 4 or parts[0] == "polyNo":
                continue
            poly_no = int(parts[0])
            x = float(parts[2])
            y = float(parts[3])
            if poly_no not in polygons:
                polygons[poly_no] = []
            polygons[poly_no].append((x, y))

    bin_elements = vertices_to_elements([
        (0, 0), (bin_width, 0), (bin_width, bin_height), (0, bin_height)
    ])

    dic = {
        "objective": "bin-packing",
        "bin_types": [{
            "type": "general",
            "copies": len(polygons.keys()),
            "elements": bin_elements,
        }],
        "item_types": [],
    }

    for poly_no in sorted(polygons.keys()):
        dic["item_types"].append({
            "type": "general",
            "copies": 1,
            "allowed_rotations": [{"start": 0, "end": 0, "mirror": False}],
            "elements": vertices_to_elements(polygons[poly_no]),
        })

    output_name = os.path.join("han2012_jotika", os.path.splitext(dat_filename)[0])
    write_dict(dic, output_name)


if __name__ == "__main__":
    for dat_filename in [
            "han80.dat",
            "han100.dat",
            "han120.dat",
            "han150.dat",
            "jotika40.dat",
            "jotika50.dat",
            "jotika60.dat",
            "jotika70.dat"]:
        convert_han2012_jotika(dat_filename)

import os
import os.path
import json


def words(filename):
    with open(os.path.join("data", "irregular_raw", "lopez2013_jp1", filename)) as f:
        for line in f:
            for word in line.split():
                yield word


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


def convert_lopez2013_jp1(txt_filename):
    w = words(txt_filename)
    n_items = int(next(w))
    strip_width = float(next(w))
    next(w)  # strip_height_limit (unused)

    bin_elements = vertices_to_elements([
        (0, 0), (strip_width, 0), (strip_width, strip_width), (0, strip_width)
    ])

    dic = {
        "objective": "bin-packing",
        "bin_types": [{
            "type": "general",
            "copies": n_items,
            "elements": bin_elements,
        }],
        "item_types": [],
    }

    for _ in range(n_items):
        n_verts = int(next(w))
        vertices = []
        for _ in range(n_verts):
            x = float(next(w))
            y = float(next(w))
            vertices.append((x, y))
        dic["item_types"].append({
            "type": "general",
            "copies": 1,
            "allowed_rotations": [
                {"start": 0, "end": 0, "mirror": False},
                {"start": 180, "end": 180, "mirror": False},
            ],
            "elements": vertices_to_elements(vertices),
        })

    output_name = os.path.join("lopez2013_jp1", os.path.splitext(txt_filename)[0])
    write_dict(dic, output_name)


if __name__ == "__main__":
    groups = ["TA", "TB", "TC", "TD", "TE", "TF", "TG", "TH", "TI", "TJ",
              "TK", "TL", "TM", "TN", "TO", "TP", "TQ", "TR"]
    for group in groups:
        for i in range(1, 31):
            convert_lopez2013_jp1(f"{group}{i:03d}.txt")

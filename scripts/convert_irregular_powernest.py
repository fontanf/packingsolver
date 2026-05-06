import os
import os.path
import json

from shapely.geometry import Polygon


def write_dict(dic, filename):
    p = os.path.join("data", "irregular", filename.replace(" ", "_") + ".json")
    d = os.path.dirname(p)
    if not os.path.exists(d):
        os.makedirs(d)
    print("Create " + p)
    with open(p, "w") as f:
        json.dump(dic, f, indent=4)


def convert_powernest(filename):
    path = os.path.join("data", "irregular_raw", filename)
    with open(path) as f:
        data = json.load(f)

    # Maximum protection offset across all parts (used for spacing).
    max_protection_offset = max(
        (part.get("protection_offset", 0.0) for part in data["parts"]),
        default=0.0)

    # Build bin types from sheets.
    # item_bin_minimum_spacing accounts for both the sheet border_gap and the
    # part protection_offset (since each part keeps its own offset from the border).
    bin_types = []
    for sheet in data["sheets"]:
        border_gap = sheet.get("border_gap") or 0.0
        bin_type = {
            "type": "rectangle",
            "width": sheet["length"],
            "height": sheet["height"],
            "copies": sheet.get("quantity", 1),
            "item_bin_minimum_spacing": border_gap + max_protection_offset,
        }
        bin_types.append(bin_type)

    objective = (
        "variable-sized-bin-packing"
        if len(bin_types) > 1
        else "bin-packing-with-leftovers"
    )

    # Build item types from parts.
    # Each element in part["geometry"] is an outer polygon (item shape).
    # Each element in part["holes"] is a hole that belongs to one of those
    # shapes; we determine which one by containment testing.
    item_types = []
    for part in data["parts"]:
        geometry = part["geometry"]
        holes = part.get("holes", [])

        # Build one shape dict per geometry element.
        shapes = [
            {
                "type": "polygon",
                "vertices": [{"x": pt[0], "y": pt[1]} for pt in geom],
            }
            for geom in geometry
        ]

        # Assign each hole to the geometry polygon that contains it.
        # We test full containment of the hole polygon (not just a single
        # point) to handle cases where geometry elements partially overlap.
        if holes:
            geom_polygons = [Polygon(geom) for geom in geometry]
            for hole in holes:
                hole_polygon = Polygon(hole)
                for shape_dict, geom_polygon in zip(shapes, geom_polygons):
                    if geom_polygon.contains(hole_polygon):
                        shape_dict.setdefault("holes", []).append({
                            "type": "polygon",
                            "vertices": [{"x": pt[0], "y": pt[1]} for pt in hole],
                        })
                        break

        for instance in part["instances"]:
            quantity = instance.get("quantity", 1)
            orientations = instance.get("orientations", [])

            # Build allowed_rotations from each orientation, preserving its
            # flip (mirror) flag.  Each (start, end, mirror) triple is
            # deduplicated.
            seen_keys = set()
            allowed_rotations = []
            for o in orientations:
                mirror = o.get("flip", False)
                if "min_angle" in o:
                    max_a = o["max_angle"]
                    # Powernest uses 359.9 to mean "full rotation"; map to 360.
                    if max_a >= 359.9:
                        max_a = 360.0
                    key = (o["min_angle"], max_a, mirror)
                    if key not in seen_keys:
                        seen_keys.add(key)
                        allowed_rotations.append({
                            "start": o["min_angle"],
                            "end": max_a,
                            "mirror": mirror,
                        })
                elif "angle" in o:
                    angle = o["angle"]
                    # Add discrete angle only if not already covered by a range
                    # entry with the same mirror flag.
                    covered = any(
                        r["mirror"] == mirror
                        and r["start"] <= angle <= r["end"]
                        for r in allowed_rotations
                    )
                    key = (angle, angle, mirror)
                    if not covered and key not in seen_keys:
                        seen_keys.add(key)
                        allowed_rotations.append({
                            "start": angle,
                            "end": angle,
                            "mirror": mirror,
                        })

            allowed_rotations.sort(key=lambda r: (r["mirror"], r["start"]))

            if len(shapes) == 1:
                item_type = dict(shapes[0])
            else:
                item_type = {"shapes": shapes}
            item_type["copies"] = quantity
            item_type["allowed_rotations"] = allowed_rotations

            item_types.append(item_type)

    dic = {
        "objective": objective,
        "bin_types": bin_types,
        "item_types": item_types,
    }
    # Add item-to-item spacing only when protection offsets are present.
    if max_protection_offset > 0.0:
        dic["parameters"] = {"item_item_minimum_spacing": max_protection_offset}

    write_dict(dic, filename)


if __name__ == "__main__":
    for i in range(1, 11):
        convert_powernest(os.path.join("alma2025", f"alma_{i:02d}.json"))

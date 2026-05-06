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


def convert_calendar_puzzle():
    from shapely.ops import unary_union

    month2cell = {
        "JAN": (0, 6), "FEB": (1, 6), "MAR": (2, 6),
        "APR": (3, 6), "MAY": (4, 6), "JUN": (5, 6),
        "JUL": (0, 5), "AUG": (1, 5), "SEP": (2, 5),
        "OCT": (3, 5), "NOV": (4, 5), "DEC": (5, 5),
    }
    day2cell = {
        1: (0, 4), 2: (1, 4), 3: (2, 4), 4: (3, 4), 5: (4, 4), 6: (5, 4), 7: (6, 4),
        8: (0, 3), 9: (1, 3), 10: (2, 3), 11: (3, 3), 12: (4, 3), 13: (5, 3), 14: (6, 3),
        15: (0, 2), 16: (1, 2), 17: (2, 2), 18: (3, 2), 19: (4, 2), 20: (5, 2), 21: (6, 2),
        22: (0, 1), 23: (1, 1), 24: (2, 1), 25: (3, 1), 26: (4, 1), 27: (5, 1), 28: (6, 1),
        29: (0, 0), 30: (1, 0), 31: (2, 0),
    }

    # Board polygon (hardcoded): rows y=5,6 are 6 wide (x=0..5), rows y=1..4 are 7 wide
    # (x=0..6), row y=0 is 3 wide (x=0..2). Vertices in CCW order.
    board_vertices = [
        {"x": 0, "y": 0},
        {"x": 3, "y": 0},
        {"x": 3, "y": 1},
        {"x": 7, "y": 1},
        {"x": 7, "y": 5},
        {"x": 6, "y": 5},
        {"x": 6, "y": 7},
        {"x": 0, "y": 7},
    ]

    # Canonical (first rotation) cells for each piece
    items_canonical_cells = [
        [(0, 0), (0, 1), (1, 0), (1, 1), (2, 0), (2, 1)],  # 0: 2x3 rectangle
        [(0, 0), (0, 1), (0, 2), (1, 0), (2, 0)],            # 1: L-shape
        [(0, 0), (0, 1), (0, 2), (1, 0), (1, 1)],            # 2: P-shape (needs mirroring)
        [(0, 0), (0, 1), (0, 2), (0, 3), (1, 0)],            # 3: L-pentomino (needs mirroring)
        [(0, 0), (0, 1), (0, 2), (1, 0), (1, 2)],            # 4: U-shape
        [(0, 0), (0, 1), (0, 2), (1, 2), (1, 3)],            # 5: Z/S-shape (needs mirroring)
        [(0, 0), (0, 1), (0, 2), (0, 3), (1, 2)],            # 6: Y-shape (needs mirroring)
        [(0, 0), (0, 1), (0, 2), (1, 1), (2, 1)],            # 7: T-shape
    ]
    items_allow_mirroring = [False, False, True, True, False, True, True, False]
    base_rotations = [
        {"start": 0, "end": 0, "mirror": False},
        {"start": 90, "end": 90, "mirror": False},
        {"start": 180, "end": 180, "mirror": False},
        {"start": 270, "end": 270, "mirror": False},
    ]
    base_rotations_mirrored = base_rotations + [
        {"start": 0, "end": 0, "mirror": True},
        {"start": 90, "end": 90, "mirror": True},
        {"start": 180, "end": 180, "mirror": True},
        {"start": 270, "end": 270, "mirror": True},
    ]

    def cells_to_vertices(cells):
        squares = [
            Polygon([(x, y), (x + 1, y), (x + 1, y + 1), (x, y + 1)])
            for x, y in cells
        ]
        union = unary_union(squares)
        coords = list(union.exterior.coords)[:-1]  # remove closing repeated point
        coords = coords[::-1]  # convert to anticlockwise
        return [{"x": float(cx), "y": float(cy)} for cx, cy in coords]

    item_vertices_list = [cells_to_vertices(cells) for cells in items_canonical_cells]

    days_per_month = {
        "JAN": 31, "FEB": 28, "MAR": 31, "APR": 30,
        "MAY": 31, "JUN": 30, "JUL": 31, "AUG": 31,
        "SEP": 30, "OCT": 31, "NOV": 30, "DEC": 31,
    }

    for month, num_days in days_per_month.items():
        month_cell = month2cell[month]
        for day in range(1, num_days + 1):
            day_cell = day2cell[day]

            defects = [
                {
                    "type": "polygon",
                    "vertices": [
                        {"x": month_cell[0],     "y": month_cell[1]},
                        {"x": month_cell[0] + 1, "y": month_cell[1]},
                        {"x": month_cell[0] + 1, "y": month_cell[1] + 1},
                        {"x": month_cell[0],     "y": month_cell[1] + 1},
                    ],
                },
                {
                    "type": "polygon",
                    "vertices": [
                        {"x": day_cell[0],     "y": day_cell[1]},
                        {"x": day_cell[0] + 1, "y": day_cell[1]},
                        {"x": day_cell[0] + 1, "y": day_cell[1] + 1},
                        {"x": day_cell[0],     "y": day_cell[1] + 1},
                    ],
                },
            ]

            item_types = []
            for vertices, allow_mirroring in zip(item_vertices_list, items_allow_mirroring):
                item_type = {
                    "type": "polygon",
                    "copies": 1,
                    "vertices": vertices,
                    "allowed_rotations": base_rotations_mirrored if allow_mirroring else base_rotations,
                }
                item_types.append(item_type)

            dic = {
                "objective": "feasibility",
                "bin_types": [
                    {
                        "type": "polygon",
                        "vertices": board_vertices,
                        "defects": defects,
                    }
                ],
                "item_types": item_types,
            }

            write_dict(dic, f"calendar_puzzle/{month}_{day:02d}")


if __name__ == "__main__":
    convert_calendar_puzzle()

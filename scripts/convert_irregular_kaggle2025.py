import os
import os.path
import json

from decimal import Decimal, getcontext
from shapely.geometry import Polygon


def write_dict(dic, filename):
    p = os.path.join("data", "irregular", filename.replace(" ", "_") + ".json")
    d = os.path.dirname(p)
    if not os.path.exists(d):
        os.makedirs(d)
    print("Create " + p)
    with open(p, "w") as f:
        json.dump(dic, f, indent=4)


getcontext().prec = 60
scale_factor = Decimal("1")

# Taken from https://www.kaggle.com/code/inversion/santa-2025-getting-started
class ChristmasTree:
    """Represents a single, rotatable Christmas tree of a fixed size."""

    def __init__(self, center_x="0", center_y="0", angle="0"):
        """Initializes the Christmas tree with a specific position and rotation."""
        self.center_x = Decimal(center_x)
        self.center_y = Decimal(center_y)
        self.angle = Decimal(angle)

        trunk_w = Decimal("0.15")
        trunk_h = Decimal("0.2")
        base_w = Decimal("0.7")
        mid_w = Decimal("0.4")
        top_w = Decimal("0.25")
        tip_y = Decimal("0.8")
        tier_1_y = Decimal("0.5")
        tier_2_y = Decimal("0.25")
        base_y = Decimal("0.0")
        trunk_bottom_y = -trunk_h

        initial_polygon = Polygon(
            [
                # Start at Tip
                (Decimal("0.0") * scale_factor, tip_y * scale_factor),
                # Right side - Top Tier
                (top_w / Decimal("2") * scale_factor, tier_1_y * scale_factor),
                (top_w / Decimal("4") * scale_factor, tier_1_y * scale_factor),
                # Right side - Middle Tier
                (mid_w / Decimal("2") * scale_factor, tier_2_y * scale_factor),
                (mid_w / Decimal("4") * scale_factor, tier_2_y * scale_factor),
                # Right side - Bottom Tier
                (base_w / Decimal("2") * scale_factor, base_y * scale_factor),
                # Right Trunk
                (trunk_w / Decimal("2") * scale_factor, base_y * scale_factor),
                (trunk_w / Decimal("2") * scale_factor, trunk_bottom_y * scale_factor),
                # Left Trunk
                (
                    -(trunk_w / Decimal("2")) * scale_factor,
                    trunk_bottom_y * scale_factor,
                ),
                (-(trunk_w / Decimal("2")) * scale_factor, base_y * scale_factor),
                # Left side - Bottom Tier
                (-(base_w / Decimal("2")) * scale_factor, base_y * scale_factor),
                # Left side - Middle Tier
                (-(mid_w / Decimal("4")) * scale_factor, tier_2_y * scale_factor),
                (-(mid_w / Decimal("2")) * scale_factor, tier_2_y * scale_factor),
                # Left side - Top Tier
                (-(top_w / Decimal("4")) * scale_factor, tier_1_y * scale_factor),
                (-(top_w / Decimal("2")) * scale_factor, tier_1_y * scale_factor),
            ]
        )

        # Add buffer for feasibility issues
        self.polygon = initial_polygon.buffer(0.00001)


def convert_kaggle_tree_to_vertices() -> list[dict]:
    christmas_tree_sample = ChristmasTree()

    exterior_coordinates = list(christmas_tree_sample.polygon.exterior.coords)

    vertices = [{"x": round(x, 6), "y": round(y, 6)}
                for x, y in exterior_coordinates]

    # Solver expected format does not repeat the first and last point
    vertices = vertices[:-1]

    # Convert to counter clockwise
    vertices = vertices[::-1]

    return vertices


def convert_kaggle2025():
    sample_kaggle_tree_vertices: list[dict] = convert_kaggle_tree_to_vertices()

    for num_trees in range(1, 201):
        item_type = {
            "type": "polygon",
            "copies": num_trees,
            "vertices": sample_kaggle_tree_vertices,
            "allowed_rotations": [{"start": 0, "end": 360, "mirror": False}],
        }
        dic = {
            "objective": "open-dimension-xy",
            "parameters": {"open_dimension_xy_aspect_ratio": 1},
            "bin_types": [
                {
                    "type": "rectangle",
                    "height": 200,  # Dummy value for now
                    "width": 100,  # Dummy value for now
                }
            ],
            "item_types": [item_type],
        }

        write_dict(dic, f"kaggle2025/{num_trees}_trees")


if __name__ == "__main__":
    convert_kaggle2025()

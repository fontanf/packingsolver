import argparse
import csv
import pathlib
import urllib.request
from collections import defaultdict
from decimal import Decimal, ROUND_HALF_UP


DEFAULT_CARTONS_URL = (
    "https://packrift.github.io/packaging-optimization-benchmark-corpus/"
    "data/cartonization-fixtures/fixture_cartons.csv"
)
DEFAULT_ORDERS_URL = (
    "https://packrift.github.io/packaging-optimization-benchmark-corpus/"
    "data/cartonization-fixtures/fixture_orders.csv"
)
ROTATION_COLUMNS = [
    "ROTATION_XYZ",
    "ROTATION_YXZ",
    "ROTATION_ZYX",
    "ROTATION_YZX",
    "ROTATION_XZY",
    "ROTATION_ZXY",
]


def read_csv(path_or_url):
    if path_or_url.startswith(("http://", "https://")):
        with urllib.request.urlopen(path_or_url) as response:
            lines = response.read().decode("utf-8-sig").splitlines()
    else:
        with open(path_or_url, newline="", encoding="utf-8-sig") as f:
            lines = f.read().splitlines()
    return list(csv.DictReader(lines))


def scale_length(value, scale):
    scaled = Decimal(value) * Decimal(scale)
    return int(scaled.to_integral_value(rounding=ROUND_HALF_UP))


def order_sort_key(order_id):
    return [
        (0, int(part)) if part.isdigit() else (1, part)
        for part in order_id.split("-")
    ]


def write_rows(path, fieldnames, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def convert(args):
    output_dir = pathlib.Path(args.output_dir)
    cartons = read_csv(args.cartons)
    order_rows = read_csv(args.orders)

    grouped_orders = defaultdict(list)
    for row in order_rows:
        grouped_orders[row["order_id"]].append(row)

    rotation_values = {column: 1 for column in ROTATION_COLUMNS}
    if args.no_item_rotation:
        rotation_values = {column: 0 for column in ROTATION_COLUMNS}
        rotation_values["ROTATION_XYZ"] = 1

    bin_rows = []
    for carton_id, row in enumerate(cartons):
        x = scale_length(row["length_in"], args.scale)
        y = scale_length(row["width_in"], args.scale)
        z = scale_length(row["height_in"], args.scale)
        cost = x * y * z if args.bin_cost == "volume" else args.bin_cost_value
        bin_rows.append({
            "ID": carton_id,
            "X": x,
            "Y": y,
            "Z": z,
            "COST": cost,
            "COPIES": args.bin_copies,
        })

    for order_id in sorted(grouped_orders, key=order_sort_key):
        item_rows = []
        for item_id, row in enumerate(grouped_orders[order_id]):
            x = scale_length(row["item_length_in"], args.scale)
            y = scale_length(row["item_width_in"], args.scale)
            z = scale_length(row["item_height_in"], args.scale)
            item = {
                "ID": item_id,
                "X": x,
                "Y": y,
                "Z": z,
                "COPIES": int(row.get("item_count") or 1),
                "PROFIT": x * y * z,
            }
            item.update(rotation_values)
            item_rows.append(item)

        instance_dir = output_dir / order_id
        write_rows(
            instance_dir / "bins.csv",
            ["ID", "X", "Y", "Z", "COST", "COPIES"],
            bin_rows)
        write_rows(
            instance_dir / "items.csv",
            ["ID", "X", "Y", "Z", "COPIES", "PROFIT"] + ROTATION_COLUMNS,
            item_rows)
        write_rows(
            instance_dir / "parameters.csv",
            ["NAME", "VALUE"],
            [{"NAME": "objective", "VALUE": args.objective}])
        print(f"Created {instance_dir}")


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Convert Packrift public ecommerce cartonization fixtures to "
            "packingsolver box instances."
        ))
    parser.add_argument(
        "--cartons",
        default=DEFAULT_CARTONS_URL,
        help="Packrift carton CSV path or URL.")
    parser.add_argument(
        "--orders",
        default=DEFAULT_ORDERS_URL,
        help="Packrift order fixture CSV path or URL.")
    parser.add_argument(
        "--output-dir",
        default="data/box/packrift/cartonization-fixtures",
        help="Directory where generated packingsolver instances are written.")
    parser.add_argument(
        "--scale",
        type=int,
        default=1000,
        help=(
            "Multiplier for converting inch decimals to integer length units. "
            "The default stores thousandths of an inch."
        ))
    parser.add_argument(
        "--objective",
        default="variable-sized-bin-packing",
        help="Objective written to each generated parameters.csv file.")
    parser.add_argument(
        "--bin-copies",
        type=int,
        default=1,
        help="Available copies of each carton type per generated order instance.")
    parser.add_argument(
        "--bin-cost",
        choices=["volume", "constant"],
        default="volume",
        help="Cost strategy for carton bins.")
    parser.add_argument(
        "--bin-cost-value",
        type=int,
        default=1,
        help="Cost value used when --bin-cost=constant.")
    parser.add_argument(
        "--no-item-rotation",
        action="store_true",
        help="Allow only XYZ item orientation instead of all six rotations.")
    return parser.parse_args()


if __name__ == "__main__":
    convert(parse_args())

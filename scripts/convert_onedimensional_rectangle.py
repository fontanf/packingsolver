"""
Convert a 'rectangle' instance directory into a 'onedimensional' instance
directory by replacing each item/bin's WIDTH/HEIGHT columns with a single X
column equal to its area (WIDTH * HEIGHT). All other columns (COST, COPIES,
COPIES_MIN, ...) are kept unchanged.

Usage:
    python3 scripts/convert_onedimensional_rectangle.py <source_directory> <destination_directory>

Example:
    python3 scripts/convert_onedimensional_rectangle.py \\
        data/rectangle/pisinger2005 data/onedimensional/pisinger2005
"""

import argparse
import csv
import os


def area(width_str, height_str):
    width = float(width_str)
    height = float(height_str)
    value = width * height
    if value == int(value):
        value = int(value)
    return value


def convert_file(source_path, destination_path):
    with open(source_path, newline="") as f:
        reader = csv.DictReader(f)
        if "WIDTH" not in reader.fieldnames or "HEIGHT" not in reader.fieldnames:
            print(f"Skip {source_path}: no WIDTH/HEIGHT columns")
            return
        new_fieldnames = []
        for column in reader.fieldnames:
            if column == "WIDTH":
                new_fieldnames.append("X")
            elif column == "HEIGHT":
                continue
            else:
                new_fieldnames.append(column)
        rows = []
        for row in reader:
            new_row = {}
            for column in reader.fieldnames:
                if column == "WIDTH":
                    new_row["X"] = area(row["WIDTH"], row["HEIGHT"])
                elif column == "HEIGHT":
                    continue
                else:
                    new_row[column] = row[column]
            rows.append(new_row)

    with open(destination_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=new_fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    print(f"Converted {source_path} -> {destination_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_directory")
    parser.add_argument("destination_directory")
    args = parser.parse_args()

    os.makedirs(args.destination_directory, exist_ok=True)
    for filename in sorted(os.listdir(args.source_directory)):
        if not filename.endswith(".csv"):
            continue
        convert_file(
                os.path.join(args.source_directory, filename),
                os.path.join(args.destination_directory, filename))

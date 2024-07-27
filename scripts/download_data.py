import argparse
import gdown
import os
import shutil
import pathlib
import time
import sys


def download(id):
    for _ in range(3):
        try:
            gdown.download(id=id, output="data.7z")
            os.system("7z x data.7z")
            pathlib.Path("data.7z").unlink()
        except:
            time.sleep(10)
            continue
        return
    sys.exit(1)


parser = argparse.ArgumentParser(description='')
parser.add_argument(
        "-d", "--data",
        type=str,
        nargs='*',
        help='')
args = parser.parse_args()


if args.data is None or "imahori2010" in args.data:
    # imahori2010
    download("1vo3wnve24--IyaB7mX9k1RWqGEqElhSY")
    dir_path = pathlib.Path("data") / "rectangle_raw" / "imahori2010"
    if dir_path.exists():
        shutil.rmtree(dir_path)
    shutil.move("imahori2010", dir_path)

    # imahori2010_packingsolver
    download("18I4Muyp0cfms_j-ucPgIwBEHrJOPXdEe")
    dir_path = pathlib.Path("data") / "rectangle" / "imahori2010"
    if dir_path.exists():
        shutil.rmtree(dir_path)
    shutil.move("imahori2010_packingsolver", dir_path)


if args.data is None or "cgshop2024" in args.data:
    # cgshop2024
    download("1fO9Es7Zly2fAKHZABttFZYJEj4yNN-Yf")
    dir_path = pathlib.Path("data") / "irregular_raw" / "cgshop2024"
    if dir_path.exists():
        shutil.rmtree(dir_path)
    shutil.move("cgshop2024", dir_path)

    # cgshop2024_packingsolver
    download("1ly3HH5qrnWHHtwwhnVS5hZdnKq0yCcmh")
    dir_path = pathlib.Path("data") / "irregular" / "cgshop2024"
    if dir_path.exists():
        shutil.rmtree(dir_path)
    shutil.move("cgshop2024_packingsolver", dir_path)

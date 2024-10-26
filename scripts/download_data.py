import argparse
import gdown
import os
import shutil
import pathlib
import time
import sys


def download(file_id, file_format="7z"):
    for _ in range(3):
        try:
            if file_format == "7z":
                gdown.download(id=file_id, output="data.7z")
                os.system("7z x data.7z")
                pathlib.Path("data.7z").unlink()
            elif file_format == "tar.gz":
                gdown.download(id=file_id, output="data.tar.gz")
                os.system("tar zxf data.tar.gz")
                pathlib.Path("data.tar.gz").unlink()
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


if args.data is None or "roadef2022_2024-04-25_kp" in args.data:
    # roadef2022_2024-04-25_kp
    download("1pYlKC0zwrg8sd9Ph1HOJpMLwEXe8_YLb", "tar.gz")
    dir_path = pathlib.Path("data") / "boxstacks" / "roadef2022_2024-04-25_kp"
    if dir_path.exists():
        shutil.rmtree(dir_path)
    shutil.move("roadef2022_2024-04-25_kp", dir_path)

if args.data is None or "roadef2022_2024-04-25_bpp" in args.data:
    # roadef2022_2024-04-25_bpp
    download("1w8Q6S680FM_R8xKPXJrL1u75wTVFOTCT", "tar.gz")
    dir_path = pathlib.Path("data") / "boxstacks" / "roadef2022_2024-04-25_bpp"
    if dir_path.exists():
        shutil.rmtree(dir_path)
    shutil.move("roadef2022_2024-04-25_bpp", dir_path)


if args.data is None or "cgshop2024" in args.data:
    # cgshop2024
    download("1fO9Es7Zly2fAKHZABttFZYJEj4yNN-Yf")
    dir_path = pathlib.Path("data") / "irregular_raw" / "cgshop2024"
    if dir_path.exists():
        shutil.rmtree(dir_path)
    shutil.move("cgshop2024", dir_path)

    # cgshop2024_packingsolver
    download("1I7tPh8u2-uXs1YVyytnIzSjoW8KcITrD")
    dir_path = pathlib.Path("data") / "irregular" / "cgshop2024"
    if dir_path.exists():
        shutil.rmtree(dir_path)
    shutil.move("cgshop2024_packingsolver", dir_path)

import argparse
import gdown
import os
import shutil
import pathlib
import time
import sys
import py7zr


def download(url, dest, file_format="7z"):
    for attempt in range(3):
        print(f"Downloading {url} (attempt {attempt + 1}/3)...")
        try:
            if file_format == "7z":
                gdown.download(url=url, output="data.7z")
                print(f"Download complete. Extracting to {dest}...")
                with py7zr.SevenZipFile("data.7z", mode="r") as z:
                    z.extractall(path=dest)
                pathlib.Path("data.7z").unlink()
            elif file_format == "tar.gz":
                gdown.download(url=url, output="data.tar.gz")
                print(f"Download complete. Extracting to {dest}...")
                dest.mkdir(parents=True, exist_ok=True)
                os.system(f"tar zxf data.tar.gz -C '{dest}'")
                pathlib.Path("data.tar.gz").unlink()
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(10)
            continue
        print(f"Done.")
        return
    print(f"Failed to download {url} after 3 attempts.")
    sys.exit(1)


parser = argparse.ArgumentParser(description='')
parser.add_argument(
        "-d", "--data",
        type=str,
        nargs='*',
        help='')
args = parser.parse_args()


if args.data is None or "onedimensional" in args.data:
    download(
            "https://drive.google.com/file/d/1JLxZ59PDH-JYsSsB45TpbRaeL_266eif/view?usp=drive_link",
            pathlib.Path("data") / "onedimensional_raw")
    download(
            "https://drive.google.com/file/d/1Gtp8VN1JUaCkPhxtYWOAOaf2YxHz-6H6/view?usp=drive_link",
            pathlib.Path("data") / "onedimensional")


if args.data is None or "imahori2010" in args.data:
    download(
            "https://drive.google.com/file/d/1Wt72KVUSD_WrRUgWgEkSofpQR2pswsrv/view?usp=drive_link",
            pathlib.Path("data") / "rectangle_raw")
    download(
            "https://drive.google.com/file/d/1YPjpvDJzZRPzWjh0bhmu77fv-eWB1KoJ/view?usp=drive_link",
            pathlib.Path("data") / "rectangle")


if args.data is None or "roadef2022_2024-04-25_kp" in args.data:
    download(
        "https://drive.google.com/file/d/1HbK0QJQyDk7fII8jF9nBMP0sD-D1E3N-/view?usp=drive_link",
        pathlib.Path("data") / "boxstacks",
        "tar.gz")

if args.data is None or "roadef2022_2024-04-25_bpp" in args.data:
    download(
        "https://drive.google.com/file/d/1U2MefX8UeCWn0ohFCDpxiCNwRHkAPvFP/view?usp=drive_link",
        pathlib.Path("data") / "boxstacks",
        "tar.gz")


if args.data is None or "irregular" in args.data:
    download(
            "https://drive.google.com/file/d/1aqZlTL1bm1AFs6LNhYGy8fH3Pp4MHFjh/view?usp=drive_link",
            pathlib.Path("data") / "irregular_raw")
    download(
            "https://drive.google.com/file/d/1Qjjp_PifGrSrZSbjAlZsYwWgTIo89kf1/view?usp=drive_link",
            pathlib.Path("data") / "irregular")

if args.data is None or "cgshop2024" in args.data:
    download(
            "https://drive.google.com/file/d/10isw3-gmA0C47Yih9D3HQmdeFiQD2RCw/view?usp=drive_link",
            pathlib.Path("data") / "irregular_raw")
    download(
            "https://drive.google.com/file/d/19zVUpEcxldp6mU6to1Ry1WUl68h71UIk/view?usp=drive_link",
            pathlib.Path("data") / "irregular")

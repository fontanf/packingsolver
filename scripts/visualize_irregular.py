import argparse
import importlib.util
import os

spec = importlib.util.spec_from_file_location(
        "packingsolver_visualize_irregular",
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "..", "python", "packingsolver", "visualize", "irregular.py"))
visualize = importlib.util.module_from_spec(spec)
spec.loader.exec_module(visualize)

parser = argparse.ArgumentParser(description='')
parser.add_argument('csvpath', help='path to JSON file')
parser.add_argument('itemcolor', nargs='?', default='ID', help='color palette used among ["SAME", "ID"]')
parser.add_argument('-o', '--output', help='save image to file instead of opening browser (e.g. output.png)')
parser.add_argument('--width', type=int, default=None, help='image width in pixels for PNG export')
parser.add_argument('--height', type=int, default=None, help='image height in pixels for PNG export')
parser.add_argument('--columns', type=int, default=None, help='number of columns in the subplot grid')
parser.add_argument('--scale', type=float, default=1.0, help='scale factor for cell dimensions')
args = parser.parse_args()

fig = visualize.figure(
        args.csvpath,
        item_color=args.itemcolor,
        columns=args.columns)

if args.output:
    export_width, export_height = visualize.export_size(
            args.csvpath,
            columns=args.columns,
            scale=args.scale,
            width=args.width,
            height=args.height)
    fig.write_image(args.output, width=export_width, height=export_height, scale=2)
else:
    fig.show()

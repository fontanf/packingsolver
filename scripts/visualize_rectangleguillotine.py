import argparse
import importlib.util
import math
import os

# Load the visualization module by file path, so that this script works
# without the packingsolver package being installed.
spec = importlib.util.spec_from_file_location(
        "packingsolver_visualize_rectangleguillotine",
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "..", "python", "packingsolver", "visualize",
            "rectangleguillotine.py"))
visualize = importlib.util.module_from_spec(spec)
spec.loader.exec_module(visualize)

parser = argparse.ArgumentParser(description='')
parser.add_argument('csvpath', help='path to CSV file')
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
    # Retrieve the grid size and the largest bin dimensions from the bin
    # traces (one "Bins" trace per bin, containing its rectangle).
    bins = [trace for trace in fig.data if trace.name == "Bins"]
    m = len(bins)
    number_of_cols = args.columns if args.columns is not None else math.ceil(math.sqrt(m))
    number_of_rows = math.ceil(m / number_of_cols)
    max_bin_lx = max([0] + [max(x for x in b.x if x is not None) - min(x for x in b.x if x is not None) for b in bins])
    max_bin_ly = max([0] + [max(y for y in b.y if y is not None) - min(y for y in b.y if y is not None) for b in bins])
    cell_width = int(max_bin_lx * args.scale)
    cell_height = int(max_bin_ly * args.scale)
    export_width = args.width if args.width is not None else number_of_cols * cell_width + 160
    export_height = args.height if args.height is not None else number_of_rows * cell_height + 170
    fig.write_image(args.output, width=export_width, height=export_height, scale=2)
else:
    fig.show()

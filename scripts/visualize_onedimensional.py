import argparse
import importlib.util
import os

spec = importlib.util.spec_from_file_location(
        "packingsolver_visualize_onedimensional",
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "..", "python", "packingsolver", "visualize", "onedimensional.py"))
visualize = importlib.util.module_from_spec(spec)
spec.loader.exec_module(visualize)

parser = argparse.ArgumentParser(description='')
parser.add_argument('csvpath', help='path to CSV file')
parser.add_argument('itemcolor', nargs='?', default='ID', help='color palette used among ["SAME", "ID"]')
parser.add_argument('-o', '--output', help='save image to file instead of opening browser (e.g. output.png)')
parser.add_argument('--width', type=int, default=None, help='image width in pixels for PNG export')
parser.add_argument('--height', type=int, default=None, help='image height in pixels for PNG export')
parser.add_argument('--scale', type=float, default=1.0, help='scale factor for cell dimensions')
parser.add_argument('--expand-copies', action='store_true', help='draw one subplot per bin copy instead of one per bin type')
args = parser.parse_args()

fig = visualize.figure(
        args.csvpath,
        item_color=args.itemcolor,
        expand_copies=args.expand_copies)

if args.output:
    # One "Bins" trace per subplot, each drawing the rectangle of its bin.
    bin_traces = [trace for trace in fig.data if trace.name == "Bins"]
    m = len(bin_traces)
    max_bin_lx = max(
            [0] + [max(x for x in trace.x if x is not None)
                   - min(x for x in trace.x if x is not None)
                   for trace in bin_traces])
    export_width = args.width if args.width is not None else int(max_bin_lx * args.scale) + 80
    export_height = args.height if args.height is not None else int(m * 80 * args.scale) + 170
    fig.write_image(args.output, width=export_width, height=export_height)
else:
    fig.show()

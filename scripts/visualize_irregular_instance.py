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
parser.add_argument('-o', '--output', help='save image to file instead of opening browser (e.g. output.png)')
parser.add_argument('--width', type=int, default=None, help='image width in pixels for PNG export')
parser.add_argument('--height', type=int, default=None, help='image height in pixels for PNG export')
args = parser.parse_args()

fig = visualize.instance_figure(args.csvpath)

if args.output:
    fig.write_image(args.output, width=args.width, height=args.height)
else:
    fig.show()

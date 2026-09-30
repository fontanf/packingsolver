import argparse
import importlib.util
import os

spec = importlib.util.spec_from_file_location(
        "packingsolver_visualize_boxstacks",
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "..", "python", "packingsolver", "visualize", "boxstacks.py"))
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
parser.add_argument('--zoom', type=float, default=1.0, help='camera zoom factor (higher: closer/bigger)')
parser.add_argument('--expand-copies', action='store_true', help='draw one subplot per bin copy instead of one per bin type')
parser.add_argument('--autocrop', action='store_true', help='crop exported PNG to its non-white content, with a small margin')
parser.add_argument('--no-legend', action='store_true', help='hide the legend, freeing up space for the plot itself')
args = parser.parse_args()

fig = visualize.figure(
        args.csvpath,
        item_color=args.itemcolor,
        columns=args.columns,
        zoom=args.zoom,
        expand_copies=args.expand_copies,
        legend=not args.no_legend)

if args.output:
    export_width, export_height = visualize.export_size(
            args.csvpath,
            columns=args.columns,
            scale=args.scale,
            expand_copies=args.expand_copies,
            width=args.width,
            height=args.height)
    fig.write_image(args.output, width=export_width, height=export_height, scale=2)
    if args.autocrop:
        from PIL import Image, ImageChops
        margin = 20
        im = Image.open(args.output).convert("RGB")
        bbox = ImageChops.difference(im, Image.new("RGB", im.size, (255, 255, 255))).getbbox()
        if bbox is not None:
            x1, y1, x2, y2 = bbox
            x1, y1 = max(0, x1 - margin), max(0, y1 - margin)
            x2, y2 = min(im.width, x2 + margin), min(im.height, y2 + margin)
            im.crop((x1, y1, x2, y2)).save(args.output)
else:
    fig.show()

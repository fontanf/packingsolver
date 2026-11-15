import argparse
import importlib.util
import math
import os

spec = importlib.util.spec_from_file_location(
        "packingsolver_visualize_box",
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "..", "python", "packingsolver", "visualize", "box.py"))
visualize_box = importlib.util.module_from_spec(spec)
spec.loader.exec_module(visualize_box)

parser = argparse.ArgumentParser(description='')
parser.add_argument('csvpath', help='path to CSV file')
parser.add_argument('itemcolor', nargs='?', default='ID', help='color palette used among ["SAME", "ID"]')
parser.add_argument('-o', '--output', help='save image to file instead of opening browser (e.g. output.png)')
parser.add_argument('--width', type=int, default=None, help='image width in pixels for PNG export')
parser.add_argument('--height', type=int, default=None, help='image height in pixels for PNG export')
parser.add_argument('--columns', type=int, default=None, help='number of columns in the subplot grid')
parser.add_argument('--scale', type=float, default=1.0, help='scale factor for cell dimensions')
parser.add_argument('--zoom', type=float, default=1.0, help='camera zoom factor (higher: closer/bigger)')
parser.add_argument('--autocrop', action='store_true', help='crop exported PNG to its non-white content, with a small margin')
parser.add_argument('--no-legend', action='store_true', help='hide the legend, freeing up space for the plot itself')
args = parser.parse_args()

if args.output:
    import csv
    import io
    from PIL import Image, ImageChops

    # Each bin is exported as a separate figure, and the images are assembled
    # in a grid: in a figure with several 3D scenes, the depth of the items
    # of a scene may hide the items of the next ones.
    with open(args.csvpath, newline='') as csvfile:
        rows = list(csv.DictReader(csvfile))
    bins = [row for row in rows if row["TYPE"] == "BIN"]
    m = len(bins)
    number_of_cols = args.columns if args.columns is not None else math.ceil(math.sqrt(m))
    number_of_rows = math.ceil(m / number_of_cols)
    max_bin_lx = max([0] + [int(row["LX"]) for row in bins])
    max_bin_ly = max([0] + [int(row["LY"]) for row in bins])
    # Size of the plot area of each bin, and fixed margins, with room on the
    # right for the legend, so that the scenes have the same size whether
    # their figure has the legend or not.
    margin_left, margin_top, margin_bottom = 40, 40, 40
    margin_right = 40 if args.no_legend else 160
    plot_width = int(max_bin_lx * args.scale)
    plot_height = int(max_bin_ly * args.scale)
    if args.width is not None:
        plot_width = (args.width - margin_left - margin_right) // number_of_cols
    if args.height is not None:
        plot_height = (args.height - margin_top - margin_bottom) // number_of_rows
    image_width = plot_width + margin_left + margin_right
    image_height = plot_height + margin_top + margin_bottom
    images = []
    for bin_pos in range(m):
        fig = visualize_box.figure(
                args.csvpath,
                item_color=args.itemcolor,
                columns=1,
                zoom=args.zoom,
                legend=(not args.no_legend and bin_pos == 0),
                bins=[bin_pos])
        fig.update_layout(margin=dict(
                l=margin_left, r=margin_right, t=margin_top, b=margin_bottom))
        stream = io.BytesIO(fig.to_image(
                format="png", width=image_width, height=image_height, scale=2))
        images.append(Image.open(stream).convert("RGB"))
    image_width, image_height = images[0].size
    im = Image.new("RGB", (number_of_cols * image_width, number_of_rows * image_height), (255, 255, 255))
    for bin_pos, image in enumerate(images):
        im.paste(image, ((bin_pos % number_of_cols) * image_width, (bin_pos // number_of_cols) * image_height))
    if args.autocrop:
        margin = 20
        bbox = ImageChops.difference(im, Image.new("RGB", im.size, (255, 255, 255))).getbbox()
        if bbox is not None:
            x1, y1, x2, y2 = bbox
            x1, y1 = max(0, x1 - margin), max(0, y1 - margin)
            x2, y2 = min(im.width, x2 + margin), min(im.height, y2 + margin)
            im = im.crop((x1, y1, x2, y2))
    im.save(args.output)
else:
    fig = visualize_box.figure(
            args.csvpath,
            item_color=args.itemcolor,
            columns=args.columns,
            zoom=args.zoom,
            legend=not args.no_legend)
    fig.show()

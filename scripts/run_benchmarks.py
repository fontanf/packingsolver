import os
import csv
import argparse
import sys
import datetime


def run_command(command):
    print(command)
    status = os.system(command)
    if status != 0:
        sys.exit(1)
    print()


rectangleguillotine_main = os.path.join(
        "install",
        "bin",
        "packingsolver_rectangleguillotine")

rectangle_main = os.path.join(
        "install",
        "bin",
        "packingsolver_rectangle")


if __name__ == "__main__":

    parser = argparse.ArgumentParser(
            prog='run_benchmarks',
            usage='%(prog)s [options]')
    parser.add_argument('benchmark', help='benchmark to run')
    parser.add_argument(
            '--directory',
            help='benchmark directory',
            default="benchmark_results")
    parser.add_argument('--name', help='output directory', default=None)
    parser.add_argument('--options', help='options', default="")
    args = parser.parse_args()

    benchmark = args.benchmark
    options = args.options
    output_directory = os.path.join(args.directory, benchmark)
    if args.name:
        output_directory = os.path.join(output_directory, args.name)
    else:
        date = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        output_directory = os.path.join(output_directory, date)


    if benchmark == "rectangle_bin_packing_oriented":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_oriented.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                if not row["Items"]:
                    break

                instance_path = os.path.join(
                        data_dir,
                        row["Items"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Items"] + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Items"] + "_solution.csv")
                if not os.path.exists(os.path.dirname(certificate_path)):
                    os.makedirs(os.path.dirname(certificate_path))

                command = (
                        rectangle_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --bin-infinite-copies"
                        + " --no-item-rotation"
                        + " --objective bin-packing"
                        + ("  " + options if options else "")
                        + "  --output \"" + json_output_path + "\""
                        + "  --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangle_bin_packing_rotation":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_rotation.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                if not row["Items"]:
                    break

                instance_path = os.path.join(
                        data_dir,
                        row["Items"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Items"] + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Items"] + "_solution.csv")
                if not os.path.exists(os.path.dirname(certificate_path)):
                    os.makedirs(os.path.dirname(certificate_path))

                command = (
                        rectangle_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --bin-infinite-copies"
                        + " --objective bin-packing"
                        + ("  " + options if options else "")
                        + "  --output \"" + json_output_path + "\""
                        + "  --certificate \"" + certificate_path + "\"")
                run_command(command)

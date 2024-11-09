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
            '--sub',
            help='sub-benchmark',
            default=None)
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


    if benchmark == "rectangleguillotine_roadef2018":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_roadef2018.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                if not row["Path"]:
                    break
                if args.sub and args.sub != row["Dataset"][-1]:
                    continue

                instance_path = os.path.join(
                        data_dir,
                        row["Path"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Path"] + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Path"] + "_solution.csv")
                if not os.path.exists(os.path.dirname(certificate_path)):
                    os.makedirs(os.path.dirname(certificate_path))

                command = (
                        rectangleguillotine_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --objective bin-packing-with-leftovers"
                        + " --predefined roadef2018"
                        + "  --optimization-mode anytime"
                        + " --use-tree-search 1"
                        + " --time-limit 3600"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    if benchmark == "rectangleguillotine_bin_packing_3nho":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_3nho.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                if not row["Path"]:
                    break

                instance_path = os.path.join(
                        data_dir,
                        row["Path"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Path"] + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Path"] + "_solution.csv")
                if not os.path.exists(os.path.dirname(certificate_path)):
                    os.makedirs(os.path.dirname(certificate_path))

                command = (
                        rectangleguillotine_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --bin-infinite-copies"
                        + " --objective bin-packing"
                        + " --predefined 3NHO"
                        + ("  " + options if options else "")
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    if benchmark == "rectangleguillotine_bin_packing_3nhr":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_3nhr.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                if not row["Path"]:
                    break

                instance_path = os.path.join(
                        data_dir,
                        row["Path"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Path"] + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Path"] + "_solution.csv")
                if not os.path.exists(os.path.dirname(certificate_path)):
                    os.makedirs(os.path.dirname(certificate_path))

                command = (
                        rectangleguillotine_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --bin-infinite-copies"
                        + " --objective bin-packing"
                        + " --predefined 3NHR"
                        + ("  " + options if options else "")
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    if benchmark == "rectangleguillotine_long2020":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_long2020.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                if not row["Path"]:
                    break
                if args.sub and args.sub != row["Dataset"][-1]:
                    continue

                instance_path = os.path.join(
                        data_dir,
                        row["Path"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Path"] + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Path"] + "_solution.csv")
                if not os.path.exists(os.path.dirname(certificate_path)):
                    os.makedirs(os.path.dirname(certificate_path))

                command = (
                        rectangleguillotine_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --bin-infinite-copies"
                        + " --objective bin-packing"
                        + " --predefined 3NVO"
                        + " --one2cut 1"
                        + " --time-limit 5"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    if benchmark == "rectangle_bin_packing_oriented":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_oriented.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                if not row["Path"]:
                    break

                instance_path = os.path.join(
                        data_dir,
                        row["Path"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Path"] + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Path"] + "_solution.csv")
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
                        + " --certificate \"" + certificate_path + "\"")
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
                if not row["Path"]:
                    break

                instance_path = os.path.join(
                        data_dir,
                        row["Path"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Path"] + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Path"] + "_solution.csv")
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
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)

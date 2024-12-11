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

onedimensional_main = os.path.join(
        "install",
        "bin",
        "packingsolver_onedimensional")

irregular_main = os.path.join(
        "install",
        "bin",
        "packingsolver_irregular")


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


    elif benchmark == "rectangleguillotine_bin_packing_3nho":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_3nho.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

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
                        + "  --time-limit 60"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_bin_packing_3nhr":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_3nhr.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

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
                        + "  --time-limit 60"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_long2020":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_long2020.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
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
                        + " --maximum-number-2-cuts 1"
                        + "  --time-limit 5"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_bin_packing_3hao_cintra2008":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_3hao_cintra2008.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

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
                        + " --predefined 3HAO"
                        + "  --time-limit 60"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_bin_packing_3hao_imahori2005":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_3hao_imahori2005.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

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
                        + " --predefined 3HAO"
                        + "  --time-limit 60"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_bin_packing_3hvo_alvarez2002":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_3hvo_alvarez2002.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

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
                        + " --predefined 3HVO"
                        + "  --time-limit 5"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_bin_packing_3hvo_others":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_bin_packing_3hvo_others.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

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
                        + " --predefined 3HVO"
                        + "  --time-limit 5"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_knapsack_3nvo_alvarez2002":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_knapsack_3nvo_alvarez2002.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

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
                        + " --objective knapsack"
                        + " --predefined 3NVO"
                        + "  --time-limit 60"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_knapsack_3nvo_cui2015":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_knapsack_3nvo_cui2015.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

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
                        + " --objective knapsack"
                        + " --predefined 3NVO"
                        + "  --time-limit 60"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_knapsack_3hao_others":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_knapsack_3hao_others.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

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
                        + " --objective knapsack"
                        + " --predefined 3HAO"
                        + "  --time-limit 2"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_knapsack_3hao_cui2008":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_knapsack_3hao_cui2008.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

                if row["Options"] == "unweighted":
                    s = "_unweighted"
                    options = " --unweighted"
                else:
                    s = ""
                    options = ""

                instance_path = os.path.join(
                        data_dir,
                        row["Path"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Path"] + s + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Path"] + s + "_solution.csv")
                if not os.path.exists(os.path.dirname(certificate_path)):
                    os.makedirs(os.path.dirname(certificate_path))

                command = (
                        rectangleguillotine_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --objective knapsack"
                        + options
                        + " --predefined 3HAO"
                        + "  --time-limit 180"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_knapsack_2nho_2nvo_others":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_knapsack_2nho_2nvo_others.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

                if row["Options"] == "Horizontal":
                    s = "_horizontal"
                    options = " --predefined 2NHO"
                else:
                    s = "_vertical"
                    options = " --predefined 2NVO"

                instance_path = os.path.join(
                        data_dir,
                        row["Path"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Path"] + s + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Path"] + s + "_solution.csv")
                if not os.path.exists(os.path.dirname(certificate_path)):
                    os.makedirs(os.path.dirname(certificate_path))

                command = (
                        rectangleguillotine_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --objective knapsack"
                        + options
                        + "  --time-limit 3"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_knapsack_2nho_2nvo_alvarez2002":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_knapsack_2nho_2nvo_alvarez2002.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

                if row["Options"] == "Horizontal":
                    s = "_horizontal"
                    options = " --predefined 2NHO"
                else:
                    s = "_vertical"
                    options = " --predefined 2NVO"

                instance_path = os.path.join(
                        data_dir,
                        row["Path"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Path"] + s + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Path"] + s + "_solution.csv")
                if not os.path.exists(os.path.dirname(certificate_path)):
                    os.makedirs(os.path.dirname(certificate_path))

                command = (
                        rectangleguillotine_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --objective knapsack"
                        + options
                        + "  --time-limit 5"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangleguillotine_knapsack_2nho_2nvo_hifi2012":

        datacsv_path = os.path.join(
                "data",
                "rectangle",
                "data_knapsack_2nho_2nvo_hifi2012.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

                if row["Options"] == "Horizontal":
                    s = "_horizontal"
                    options = " --predefined 2NHO"
                else:
                    s = "_vertical"
                    options = " --predefined 2NVO"

                instance_path = os.path.join(
                        data_dir,
                        row["Path"])

                json_output_path = os.path.join(
                        output_directory,
                        row["Path"] + s + "_output.json")
                if not os.path.exists(os.path.dirname(json_output_path)):
                    os.makedirs(os.path.dirname(json_output_path))

                certificate_path = os.path.join(
                        output_directory,
                        row["Path"] + s + "_solution.csv")
                if not os.path.exists(os.path.dirname(certificate_path)):
                    os.makedirs(os.path.dirname(certificate_path))

                command = (
                        rectangleguillotine_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --objective knapsack"
                        + options
                        + "  --time-limit 300"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "rectangle_bin_packing_oriented":

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
                        + "  --optimization-mode not-anytime"
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
                        + "  --optimization-mode not-anytime"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark == "onedimensional_gschwind2016":

        datacsv_path = os.path.join(
                "data",
                "onedimensional",
                "data_gschwind2016.csv")

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:

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
                        onedimensional_main
                        + "  --verbosity-level 1"
                        + "  --items \"" + instance_path + "\""
                        + " --bin-infinite-copies"
                        + " --objective bin-packing"
                        + "  --time-limit 60"
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)


    elif benchmark in ("irregular_cgshop2024_100",
                       "irregular_cgshop2024_1000",
                       "irregular_cgshop2024_10000",
                       "irregular_cgshop2024_100000"):

        datacsv_path = os.path.join(
                "data",
                "irregular",
                "data_cgshop2024.csv")

        time_limit = 300
        if benchmark == "irregular_cgshop2024_100":
            time_limit = 300
        elif benchmark == "irregular_cgshop2024_1000":
            time_limit = 900
        elif benchmark == "irregular_cgshop2024_10000":
            time_limit = 3600
        elif benchmark == "irregular_cgshop2024_100000":
            time_limit = 10800

        data_dir = os.path.dirname(os.path.realpath(datacsv_path))
        with open(datacsv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                n = int(row["Number of items"])
                if benchmark == "irregular_cgshop2024_100":
                    if n >= 100:
                        continue
                elif benchmark == "irregular_cgshop2024_1000":
                    if n < 100 or n >= 1000:
                        continue
                elif benchmark == "irregular_cgshop2024_10000":
                    if n < 1000 or n >= 10000:
                        continue
                elif benchmark == "irregular_cgshop2024_100000":
                    if n < 10000:
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
                        irregular_main
                        + "  --verbosity-level 1"
                        + "  --input \"" + instance_path + "\""
                        + "  --time-limit " + str(time_limit)
                        + "  --output \"" + json_output_path + "\""
                        + " --certificate \"" + certificate_path + "\"")
                run_command(command)

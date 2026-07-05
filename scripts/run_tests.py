import argparse
import sys
import os

parser = argparse.ArgumentParser(description='')
parser.add_argument('directory')
parser.add_argument(
        "-t", "--tests",
        type=str,
        nargs='*',
        help='')

args = parser.parse_args()

######################
# rectangleguilotine #
######################

rectangleguillotine_main = os.path.join(
        "install",
        "bin",
        "packingsolver_rectangleguillotine")

if args.tests is None or "rectangleguillotine-single-knapsack-tree-search" in args.tests:
    print("rectangleguillotine, single knapsack, tree search")
    print("-------------------------------------------------")
    print()

    data = [
            (os.path.join("fayard1998", "CU1"), ""),
            (os.path.join("fayard1998", "CU2"), ""),
            (os.path.join("fayard1998", "CW1"), ""),
            (os.path.join("fayard1998", "CW2"), "")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "single_knapsack_tree_search",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective knapsack"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search 1"
                + " --not-anytime-tree-search-queue-size 256"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-single-knapsack-tree-search-maximal-spaces" in args.tests:
    print("rectangleguillotine, single knapsack, tree search maximal spaces")
    print("----------------------------------------------------------------")
    print()

    data = [
            (os.path.join("velasco2019", "P1_100_200_25_1.txt"), ""),
            (os.path.join("velasco2019", "P1_100_200_50_1.txt"), ""),
            (os.path.join("velasco2019", "P1_100_400_25_1.txt"), ""),
            (os.path.join("velasco2019", "P1_100_400_50_1.txt"), ""),
            (os.path.join("velasco2019", "P2_200_100_25_1.txt"), ""),
            (os.path.join("velasco2019", "P2_200_100_50_1.txt"), ""),
            (os.path.join("velasco2019", "P2_400_100_25_1.txt"), ""),
            (os.path.join("velasco2019", "P2_400_100_50_1.txt"), ""),
            (os.path.join("velasco2019", "P3_150_150_25_1.txt"), ""),
            (os.path.join("velasco2019", "P3_150_150_50_1.txt"), ""),
            (os.path.join("velasco2019", "P3_250_250_25_1.txt"), ""),
            (os.path.join("velasco2019", "P3_250_250_50_1.txt"), ""),
            (os.path.join("velasco2019", "P4_150_150_25_1.txt"), ""),
            (os.path.join("velasco2019", "P4_150_150_50_1.txt"), ""),
            (os.path.join("velasco2019", "P4_250_250_25_1.txt"), ""),
            (os.path.join("velasco2019", "P4_250_250_50_1.txt"), "")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "single_knapsack_tree_search_maximal_spaces",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective knapsack"
                + " --predefined UO"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search-maximal-spaces 1"
                + " --not-anytime-tree-search-maximal-spaces-queue-size 16"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-strip-packing-tree-search" in args.tests:
    print("rectangleguillotine, strip packing, tree search")
    print("-----------------------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_01.2bp_40_1"), " --bin-infinite-x"),
            (os.path.join("berkey1987", "Class_02.2bp_40_1"), " --bin-infinite-x"),
            (os.path.join("berkey1987", "Class_03.2bp_40_1"), " --bin-infinite-x"),
            (os.path.join("berkey1987", "Class_04.2bp_40_1"), " --bin-infinite-x"),
            (os.path.join("berkey1987", "Class_05.2bp_40_1"), " --bin-infinite-x"),
            (os.path.join("berkey1987", "Class_06.2bp_40_1"), " --bin-infinite-x"),
            (os.path.join("martello1998", "Class_07.2bp_40_1"), " --bin-infinite-x"),
            (os.path.join("martello1998", "Class_08.2bp_40_1"), " --bin-infinite-x"),
            (os.path.join("martello1998", "Class_09.2bp_40_1"), " --bin-infinite-x"),
            (os.path.join("martello1998", "Class_10.2bp_40_1"), " --bin-infinite-x")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "strip_packing_tree_search",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective open-dimension-x"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search 1"
                + " --not-anytime-tree-search-queue-size 256"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-bin-packing-tree-search" in args.tests:
    print("rectangleguillotine, bin packing, tree search")
    print("---------------------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_01.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_02.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_03.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_04.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_05.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_06.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_07.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_08.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_09.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_10.2bp_40_1"), " --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "bin_packing_tree_search",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search 1"
                + " --not-anytime-tree-search-queue-size 256"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-bin-packing-sequential-single-knapsack" in args.tests:
    print("rectangleguillotine, bin packing, sequential single knapsack")
    print("------------------------------------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_02.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_04.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_06.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "bin_packing_sequential_single_knapsack",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-sequential-single-knapsack 1"
                + " --not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size 256"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-bin-packing-sequential-strips-onedimensional" in args.tests:
    print("rectangleguillotine, bin packing, sequential strips onedimensional")
    print("--------------------------------------------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_01.2bp_100_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_02.2bp_100_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_03.2bp_100_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_04.2bp_100_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_05.2bp_100_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_06.2bp_100_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_07.2bp_100_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_08.2bp_100_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_09.2bp_100_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_10.2bp_100_1"), " --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "bin_packing_sequential_strips_onedimensional",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing"
                + " --predefined 2NHO"
                + "  --optimization-mode not-anytime-sequential"
                + " --use-sequential-strips-onedimensional 1"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-bin-packing-sequential-value-correction" in args.tests:
    print("rectangleguillotine, bin packing, sequential value correction")
    print("-------------------------------------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_01.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_03.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_05.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_07.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_08.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_09.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_10.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "bin_packing_sequential_value_correction",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-sequential-value-correction 1"
                + " --sequential-value-correction-subproblem-tree-search-queue-size 256"
                + " --not-anytime-sequential-value-correction-number-of-iterations 4"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-bin-packing-with-leftovers-tree-search" in args.tests:
    print("rectangleguillotine, bin packing with leftovers, tree search")
    print("------------------------------------------------------------")
    print()

    data = [
            (os.path.join("roadef2018", "A3"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A4"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A5"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A6"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A7"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A8"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A9"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A10"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A11"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A12"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A13"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A14"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A15"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A16"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A17"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A18"), " --predefined roadef2018"),
            (os.path.join("roadef2018", "A19"), " --predefined roadef2018")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "bin_packing_with_leftovers_tree_search",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing-with-leftovers"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search 1"
                + " --not-anytime-tree-search-queue-size 256"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-bin-packing-column-generation" in args.tests:
    print("rectangleguillotine, bin packing, column generation")
    print("---------------------------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_01.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_03.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_05.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_07.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_08.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_09.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_10.2bp_40_1"), " --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "bin_packing_column_generation",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-column-generation 1"
                + " --column-generation-subproblem-tree-search-queue-size 256"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-variable-sized-bin-packing-column-generation" in args.tests:
    print("rectangleguillotine, variable-sized bin packing, column generation")
    print("------------------------------------------------------------------")
    print()

    data = [
            (os.path.join("pisinger2005", "MB_C1_20"), " --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C3_20"), " --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C5_20"), " --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C7_20"), " --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C8_20"), " --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C9_20"), " --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "variable_sized_bin_packing_column_generation",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective variable-sized-bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-column-generation 1"
                + " --column-generation-subproblem-tree-search-queue-size 256"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-variable-sized-bin-packing-dichotomic-search" in args.tests:
    print("rectangleguillotine, variable-sized bin packing, dichotomic search")
    print("------------------------------------------------------------------")
    print()

    data = [
            (os.path.join("ortmann2010", "Nice100i5b3"), ""),
            (os.path.join("ortmann2010", "Path100i5b3"), "")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "variable_sized_bin_packing_dichotomic_search",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective variable-sized-bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-dichotomic-search 1"
                + " --not-anytime-dichotomic-search-subproblem-tree-search-queue-size 256"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-variable-sized-bin-packing-sequential-single-knapsack" in args.tests:
    print("rectangleguillotine, variable-sized bin packing, sequential single knapsack")
    print("---------------------------------------------------------------------------")
    print()

    data = [
            (os.path.join("pisinger2005", "MB_C2_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C4_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C6_10"), " --item-multiply-copies 100 --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "variable_sized_bin_packing_sequential_single_knapsack",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective variable-sized-bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-sequential-single-knapsack 1"
                + " --not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size 256"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangleguillotine-variable-sized-bin-packing-sequential-value-correction" in args.tests:
    print("rectangleguillotine, variable-sized bin packing, sequential value correction")
    print("----------------------------------------------------------------------------")
    print()

    data = [
            (os.path.join("pisinger2005", "MB_C1_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C3_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C5_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C7_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C8_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C9_10"), " --item-multiply-copies 100 --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangleguillotine",
                "variable_sized_bin_packing_sequential_value_correction",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangleguillotine_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective variable-sized-bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-sequential-value-correction 1"
                + " --sequential-value-correction-subproblem-tree-search-queue-size 256"
                + " --not-anytime-sequential-value-correction-number-of-iterations 4"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

#############
# rectangle #
#############

rectangle_main = os.path.join(
        "install",
        "bin",
        "packingsolver_rectangle")

if args.tests is None or "rectangle-single-knapsack-tree-search" in args.tests:
    print("rectangle, single knapsack, tree search")
    print("---------------------------------------")
    print()

    data = [
            (os.path.join("fayard1998", "CU1"), ""),
            (os.path.join("fayard1998", "CU2"), ""),
            (os.path.join("fayard1998", "CW1"), ""),
            (os.path.join("fayard1998", "CW2"), "")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "single_knapsack_tree_search",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective knapsack"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search 1"
                + " --not-anytime-tree-search-queue-size 512"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-single-knapsack-tree-search-maximal-spaces" in args.tests:
    print("rectangle, single knapsack, tree search maximal spaces")
    print("-------------------------------------------------------")
    print()

    data = [
            (os.path.join("egeblad2009", "ep2-100-D-C-25.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-D-C-75.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-D-R-25.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-D-R-75.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-S-C-25.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-S-C-75.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-S-R-25.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-S-R-75.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-T-C-25.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-T-C-75.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-T-R-25.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-T-R-75.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-U-C-25.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-U-C-75.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-U-R-25.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-U-R-75.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-W-C-25.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-W-C-75.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-W-R-25.2kp"), ""),
            (os.path.join("egeblad2009", "ep2-100-W-R-75.2kp"), "")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "single_knapsack_tree_search_maximal_spaces",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective knapsack"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search-maximal-spaces 1"
                + " --not-anytime-tree-search-maximal-spaces-queue-size 16"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-strip-packing-tree-search" in args.tests:
    print("rectangle, strip packing, tree search")
    print("-------------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_01.2bp_40_1"), " --bin-infinite-y"),
            (os.path.join("berkey1987", "Class_02.2bp_40_1"), " --bin-infinite-y"),
            (os.path.join("berkey1987", "Class_03.2bp_40_1"), " --bin-infinite-y"),
            (os.path.join("berkey1987", "Class_04.2bp_40_1"), " --bin-infinite-y"),
            (os.path.join("berkey1987", "Class_05.2bp_40_1"), " --bin-infinite-y"),
            (os.path.join("berkey1987", "Class_06.2bp_40_1"), " --bin-infinite-y"),
            (os.path.join("martello1998", "Class_07.2bp_40_1"), " --bin-infinite-y"),
            (os.path.join("martello1998", "Class_08.2bp_40_1"), " --bin-infinite-y"),
            (os.path.join("martello1998", "Class_09.2bp_40_1"), " --bin-infinite-y"),
            (os.path.join("martello1998", "Class_10.2bp_40_1"), " --bin-infinite-y")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "strip_packing_tree_search",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective open-dimension-y"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search 1"
                + " --not-anytime-tree-search-queue-size 512"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-bin-packing-tree-search" in args.tests:
    print("rectangle, bin packing, tree search")
    print("-----------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_01.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_02.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_03.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_04.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_05.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_06.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_07.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_08.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_09.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_10.2bp_40_1"), " --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "bin_packing_tree_search",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search 1"
                + " --not-anytime-tree-search-queue-size 512"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-bin-packing-sequential-single-knapsack" in args.tests:
    print("rectangle, bin packing, sequential single knapsack")
    print("--------------------------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_02.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_04.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_06.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "bin_packing_sequential_single_knapsack",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-sequential-single-knapsack 1"
                + " --not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size 512"
                + " --not-anytime-sequential-single-knapsack-subproblem-tree-search-maximal-spaces-queue-size 16"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-bin-packing-sequential-value-correction" in args.tests:
    print("rectangle, bin packing, sequential value correction")
    print("---------------------------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_01.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_03.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_05.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_07.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_08.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_09.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_10.2bp_20_1"), " --item-multiply-copies 100 --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "bin_packing_sequential_value_correction",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-sequential-value-correction 1"
                + " --sequential-value-correction-subproblem-tree-search-queue-size 512"
                + " --sequential-value-correction-subproblem-tree-search-maximal-spaces-queue-size 16"
                + " --not-anytime-sequential-value-correction-number-of-iterations 4"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-bin-packing-with-leftovers-tree-search" in args.tests:
    print("rectangle, bin packing with leftovers, tree search")
    print("--------------------------------------------------")
    print()

    data = [
            (os.path.join("roadef2018", "A3"), ""),
            (os.path.join("roadef2018", "A4"), ""),
            (os.path.join("roadef2018", "A5"), ""),
            (os.path.join("roadef2018", "A6"), ""),
            (os.path.join("roadef2018", "A7"), ""),
            (os.path.join("roadef2018", "A8"), ""),
            (os.path.join("roadef2018", "A9"), ""),
            (os.path.join("roadef2018", "A10"), ""),
            (os.path.join("roadef2018", "A11"), ""),
            (os.path.join("roadef2018", "A12"), ""),
            (os.path.join("roadef2018", "A13"), ""),
            (os.path.join("roadef2018", "A14"), ""),
            (os.path.join("roadef2018", "A15"), ""),
            (os.path.join("roadef2018", "A16"), ""),
            (os.path.join("roadef2018", "A17"), ""),
            (os.path.join("roadef2018", "A18"), ""),
            (os.path.join("roadef2018", "A19"), "")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "bin_packing_with_leftovers_tree_search",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing-with-leftovers"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search 1"
                + " --not-anytime-tree-search-queue-size 512"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-bin-packing-column-generation" in args.tests:
    print("rectangle, bin packing, column generation")
    print("-----------------------------------------")
    print()

    data = [
            (os.path.join("berkey1987", "Class_01.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_03.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("berkey1987", "Class_05.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_07.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_08.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_09.2bp_40_1"), " --bin-infinite-copies"),
            (os.path.join("martello1998", "Class_10.2bp_40_1"), " --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "bin_packing_column_generation",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-column-generation 1"
                + " --column-generation-subproblem-tree-search-queue-size 512"
                + " --column-generation-subproblem-tree-search-maximal-spaces-queue-size 16"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-variable-sized-bin-packing-column-generation" in args.tests:
    print("rectangle, variable-sized bin packing, column generation")
    print("--------------------------------------------------------")
    print()

    data = [
            (os.path.join("pisinger2005", "MB_C1_20"), " --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C3_20"), " --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C5_20"), " --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C7_20"), " --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C8_20"), " --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C9_20"), " --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "variable_sized_bin_packing_column_generation",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective variable-sized-bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-column-generation 1"
                + " --column-generation-subproblem-tree-search-queue-size 512"
                + " --column-generation-subproblem-tree-search-maximal-spaces-queue-size 16"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-variable-sized-bin-packing-dichotomic-search" in args.tests:
    print("rectangle, variable-sized bin packing, dichotomic search")
    print("--------------------------------------------------------")
    print()

    data = [
            (os.path.join("ortmann2010", "Nice100i5b3"), ""),
            (os.path.join("ortmann2010", "Path100i5b3"), "")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "variable_sized_bin_packing_dichotomic_search",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective variable-sized-bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-dichotomic-search 1"
                + " --not-anytime-dichotomic-search-subproblem-tree-search-queue-size 512"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-variable-sized-bin-packing-sequential-single-knapsack" in args.tests:
    print("rectangle, variable-sized bin packing, sequential single knapsack")
    print("-----------------------------------------------------------------")
    print()

    data = [
            (os.path.join("pisinger2005", "MB_C2_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C4_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C6_10"), " --item-multiply-copies 100 --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "variable_sized_bin_packing_sequential_single_knapsack",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective variable-sized-bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-sequential-single-knapsack 1"
                + " --not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size 512"
                + " --not-anytime-sequential-single-knapsack-subproblem-tree-search-maximal-spaces-queue-size 16"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

if args.tests is None or "rectangle-variable-sized-bin-packing-sequential-value-correction" in args.tests:
    print("rectangle, variable-sized bin packing, sequential value correction")
    print("------------------------------------------------------------------")
    print()

    data = [
            (os.path.join("pisinger2005", "MB_C1_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C3_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C5_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C7_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C8_10"), " --item-multiply-copies 100 --bin-infinite-copies"),
            (os.path.join("pisinger2005", "MB_C9_10"), " --item-multiply-copies 100 --bin-infinite-copies")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "rectangle",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "rectangle",
                "variable_sized_bin_packing_sequential_value_correction",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                rectangle_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective variable-sized-bin-packing"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-sequential-value-correction 1"
                + " --sequential-value-correction-subproblem-tree-search-queue-size 512"
                + " --sequential-value-correction-subproblem-tree-search-maximal-spaces-queue-size 16"
                + " --not-anytime-sequential-value-correction-number-of-iterations 4"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

#######
# box #
#######

box_main = os.path.join(
        "install",
        "bin",
        "packingsolver_box")

if args.tests is None or "box-single-knapsack-tree-search-maximal-spaces" in args.tests:
    print("box, single knapsack, tree search maximal spaces")
    print("-------------------------------------------------")
    print()

    data = [
            (os.path.join("bischoff1995", "BR1.txt_1"), ""),
            (os.path.join("bischoff1995", "BR2.txt_1"), ""),
            (os.path.join("bischoff1995", "BR3.txt_1"), ""),
            (os.path.join("bischoff1995", "BR4.txt_1"), ""),
            (os.path.join("bischoff1995", "BR5.txt_1"), ""),
            (os.path.join("bischoff1995", "BR6.txt_1"), ""),
            (os.path.join("bischoff1995", "BR7.txt_1"), ""),
            (os.path.join("davies1999", "BR0.txt_1"), ""),
            (os.path.join("davies1999", "BR8.txt_1"), ""),
            (os.path.join("davies1999", "BR9.txt_1"), ""),
            (os.path.join("davies1999", "BR10.txt_1"), ""),
            (os.path.join("davies1999", "BR11.txt_1"), ""),
            (os.path.join("davies1999", "BR12.txt_1"), ""),
            (os.path.join("davies1999", "BR13.txt_1"), ""),
            (os.path.join("davies1999", "BR14.txt_1"), ""),
            (os.path.join("davies1999", "BR15.txt_1"), "")]
    for instance, options in data:
        instance_path = os.path.join(
                "data",
                "box",
                instance)
        json_output_path = os.path.join(
                args.directory,
                "box",
                "single_knapsack_tree_search_maximal_spaces",
                instance + ".json")
        if not os.path.exists(os.path.dirname(json_output_path)):
            os.makedirs(os.path.dirname(json_output_path))
        command = (
                box_main
                + "  --verbosity-level 1"
                + "  --items \"" + instance_path + "\""
                + options
                + " --objective knapsack"
                + "  --optimization-mode not-anytime-deterministic"
                + " --use-tree-search-maximal-spaces 1"
                + " --not-anytime-tree-search-maximal-spaces-queue-size 16"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

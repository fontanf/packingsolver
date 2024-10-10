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
                + "  --optimization-mode not-anytime"
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
                + "  --optimization-mode not-anytime"
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
                + "  --optimization-mode not-anytime"
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
                + "  --optimization-mode not-anytime"
                + " --use-sequential-single-knapsack 1"
                + " --not-anytime-sequential-single-knapsack-subproblem-queue-size 256"
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
                + "  --optimization-mode not-anytime"
                + " --use-sequential-value-correction 1"
                + " --sequential-value-correction-subproblem-queue-size 256"
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
                + "  --optimization-mode not-anytime"
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
                + "  --optimization-mode not-anytime"
                + " --use-column-generation 1"
                + " --column-generation-subproblem-queue-size 256"
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
                + "  --optimization-mode not-anytime"
                + " --use-column-generation 1"
                + " --column-generation-subproblem-queue-size 256"
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
                + "  --optimization-mode not-anytime"
                + " --use-dichotomic-search 1"
                + " --not-anytime-dichotomic-search-subproblem-queue-size 256"
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
                + "  --optimization-mode not-anytime"
                + " --use-sequential-single-knapsack 1"
                + " --not-anytime-sequential-single-knapsack-subproblem-queue-size 256"
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
                + "  --optimization-mode not-anytime"
                + " --use-sequential-value-correction 1"
                + " --sequential-value-correction-subproblem-queue-size 256"
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
                + "  --optimization-mode not-anytime"
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
                + "  --optimization-mode not-anytime"
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
                + "  --optimization-mode not-anytime"
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
                + "  --optimization-mode not-anytime"
                + " --use-sequential-single-knapsack 1"
                + " --not-anytime-sequential-single-knapsack-subproblem-queue-size 512"
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
                + "  --optimization-mode not-anytime"
                + " --use-sequential-value-correction 1"
                + " --sequential-value-correction-subproblem-queue-size 512"
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
                + "  --optimization-mode not-anytime"
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
                + "  --optimization-mode not-anytime"
                + " --use-column-generation 1"
                + " --column-generation-subproblem-queue-size 512"
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
                + "  --optimization-mode not-anytime"
                + " --use-column-generation 1"
                + " --column-generation-subproblem-queue-size 512"
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
                + "  --optimization-mode not-anytime"
                + " --use-dichotomic-search 1"
                + " --not-anytime-dichotomic-search-subproblem-queue-size 512"
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
                + "  --optimization-mode not-anytime"
                + " --use-sequential-single-knapsack 1"
                + " --not-anytime-sequential-single-knapsack-subproblem-queue-size 512"
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
                + "  --optimization-mode not-anytime"
                + " --use-sequential-value-correction 1"
                + " --sequential-value-correction-subproblem-queue-size 512"
                + " --not-anytime-sequential-value-correction-number-of-iterations 4"
                + "  --output \"" + json_output_path + "\"")
        print(command)
        status = os.system(command)
        if status != 0:
            sys.exit(1)
        print()
    print()
    print()

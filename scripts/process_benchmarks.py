import streamlit as st
import argparse
import os
import csv
import json
import datetime
import pandas as pd


st.set_page_config(layout='wide')
st.title("PackingSolver benchmarks analyzer")

def show_datafram(df):
    st.dataframe(
            df,
            use_container_width=True,
            height=(len(df.index) + 1) * 35 + 3)

benchmarks = [
    f
    for f in os.listdir("benchmark_results")
    if os.path.isdir(os.path.join("benchmark_results", f))]

benchmark = st.selectbox(
    "Benchmark",
    benchmarks)

benchmark_directory = os.path.join(
        "benchmark_results",
        benchmark)

output_directories = [
    f
    for f in os.listdir(benchmark_directory)
    if os.path.isdir(os.path.join(benchmark_directory, f))]

bksv_field = "Best known solution value"


if benchmark == "rectangleguillotine_roadef2018":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_roadef2018.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in reader.fieldnames
                          if "Solution value" in fieldname]

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": p} for p in ["a", "b", "x", "b+x", "a+b+x"]]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0

        for row in reader:

            row[bksv_field] = int(row[bksv_field])

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = (
                        json_data["Output"]["Solution"]["Waste"])

            # Get extra rows to update.
            extra_rows_to_update = []
            if row["Dataset"] == "a":
                extra_rows_to_update.append(0)
            elif row["Dataset"] == "b":
                extra_rows_to_update.append(1)
                extra_rows_to_update.append(3)
            elif row["Dataset"] == "x":
                extra_rows_to_update.append(2)
                extra_rows_to_update.append(3)
            extra_rows_to_update.append(4)

            # Update "Best known solution value" column of extra row.
            for row_id in extra_rows_to_update:
                extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                waste = int(row[result_column])
                row[result_column] = waste
                for row_id in extra_rows_to_update:
                    extra_rows[row_id][result_column] += waste

            # Add current row.
            out_rows.append(row)

        # Add extra rows.
        for row in extra_rows:
            out_rows.append(row)

        df = pd.DataFrame.from_records(out_rows, columns=out_fieldnames)

        def highlight(s):
            return [('background-color: lightgreen'
                     if s[fieldname] == s[bksv_field]
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns + output_directories
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_bin_packing_3nho":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_bin_packing_3nho.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory)

        out_rows = []

        # Initialize extra rows.
        extra_rows = [
                {
                    "Path": ("Class_" + instance_class
                              + ".2bp_" + number_of_items),
                    bksv_field: 0,
                }
                for instance_class in ["01", "02", "03", "04", "05",
                                       "06", "07", "08", "09", "10"]
                for number_of_items in ["20", "40", "60", "80", "100"]
                ] + {
                        "Path": "Total",
                        bksv_field: 0}
        for output_directory in output_directories:
            for row in extra_rows:
                row[output_directory] = 0

        for row in reader:
            if not row["Path"]:
                break

            row[bksv_field] = int(row[bksv_field])

            # Get extra rows to update.
            instance_class = int(row["Path"].split('_')[1].split('.')[0])
            number_of_items = int(row["Path"].split('_')[2])
            extra_rows_to_update = [
                    ((instance_class - 1) * 5
                     + int(number_of_items / 20 - 1)),
                    -1]

            # Update "Best known solution value" column.
            for row_id in extra_rows_to_update:
                extra_rows[row_id][bksv_field] += row[bksv_field]

            for output_directory in output_directories:
                # Retrieve solution value.
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                number_of_bins = int(json_data["Output"]
                                              ["Solution"]
                                              ["NumberOfBins"])

                # Update current row.
                row[output_directory] = number_of_bins

                # Update extra rows.
                extra_rows[row_id][output_directory] += number_of_bins
                extra_rows[-1][output_directory] += number_of_bins

            # Update result columns of extra rows.
            for result_column in result_columns:
                waste = int(row[result_column])
                row[result_column] = waste
                for row_id in extra_rows_to_update:
                    extra_rows[row_id][result_column] += waste

            # Add current row.
            out_rows.append(row)

        # Add extra rows.
        for row in extra_rows:
            out_rows.append(extra_rows[i])

        df = pd.DataFrame.from_records(out_rows, columns=out_fieldnames)

        def highlight(s):
            return [('background-color: lightgreen'
                     if s[fieldname] == int(s[bksv_field])
                     else 'background-color: pink')
                    if fieldname in output_directories
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_bin_packing_3nhr":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_bin_packing_3nhr.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        out_rows = []
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory)

        extra_rows = [
                {
                    "Path": ("Class_" + instance_class
                              + ".2bp_" + number_of_items),
                    bksv_field: 0,
                }
                for instance_class in ["01", "02", "03", "04", "05",
                                       "06", "07", "08", "09", "10"]
                for number_of_items in ["20", "40", "60", "80", "100"]
                ] + {
                        "Path": "Total",
                        bksv_field: 0}
        for output_directory in output_directories:
            for row in extra_rows:
                extra_rows[i][output_directory] = 0

        for row in reader:
            if not row["Path"]:
                break

            row[bksv_field] = int(row[bksv_field])

            instance_class = int(row["Path"].split('_')[1].split('.')[0])
            number_of_items = int(row["Path"].split('_')[2])
            row_id = (
                    (instance_class - 1) * 5
                    + int(number_of_items / 20 - 1))

            # Update "Best known solution value" column.
            extra_rows[row_id][bksv_field] += row[bksv_field]
            extra_rows[-1][bksv_field] += row[bksv_field]

            for output_directory in output_directories:
                # Retrieve solution value.
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                number_of_bins = int(json_data["Output"]
                                              ["Solution"]
                                              ["NumberOfBins"])

                # Update current row.
                row[output_directory] = number_of_bins

                # Update extra rows.
                extra_rows[row_id][output_directory] += number_of_bins
                extra_rows[-1][output_directory] += number_of_bins

            # Add current row.
            out_rows.append(row)

        # Add extra rows.
        for row in extra_rows:
            out_rows.append(extra_rows[i])

        df = pd.DataFrame.from_records(out_rows, columns=out_fieldnames)

        def highlight(s):
            return [('background-color: lightgreen'
                     if s[fieldname] == int(s[bksv_field])
                     else 'background-color: pink')
                    if fieldname in output_directories
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_long2020":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_long2020.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory)

        result_columns = [
                "long2020 / SILP / Solution value",
                "long2020 / HCG2 / Solution value"]

        out_rows = []

        for row in reader:

            row[bksv_field] = int(row[bksv_field])

            for output_directory in output_directories:
                # Retrieve solution value.
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                number_of_bins = int(json_data["Output"]
                                              ["Solution"]
                                              ["NumberOfBins"])

                # Update current row.
                row[output_directory] = number_of_bins

            # Update result columns of extra rows.
            for result_column in result_columns:
                number_of_bins = int(row[result_column])
                row[result_column] = number_of_bins

            # Add current row.
            out_rows.append(row)

        df = pd.DataFrame.from_records(out_rows, columns=out_fieldnames)

        def highlight(s):
            return [('background-color: lightgreen'
                     if s[fieldname] == s[bksv_field]
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns + output_directories
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_bin_packing_3hao":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_bin_packing_3hao.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        out_rows = []

        # Initialize extra rows.
        extra_rows = [
                {
                    "Path": dataset,
                    bksv_field: 0,
                }
                for dataset in ["cintra2008_1bintype", "imahori2005"]]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0

        for row in reader:
            if not row["Path"]:
                break

            row[bksv_field] = int(row[bksv_field])

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = (
                        json_data["Output"]["Solution"]["NumberOfBins"])

            # Get extra rows to update.
            row_id = (
                    0 if "cintra2008_1bintype" in row["Path"] else 1)

            # Update "Best known solution value" column of extra row.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                number_of_bins = int(row[result_column])
                row[result_column] = number_of_bins
                extra_rows[row_id][result_column] += number_of_bins

            # Add current row.
            out_rows.append(row)

        # Add extra rows.
        for row in extra_rows:
            out_rows.append(row)

        df = pd.DataFrame.from_records(out_rows, columns=out_fieldnames)

        def highlight(s):
            return [('background-color: lightgreen'
                     if s[fieldname] == s[bksv_field]
                     else 'background-color: pink')
                    if "Solution value" in fieldname
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangle_bin_packing_oriented":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_bin_packing_oriented.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        out_rows = []
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory)

        extra_rows = [
                {
                    "Path": ("Class_" + instance_class
                              + ".2bp_" + number_of_items),
                    bksv_field: 0,
                }
                for instance_class in ["01", "02", "03", "04", "05",
                                       "06", "07", "08", "09", "10"]
                for number_of_items in ["20", "40", "60", "80", "100"]
            ]
        for output_directory in output_directories:
            for row in extra_rows:
                row[output_directory] = 0

        extra_row = {
                "Path": "Total",
                bksv_field: 0}
        for output_directory in output_directories:
            extra_row[output_directory] = 0

        for row in reader:
            if not row["Path"]:
                break

            row[bksv_field] = int(row[bksv_field])

            instance_class = int(row["Path"].split('_')[1].split('.')[0])
            number_of_items = int(row["Path"].split('_')[2])
            row_id = (
                    (instance_class - 1) * 5
                    + int(number_of_items / 20 - 1))
            extra_row[bksv_field] += int(row[bksv_field])
            extra_rows[row_id][bksv_field] += int(row[bksv_field])

            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                number_of_bins = int(json_data["Output"]
                                              ["Solution"]
                                              ["NumberOfBins"])
                row[output_directory] = number_of_bins
                extra_rows[row_id][output_directory] += number_of_bins
                extra_row[output_directory] += number_of_bins

            out_rows.append(row)

        for row in extra_rows:
            out_rows.append(row)
        out_rows.append(extra_row)

        df = pd.DataFrame.from_records(out_rows, columns=out_fieldnames)

        def highlight(s):
            return [('background-color: lightgreen'
                     if s[fieldname] == int(s[bksv_field])
                     else 'background-color: pink')
                    if fieldname in output_directories
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangle_bin_packing_rotation":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_bin_packing_rotation.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        out_rows = []
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory)

        extra_rows = [
                {
                    "Path": ("Class_" + instance_class
                              + ".2bp_" + number_of_items),
                    bksv_field: 0,
                }
                for instance_class in ["01", "02", "03", "04", "05",
                                       "06", "07", "08", "09", "10"]
                for number_of_items in ["20", "40", "60", "80", "100"]
            ]
        for output_directory in output_directories:
            for row in extra_rows:
                row[output_directory] = 0

        extra_row = {
                "Path": "Total",
                bksv_field: 0}
        for output_directory in output_directories:
            extra_row[output_directory] = 0

        for row in reader:
            if not row["Path"]:
                break

            row[bksv_field] = int(row[bksv_field])

            instance_class = int(row["Path"].split('_')[1].split('.')[0])
            number_of_items = int(row["Path"].split('_')[2])
            row_id = (
                    (instance_class - 1) * 5
                    + int(number_of_items / 20 - 1))
            extra_row[bksv_field] += int(row[bksv_field])
            extra_rows[row_id][bksv_field] += int(row[bksv_field])

            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                number_of_bins = int(json_data["Output"]
                                              ["Solution"]
                                              ["NumberOfBins"])
                row[output_directory] = number_of_bins
                extra_rows[row_id][output_directory] += number_of_bins
                extra_row[output_directory] += number_of_bins

            out_rows.append(row)

        for row in extra_rows:
            out_rows.append(row)
        out_rows.append(extra_row)

        df = pd.DataFrame.from_records(out_rows, columns=out_fieldnames)

        def highlight(s):
            return [('background-color: lightgreen'
                     if s[fieldname] == int(s[bksv_field])
                     else 'background-color: pink')
                    if fieldname in output_directories
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)

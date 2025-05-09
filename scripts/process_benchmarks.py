import streamlit as st
import argparse
import os
import csv
import json
import datetime
import pandas as pd
import pathlib


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
benchmarks.sort()

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
output_directories.sort()

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

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": p} for p in ["a", "b", "x", "b+x", "a+b+x"]]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

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

            # Update "Best known solution value" column of extra rows.
            for row_id in extra_rows_to_update:
                extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                waste = int(row[result_column])
                row[result_column] = waste
                for row_id in extra_rows_to_update:
                    extra_rows[row_id][result_column] += waste

                # Compute gap.
                gap = (waste - row[bksv_field]) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                for row_id in extra_rows_to_update:
                    extra_rows[row_id][gap_column] += gap

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
                    if fieldname in result_columns
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
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in reader.fieldnames
                          if "Solution value" in fieldname]

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
                ] + [{
                        "Path": "Total",
                        bksv_field: 0}]
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
            instance_class = int(row["Path"].split('_')[1].split('.')[0])
            number_of_items = int(row["Path"].split('_')[2])
            extra_rows_to_update = [
                    ((instance_class - 1) * 5 + int(number_of_items / 20 - 1)),
                    -1]

            # Update "Best known solution value" column of extra rows.
            for row_id in extra_rows_to_update:
                extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                number_of_bins = int(row[result_column])
                row[result_column] = number_of_bins
                for row_id in extra_rows_to_update:
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
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
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

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in reader.fieldnames
                          if "Solution value" in fieldname]

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
                ] + [{
                        "Path": "Total",
                        bksv_field: 0}]
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
            instance_class = int(row["Path"].split('_')[1].split('.')[0])
            number_of_items = int(row["Path"].split('_')[2])
            extra_rows_to_update = [
                    ((instance_class - 1) * 5 + int(number_of_items / 20 - 1)),
                    -1]

            # Update "Best known solution value" column of extra rows.
            for row_id in extra_rows_to_update:
                extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                number_of_bins = int(row[result_column])
                row[result_column] = number_of_bins
                for row_id in extra_rows_to_update:
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
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
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
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in reader.fieldnames
                          if "Solution value" in fieldname]

        out_rows = []

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
                        json_data["Output"]["Solution"]["NumberOfBins"])

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
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_bin_packing_3hao_cintra2008":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_bin_packing_3hao_cintra2008.csv")

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
        extra_rows = [{"Path": "Total", bksv_field: 0}]
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
                        json_data["Output"]["Solution"]["NumberOfBins"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
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
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_bin_packing_3hao_imahori2005":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_bin_packing_3hao_imahori2005.csv")

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
        extra_rows = [{"Path": "Total", bksv_field: 0}]
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
                        json_data["Output"]["Solution"]["NumberOfBins"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
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
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_bin_packing_3hvo_alvarez2002":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_bin_packing_3hvo_alvarez2002.csv")

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
        extra_rows = [{"Path": "Total", bksv_field: 0}]
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
                        json_data["Output"]["Solution"]["NumberOfBins"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
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
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_bin_packing_3hvo_others":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_bin_packing_3hvo_others.csv")

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
        extra_rows = [{"Path": "Total", bksv_field: 0}]
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
            row_id = 0

            # Update "Best known solution value" column of extra rows.
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
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_variable_sized_bin_packing_2nho_cintra2008":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_variable_sized_bin_packing_2nho_cintra2008.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

        for row in reader:

            row[bksv_field] = float(row[bksv_field])

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = (
                        json_data["Output"]["Solution"]["BinCost"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                cost = float(row[result_column])
                row[result_column] = cost
                extra_rows[row_id][result_column] += cost

                # Compute gap.
                gap = (cost - row[bksv_field]) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_variable_sized_bin_packing_2nhr_cintra2008":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_variable_sized_bin_packing_2nhr_cintra2008.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

        for row in reader:

            row[bksv_field] = float(row[bksv_field])

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = (
                        json_data["Output"]["Solution"]["BinCost"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                cost = float(row[result_column])
                row[result_column] = cost
                extra_rows[row_id][result_column] += cost

                # Compute gap.
                gap = (cost - row[bksv_field]) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_knapsack_3nvo_alvarez2002":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_knapsack_3nvo_alvarez2002.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

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
                        json_data["Output"]["Solution"]["ItemProfit"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                profit = int(row[result_column])
                row[result_column] = profit
                extra_rows[row_id][result_column] += profit

                # Compute gap.
                gap = (row[bksv_field] - profit) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                           if s[fieldname] < s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_knapsack_3nvo_cui2015":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_knapsack_3nvo_cui2015.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

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
                        json_data["Output"]["Solution"]["ItemProfit"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                profit = int(row[result_column])
                row[result_column] = profit
                extra_rows[row_id][result_column] += profit

                # Compute gap.
                gap = (row[bksv_field] - profit) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                           if s[fieldname] < s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_knapsack_3hao_others":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_knapsack_3hao_others.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

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
                        json_data["Output"]["Solution"]["ItemProfit"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                if row[result_column] == "":
                    continue
                profit = int(row[result_column])
                row[result_column] = profit
                extra_rows[row_id][result_column] += profit

                # Compute gap.
                gap = (row[bksv_field] - profit) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                           if s[fieldname] < s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns and s[fieldname] != ""
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_knapsack_3hao_cui2008":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_knapsack_3hao_cui2008.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [
                {
                    "Path": dataset,
                    bksv_field: 0,
                }
                for dataset in ["weighted", "unweighted"]]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

        for row in reader:

            row[bksv_field] = int(row[bksv_field])

            if row["Options"] == "unweighted":
                s = "_unweighted"
            else:
                s = ""

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + s + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = (
                        json_data["Output"]["Solution"]["ItemProfit"])

            # Get extra rows to update.
            row_id = (1 if "unweighted" in row["Options"] else 0)

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                if row[result_column] == "":
                    continue
                profit = int(row[result_column])
                row[result_column] = profit
                extra_rows[row_id][result_column] += profit

                # Compute gap.
                gap = (row[bksv_field] - profit) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                           if s[fieldname] < s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns and s[fieldname] != ""
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_knapsack_2nho_2nvo_others":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_knapsack_2nho_2nvo_others.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

        for row in reader:

            row[bksv_field] = int(row[bksv_field])

            if row["Options"] == "Horizontal":
                s = "_horizontal"
            else:
                s = "_vertical"

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + s + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = (
                        json_data["Output"]["Solution"]["ItemProfit"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                profit = int(row[result_column])
                row[result_column] = profit
                extra_rows[row_id][result_column] += profit

                # Compute gap.
                gap = (row[bksv_field] - profit) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                           if s[fieldname] < s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_knapsack_2nho_2nvo_alvarez2002":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_knapsack_2nho_2nvo_alvarez2002.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

        for row in reader:

            row[bksv_field] = int(row[bksv_field])

            if row["Options"] == "Horizontal":
                s = "_horizontal"
            else:
                s = "_vertical"

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + s + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = (
                        json_data["Output"]["Solution"]["ItemProfit"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                profit = int(row[result_column])
                row[result_column] = profit
                extra_rows[row_id][result_column] += profit

                # Compute gap.
                gap = (row[bksv_field] - profit) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                           if s[fieldname] < s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangleguillotine_knapsack_2nho_2nvo_hifi2012":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_knapsack_2nho_2nvo_hifi2012.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

        for row in reader:

            row[bksv_field] = int(row[bksv_field])

            if row["Options"] == "Horizontal":
                s = "_horizontal"
            else:
                s = "_vertical"

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + s + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = (
                        json_data["Output"]["Solution"]["ItemProfit"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                profit = int(row[result_column])
                row[result_column] = profit
                extra_rows[row_id][result_column] += profit

                # Compute gap.
                gap = (row[bksv_field] - profit) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                           if s[fieldname] < s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
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

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in reader.fieldnames
                          if "Solution value" in fieldname]

        # Initialize extra rows.
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
                ] + [{
                        "Path": "Total",
                        bksv_field: 0}]
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
            instance_class = int(row["Path"].split('_')[1].split('.')[0])
            number_of_items = int(row["Path"].split('_')[2])
            extra_rows_to_update = [
                    ((instance_class - 1) * 5 + int(number_of_items / 20 - 1)),
                    -1]

            # Update "Best known solution value" column of extra rows.
            for row_id in extra_rows_to_update:
                extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                number_of_bins = int(row[result_column])
                row[result_column] = number_of_bins
                for row_id in extra_rows_to_update:
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
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
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

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in reader.fieldnames
                          if "Solution value" in fieldname]

        # Initialize extra rows.
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
                ] + [{
                        "Path": "Total",
                        bksv_field: 0}]
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
            instance_class = int(row["Path"].split('_')[1].split('.')[0])
            number_of_items = int(row["Path"].split('_')[2])
            extra_rows_to_update = [
                    ((instance_class - 1) * 5 + int(number_of_items / 20 - 1)),
                    -1]

            # Update "Best known solution value" column of extra rows.
            for row_id in extra_rows_to_update:
                extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                number_of_bins = int(row[result_column])
                row[result_column] = number_of_bins
                for row_id in extra_rows_to_update:
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
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "rectangle_variable_sized_bin_packing_oriented_pisinger2005":

    datacsv_path = os.path.join(
            "data",
            "rectangle",
            "data_variable_sized_bin_packing_oriented_pisinger2005.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

        for row in reader:

            row[bksv_field] = float(row[bksv_field])

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = (
                        json_data["Output"]["Solution"]["BinCost"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                cost = float(row[result_column])
                row[result_column] = cost
                extra_rows[row_id][result_column] += cost

                # Compute gap.
                gap = (cost - row[bksv_field]) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "boxstacks_knapsack_roadef2022_2024-04-25":

    datacsv_path = os.path.join(
            "data",
            "boxstacks",
            "data_knapsack_roadef2022_2024-04-25.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(
                    output_directory + " / # of incomplete solutions")
            out_fieldnames.append(
                    output_directory + " / Total time")

        result_columns = [fieldname for fieldname in reader.fieldnames
                          if "# of incomplete Solutions" in fieldname]
        time_columns = [fieldname for fieldname in reader.fieldnames
                          if "Total time" in fieldname]

        # Initialize extra rows.
        out_rows = []
        row_name_to_row = {}

        extra_rows = [{
                        "Path": "Total",
                        "# of incomplete solutions": 0,
                        "Total time": 0.0}]
        for fieldname in reader.fieldnames:
            if ("# of incomplete solutions" in fieldname or "Total time" in fieldname):
                for row in extra_rows:
                    row[fieldname] = 0

        for row in reader:
            if not row["Path"]:
                break

            path = pathlib.Path(row["Path"])
            row_name = path.parts[1] + "/" + path.parts[2]
            if row_name not in row_name_to_row:
                row_name_to_row[row_name] = len(out_rows)
                row_new = {"Path": row_name, "# instances": 0}
                for output_directory in output_directories:
                    row_new[output_directory + " / Total time"] = 0
                    row_new[output_directory
                            + " / # of incomplete solutions"] = 0
                out_rows.append(row_new)
            row_new = out_rows[row_name_to_row[row_name]]
            row_new["# instances"] += 1

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                t = json_data["Output"]["Time"]
                row_new[output_directory + " / Total time"] += t
                v = json_data["Output"]["Solution"]["NumberOfUnpackedItems"]
                if v != 0:
                    row_new[output_directory
                            + " / # of incomplete solutions"] += 1


        # Get extra rows to update.
        extra_rows_to_update = [-1]

        # Update result columns of extra rows.
        for row_new in out_rows:
            for fieldname in reader.fieldnames:
                if "# of incomplete solutions" in fieldname:
                    n = int(row_new[fieldname])
                    row_new[fieldname] = n
                    for row_id in extra_rows_to_update:
                        extra_rows[row_id][fieldname] += n
                if "Total time" in fieldname:
                    time = float(row_new[fieldname])
                    row_new[fieldname] = time
                    for row_id in extra_rows_to_update:
                        extra_rows[row_id][fieldname] += time

        # Add extra rows.
        for row in extra_rows:
            out_rows.append(row)

        df = pd.DataFrame.from_records(out_rows, columns=out_fieldnames)

        def highlight(s):
            return [('background-color: lightgreen'
                     if s[fieldname] == 0
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "onedimensional_gschwind2016":

    datacsv_path = os.path.join(
            "data",
            "onedimensional",
            "data_gschwind2016.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")
            out_fieldnames.append(output_directory + " / Time")

        result_columns = [fieldname for fieldname in reader.fieldnames
                          if "Solution value" in fieldname]

        # Initialize extra rows.
        out_rows = []

        # Initialize extra rows.
        extra_rows = [{
                        "Path": "Total",
                        bksv_field: 0}]
        for fieldname in reader.fieldnames:
            if "olution value" in fieldname or "Time" in fieldname:
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
                row[output_directory + " / Time"] = (
                        json_data["Output"]["Time"])

            # Get extra rows to update.
            extra_rows_to_update = [-1]

            # Update "Best known solution value" column of extra rows.
            for row_id in extra_rows_to_update:
                extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for fieldname in reader.fieldnames:
                if "Solution value" in fieldname:
                    number_of_bins = int(row[fieldname])
                    row[fieldname] = number_of_bins
                    for row_id in extra_rows_to_update:
                        extra_rows[row_id][fieldname] += number_of_bins
                if "Time" in fieldname:
                    time = float(row[fieldname])
                    row[fieldname] = time
                    for row_id in extra_rows_to_update:
                        extra_rows[row_id][fieldname] += time

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
                    if "olution value" in fieldname
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark in ("irregular_cgshop2024_100",
                   "irregular_cgshop2024_1000",
                   "irregular_cgshop2024_10000",
                   "irregular_cgshop2024_100000"):

    datacsv_path = os.path.join(
            "data",
            "irregular",
            "data_cgshop2024.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in out_fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

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
                        json_data["Output"]["Solution"]["ItemProfit"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                profit = int(row[result_column])
                row[result_column] = profit
                extra_rows[row_id][result_column] += profit

                # Compute gap.
                gap = (row[bksv_field] - profit) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                           if s[fieldname] < s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


if benchmark == "irregular_opencutlist":

    datacsv_path = os.path.join(
            "data",
            "irregular",
            "data_opencutlist.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in reader.fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: (0, 0)}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = (0, 0)

        for row in reader:

            # row[bksv_field] = row[bksv_field]
            row[bksv_field] = (0, 0)

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = ((
                    json_data["Output"]["Solution"]["NumberOfBins"],
                    json_data["Output"]["Solution"]["LeftoverValue"]))

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] = tuple(int(p) + float(q) for p, q in zip(extra_rows[row_id][bksv_field], row[bksv_field]))

            # Update result columns of extra rows.
            for result_column in result_columns:
                extra_rows[row_id][result_column] = tuple(int(p) + float(q) for p, q in zip(extra_rows[row_id][result_column], row[result_column]))

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
                           if s[fieldname] < s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


if benchmark == "irregular_literature":

    datacsv_path = os.path.join(
            "data",
            "irregular",
            "data_literature.csv")

    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)

        # Get fieldnames of CSV output file.
        out_fieldnames = reader.fieldnames
        for output_directory in output_directories:
            out_fieldnames.append(output_directory + " / Solution value")

        result_columns = [fieldname for fieldname in reader.fieldnames
                          if "Solution value" in fieldname]

        # Add gap columns.
        out_fieldnames_tmp = []
        for fieldname in out_fieldnames:
            out_fieldnames_tmp.append(fieldname)
            if "Solution value" in fieldname:
                out_fieldnames_tmp.append(
                        fieldname.replace("Solution value", "Gap"))
        out_fieldnames = out_fieldnames_tmp

        out_rows = []

        # Initialize extra rows.
        extra_rows = [{"Path": "Total", bksv_field: 0}]
        for fieldname in [bksv_field] + result_columns:
            for row in extra_rows:
                row[fieldname] = 0
                row[fieldname.replace("Solution value", "Gap")] = 0

        for row in reader:

            row[bksv_field] = float(row[bksv_field])

            # Fill current row.
            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Path"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                row[output_directory + " / Solution value"] = (
                        json_data["Output"]["Solution"]["DensityX"])

            # Get extra rows to update.
            row_id = 0

            # Update "Best known solution value" column of extra rows.
            extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                density_x = float(row[result_column]) * 100
                row[result_column] = density_x
                extra_rows[row_id][result_column] += density_x

                # Compute gap.
                gap = (row[bksv_field] - density_x) / row[bksv_field] * 100
                gap_column = result_column.replace("Solution value", "Gap")
                row[gap_column] = gap
                extra_rows[row_id][gap_column] += gap

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
                           if s[fieldname] < s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)


elif benchmark == "irregular_user2025":

    datacsv_path = os.path.join(
            "data",
            "irregular",
            "data_user2025.csv")

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
        extra_rows = [
                {
                    "Path": (name),
                    bksv_field: 0,
                }
                for name in ["hand", "real"]
                ] + [{
                        "Path": "Total",
                        bksv_field: 0}]
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
            if "hand" in row["Path"]:
                extra_rows_to_update = [0, -1]
            elif "real" in row["Path"]:
                extra_rows_to_update = [1, -1]

            # Update "Best known solution value" column of extra rows.
            for row_id in extra_rows_to_update:
                extra_rows[row_id][bksv_field] += row[bksv_field]

            # Update result columns of extra rows.
            for result_column in result_columns:
                number_of_bins = int(row[result_column])
                row[result_column] = number_of_bins
                for row_id in extra_rows_to_update:
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
                     else ('background-color: pink'
                           if s[fieldname] > s[bksv_field]
                           else 'background-color: yellow'))
                    if fieldname in result_columns
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        show_datafram(df)

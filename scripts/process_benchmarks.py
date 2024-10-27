import streamlit as st
import argparse
import os
import csv
import json
import datetime
import pandas as pd


st.title("PackingSolver benchmarks analyzer")

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


if benchmark == "rectangle_bin_packing_oriented":

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

        last_rows = [
                {
                    "Items": ("Class_" + instance_class
                              + ".2bp_" + number_of_items),
                    bksv_field: 0,
                }
                for instance_class in ["01", "02", "03", "04", "05",
                                       "06", "07", "08", "09", "10"]
                for number_of_items in ["20", "40", "60", "80", "100"]
            ]
        for output_directory in output_directories:
            for i in range(50):
                last_rows[i][output_directory] = 0

        last_row = {
                "Items": "Total",
                bksv_field: 0}
        for output_directory in output_directories:
            last_row[output_directory] = 0

        for row in reader:
            if not row["Items"]:
                break

            instance_class = int(row["Items"].split('_')[1].split('.')[0])
            number_of_items = int(row["Items"].split('_')[2])
            row[bksv_field] = int(row[bksv_field])
            row_id = (
                    (instance_class - 1) * 5
                    + int(number_of_items / 20 - 1))
            last_row[bksv_field] += int(row[bksv_field])
            last_rows[row_id][bksv_field] += int(row[bksv_field])

            for output_directory in output_directories:
                json_output_path = os.path.join(
                        benchmark_directory,
                        output_directory,
                        row["Items"] + "_output.json")
                json_output_file = open(json_output_path, "r")
                json_data = json.load(json_output_file)
                number_of_bins = int(json_data["Output"]
                                              ["Solution"]
                                              ["NumberOfBins"])
                row[output_directory] = number_of_bins
                last_rows[row_id][output_directory] += number_of_bins
                last_row[output_directory] += number_of_bins

            out_rows.append(row)

        for i in range(50):
            out_rows.append(last_rows[i])
        out_rows.append(last_row)

        df = pd.DataFrame.from_records(out_rows, columns=out_fieldnames)

        def highlight(s):
            return [('background-color: green'
                     if s[fieldname] == int(s[bksv_field])
                     else 'background-color: red')
                    if fieldname in output_directories
                    else ''
                    for fieldname in out_fieldnames]
        df = df.style.apply(highlight, axis = 1)
        st.dataframe(df)


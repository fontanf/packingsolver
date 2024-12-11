import os
import os.path
import csv
import re


def words(filename):
    f = open(os.path.join("data", "onedimensional_raw", filename), "r")
    for line in f:
        for word in line.split():
            yield word


def write_dict(dic, filename):
    p = os.path.join("data", "onedimensional", filename.replace(" ", "_"))
    d = os.path.dirname(p)
    if not os.path.exists(d):
        os.makedirs(d)
    print("Create " + p)
    f = open(p, "w")
    for i in range(-1, len(dic[next(iter(dic))])):
        if i == -1:
            f.write("ID")
        else:
            f.write(str(i))
        for k in dic.keys():
            if i == -1:
                f.write("," + k)
            else:
                f.write("," + str(dic[k][i]))
        f.write("\n")


def convert_bpplib(filename):
    w = words(filename)

    bins = {"X": []}
    number_of_item_types = int(next(w))
    bins["X"].append(int(next(w)))
    write_dict(bins, filename + "_bins.csv")

    items = {"X": [], "COPIES": []}
    for i in range(0, number_of_item_types):
        items["X"].append(int(next(w)))
        items["COPIES"].append(int(next(w)))
    write_dict(items, filename + "_items.csv")


###############################################################################


if __name__ == "__main__":

    for f in ["gschwind2016/cs" + a + b + n + "_" + str(i) + ".txt"
              for a in ['A', 'B']
              for b in ['A', 'B']
              for n in ['125', '250', '500']
              for i in range(1, 21)]:
        convert_bpplib(f)

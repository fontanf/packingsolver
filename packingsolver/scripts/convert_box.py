import os
import os.path
import csv
import re

def write_dict(dic, filename):
    p = os.path.join("data", "box", filename.replace(" ", "_"))
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

def convert_egeblad2009(filename):
    bins = {"X": [], "Y": [], "Z": []}
    items = {"X": [], "Y": [], "Z": [], "PROFIT": [], "COPIES": []}

    f = open("data/box_raw/" + filename, "r")
    line = f.readline().split(",")
    bins["X"].append(int(line[1]))
    bins["Y"].append(int(line[2]))
    bins["Z"].append(int(line[3]))
    while True:
        l = f.readline()
        if not l:
            break
        line = l.split(",")
        items["X"].append(int(line[2]))
        items["Y"].append(int(line[3]))
        items["Z"].append(int(line[4]))
        items["PROFIT"].append(int(line[5]))
        items["COPIES"].append(int(line[6]))

    write_dict(bins, filename + "_bins.csv")
    write_dict(items, filename + "_items.csv")


if __name__ == "__main__":

    for f in [
            "egeblad2009/ep3d-" + i + "-" + a + "-" + b + "-" + j + ".3kp"
            for i in ["20", "40", "60"]
            for a in ["F", "L", "C", "U", "D"]
            for b in ["C", "R"]
            for j in ["50", "90"]]:
        convert_egeblad2009(f)

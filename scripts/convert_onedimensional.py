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

    for f in ["falkenauer1996_t/Falkenauer_t" + n + "_" + str(i).zfill(2) + ".txt"
              for n in ['60', '120', '249', '501']
              for i in range(20)]:
        convert_bpplib(f)

    for f in ["falkenauer1996_u/Falkenauer_u" + n + "_" + str(i).zfill(2) + ".txt"
              for n in ['120', '250', '500', '1000']
              for i in range(20)]:
        convert_bpplib(f)

    for f in ["scholl1997_1/N" + n + "C" + c + "W" + w + "_" + letter + ".txt"
              for n in ['1', '2', '3', '4']
              for c in ['1', '2', '3']
              for w in ['1', '2', '4']
              for letter in 'ABCDEFGHIJKLMNOPQRST']:
        convert_bpplib(f)

    for f in ["scholl1997_2/N" + n + "W" + w + "B" + b + "R" + str(r) + ".txt"
              for n in ['1', '2', '3', '4']
              for w in ['1', '2', '3', '4']
              for b in ['1', '2', '3']
              for r in range(10)]:
        convert_bpplib(f)

    for f in ["scholl1997_3/HARD" + str(i) + ".txt"
              for i in range(10)]:
        convert_bpplib(f)

    for id_ in ["0005", "0014", "0022", "0030", "0044", "0049", "0054",
                "0055A", "0055B", "0058", "0065", "0068", "0075", "0082",
                "0084", "0095", "0097"]:
        convert_bpplib(f"wascher1996/Waescher_TEST{id_}.txt")

    for i in range(1, 101):
        convert_bpplib(f"schwerin1997_1/Schwerin1_BPP{i}.txt")

    for i in range(1, 101):
        convert_bpplib(f"schwerin1997_2/Schwerin2_BPP{i}.txt")

    for id_ in [13, 14, 40, 47, 60, 119, 144, 175, 178, 181, 195, 359, 360,
                419, 485, 531, 561, 640, 645, 709, 716, 742, 766, 781, 785, 814, 832, 900]:
        convert_bpplib(f"schoenfield2002/Hard28_BPP{id_}.txt")

    for f in ["delorme2016_random/BPP_" + n + "_" + C + "_" + a + "_" + b + "_" + str(i) + ".txt"
              for n in ['50', '100', '200', '300', '400', '500', '750', '1000']
              for C in ['50', '75', '100', '120', '125', '150', '200', '300', '400', '500', '750', '1000']
              for a in ['0.1', '0.2']
              for b in ['0.7', '0.8']
              for i in range(10)]:
        convert_bpplib(f)

    for prefix in ["201_2500", "402_10000", "600_20000", "801_40000", "1002_80000"]:
        for i in range(50):
            convert_bpplib(f"delorme2016_ai/{prefix}_DI_{i}.txt")

    for prefix in ["201_2500", "402_10000", "600_20000", "801_40000", "1002_80000"]:
        for i in range(50):
            convert_bpplib(f"delorme2016_ani/{prefix}_NR_{i}.txt")

    for f in ["gschwind2016/cs" + a + b + n + "_" + str(i) + ".txt"
              for a in ['A', 'B']
              for b in ['A', 'B']
              for n in ['125', '250', '500']
              for i in range(1, 21)]:
        convert_bpplib(f)

import os
import os.path
import csv
import re


def words(filename):
    f = open(os.path.join("data", "rectangle_raw", filename), "r")
    for line in f:
        for word in line.split():
            yield word


def write_dict(dic, filename):
    p = os.path.join("data", "rectangle", filename.replace(" ", "_"))
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


###############################################################################


def convert_generic(filename, s1="nwh", s2="whpc"):
    w = words(filename)

    bins = {}
    for c in s1:
        if c == 'w':
            bins["WIDTH"] = []
        elif c == 'h':
            bins["HEIGHT"] = []
    for c in s1:
        if c == 'n':
            number_of_item_types = int(next(w))
        elif c == 'w':
            bins["WIDTH"].append(int(next(w)))
        elif c == 'h':
            bins["HEIGHT"].append(int(next(w)))
        elif c == 'x':
            next(w)
    write_dict(bins, filename + "_bins.csv")

    items = {}
    for c in s2:
        if c == 'w':
            items["WIDTH"] = []
        elif c == 'h':
            items["HEIGHT"] = []
        elif c == 'p':
            items["PROFIT"] = []
        elif c == 'c':
            items["COPIES"] = []
    for i in range(0, number_of_item_types):
        for c in s2:
            if c == 'w':
                items["WIDTH"].append(int(next(w)))
            elif c == 'h':
                items["HEIGHT"].append(int(next(w)))
            elif c == 'p':
                items["PROFIT"].append(int(next(w)))
            elif c == 'c':
                items["COPIES"].append(int(next(w)))
            elif c == 'x':
                next(w)
    write_dict(items, filename + "_items.csv")


def convert_vbpp(filename, s1="mn", s2="whpc", s3="", s4="whpc"):
    w = words(filename)

    for c in s1:
        if c == 'n':
            number_of_item_types = int(next(w))
        elif c == 'm':
            number_of_bin_types = int(next(w))
        elif c == 'x':
            next(w)

    bins = {}
    for c in s2:
        if c == 'w':
            bins["WIDTH"] = []
        elif c == 'h':
            bins["HEIGHT"] = []
        elif c == 'p':
            bins["COST"] = []
        elif c == 'c':
            bins["COPIES"] = []
    for i in range(0, number_of_bin_types):
        for c in s2:
            if c == 'w':
                bins["WIDTH"].append(int(next(w)))
            elif c == 'h':
                bins["HEIGHT"].append(int(next(w)))
            elif c == 'p':
                bins["COST"].append(int(next(w)))
            elif c == 'c':
                bins["COPIES"].append(int(next(w)))
            elif c == 'x':
                next(w)
    write_dict(bins, filename + "_bins.csv")

    for c in s3:
        if c == 'n':
            number_of_item_types = int(next(w))
        elif c == 'm':
            number_of_bin_types = int(next(w))
        elif c == 'x':
            next(w)

    items = {}
    for c in s4:
        if c == 'w':
            items["WIDTH"] = []
        elif c == 'h':
            items["HEIGHT"] = []
        elif c == 'p':
            items["PROFIT"] = []
        elif c == 'c':
            items["COPIES"] = []
    for i in range(0, number_of_item_types):
        for c in s4:
            if c == 'w':
                items["WIDTH"].append(int(next(w)))
            elif c == 'h':
                items["HEIGHT"].append(int(next(w)))
            elif c == 'p':
                items["PROFIT"].append(int(next(w)))
            elif c == 'c':
                items["COPIES"].append(int(next(w)))
            elif c == 'x':
                next(w)
    write_dict(items, filename + "_items.csv")


def convert_berkey1987(filename):
    w = words(filename)
    for instance_number in range(0, 50):
        bins = {"WIDTH": [], "HEIGHT": []}
        items = {"WIDTH": [], "HEIGHT": []}
        for _ in range(3):
            next(w)
        number_of_item_types = int(next(w))
        for _ in range(3):
            next(w)
        instance_relative_number = int(next(w))
        for _ in range(7):
            next(w)
        bins["HEIGHT"].append(int(next(w)))
        bins["WIDTH"].append(int(next(w)))
        next(w)
        for i in range(0, number_of_item_types):
            items["HEIGHT"].append(int(next(w)))
            items["WIDTH"].append(int(next(w)))
            if i == 0:
                next(w)
        suffix = (
                "_" + str(number_of_item_types)
                + "_" + str(instance_relative_number))
        write_dict(bins, filename + suffix + "_bins.csv")
        write_dict(items, filename + suffix + "_items.csv")


def convert_beasley2004(filename):
    w = words(filename)
    instance_number = int(next(w))
    for instance in range(0, instance_number):
        bins = {"WIDTH": [], "HEIGHT": []}
        items = {"WIDTH": [], "HEIGHT": [], "PROFIT": [], "COPIES": []}
        number_of_item_types = int(next(w))
        bins["WIDTH"].append(int(next(w)))
        bins["HEIGHT"].append(int(next(w)))
        for i in range(0, number_of_item_types):
            items["WIDTH"].append(int(next(w)))
            items["HEIGHT"].append(int(next(w)))
            next(w)
            items["COPIES"].append(int(next(w)))
            items["PROFIT"].append(int(next(w)))
        suffix = "_" + str(instance + 1)
        write_dict(bins, filename + suffix + "_bins.csv")
        write_dict(items, filename + suffix + "_items.csv")


def convert_cintra2008(filename):
    w = words(filename)

    bins = {"WIDTH": [], "HEIGHT": []}
    items = {"WIDTH": [], "HEIGHT": [], "COPIES": []}

    next(w)
    platetype_number = int(next(w))
    number_of_item_types = int(next(w))

    for _ in range(3):
        next(w)

    bins["WIDTH"].append(int(next(w)))
    bins["HEIGHT"].append(int(next(w)))
    next(w)

    for _ in range(1, platetype_number):
        for _ in range(3):
            next(w)

    for i in range(0, number_of_item_types):
        items["WIDTH"].append(int(next(w)))
        items["HEIGHT"].append(int(next(w)))
        items["COPIES"].append(int(next(w)))
        next(w)

    write_dict(bins, filename + "_1bintype_bins.csv")
    write_dict(items, filename + "_1bintype_items.csv")


def convert_egeblad2009(filename):
    bins = {"WIDTH": [], "HEIGHT": []}
    items = {"WIDTH": [], "HEIGHT": [], "PROFIT": [], "COPIES": []}

    f = open("data/rectangle_raw/" + filename, "r")
    line = f.readline().split(",")
    bins["WIDTH"].append(int(line[1]))
    bins["HEIGHT"].append(int(line[2]))
    while True:
        l = f.readline()
        if not l:
            break
        line = l.split(",")
        items["WIDTH"].append(int(line[2]))
        items["HEIGHT"].append(int(line[3]))
        items["PROFIT"].append(int(line[4]))
        items["COPIES"].append(int(line[5]))

    write_dict(bins, filename + "_bins.csv")
    write_dict(items, filename + "_items.csv")


def convert_silveira2013(filename):
    w = words(filename)

    bins = {"WIDTH": [], "HEIGHT": []}
    items = {"WIDTH": [], "HEIGHT": [], "GROUP": []}

    next(w)
    number_of_groups = int(next(w))
    number_of_item_types = int(next(w))
    bins["HEIGHT"].append(int(next(w)))
    bins["WIDTH"].append(int(next(w)))
    for group in range(number_of_groups):
        next(w)
        group_size = int(next(w))
        for i in range(group_size):
            items["HEIGHT"].append(int(next(w)))
            items["WIDTH"].append(int(next(w)))
            items["GROUP"].append(group)

    write_dict(bins, filename + "_bins.csv")
    write_dict(items, filename + "_items.csv")


def convert_afsharian2014(filename):
    f = open(os.path.join("data", "rectangle_raw", filename), "r")
    instances = {}
    instance = None
    while True:
        line = f.readline()
        if not line:
            break
        line_split = line.split()
        if "static SmallObject[]" in line:
            instance = line_split[2].split('_')[0]
            for d in range(5):
                instances[instance + "_D" + str(d)] = {
                        "bins": {"WIDTH": [], "HEIGHT": []},
                        "items": {"WIDTH": [], "HEIGHT": [], "PROFIT": []},
                        "defects": {"BIN": [], "X": [], "Y": [], "WIDTH": [], "HEIGHT": []},
                }
            continue
        if "new Data(" in line:
            instance = line.split('"')[1]
            continue
        if "new SmallObject(" in line:
            numbers = re.findall(r'\d+', line)
            for d in range(5):
                instances[instance + "_D" + str(d)]["items"]["WIDTH"].append(numbers[0])
                instances[instance + "_D" + str(d)]["items"]["HEIGHT"].append(numbers[1])
                instances[instance + "_D" + str(d)]["items"]["PROFIT"].append(numbers[2])
            continue
        if "new Defect(" in line:
            numbers = re.findall(r'\d+', line)
            instances[instance]["defects"]["BIN"].append(0)
            instances[instance]["defects"]["X"].append(numbers[0])
            instances[instance]["defects"]["Y"].append(numbers[1])
            instances[instance]["defects"]["WIDTH"].append(numbers[2])
            instances[instance]["defects"]["HEIGHT"].append(numbers[3])
            continue
        if "}, " in line and "_D1" in instance:
            numbers = re.findall(r'\d+', line)
            instance = instance.split('_')[0]
            for d in range(5):
                instances[instance + "_D" + str(d)]["bins"]["WIDTH"].append(numbers[0])
                instances[instance + "_D" + str(d)]["bins"]["HEIGHT"].append(numbers[1])
            continue

    for k, v in instances.items():
        for k2, v2 in v.items():
            write_dict(v2, filename + "/" + k + "_" + k2 + ".csv")


def convert_roadef2018(filename):
    bins = {"WIDTH": [], "HEIGHT": []}
    for _ in range(100):
        bins["WIDTH"].append(6000)
        bins["HEIGHT"].append(3210)
    write_dict(bins, filename + "_bins.csv")

    with open(os.path.join("data", "rectangle_raw", filename + "_batch.csv"), newline='') as csvfile:
        items = {"WIDTH": [], "HEIGHT": [], "STACK_ID": []}
        spamreader = csv.reader(csvfile, delimiter=';', quotechar='|')
        first_line = True
        for row in spamreader:
            if first_line:
                first_line = False
                continue
            items["WIDTH"].append(int(row[1]))
            items["HEIGHT"].append(int(row[2]))
            items["STACK_ID"].append(int(row[3]))
        write_dict(items, filename + "_items.csv")

    with open(os.path.join("data", "rectangle_raw", filename + "_defects.csv"), newline='') as csvfile:
        defects = {"BIN": [], "X": [], "Y": [], "WIDTH": [], "HEIGHT": []}
        spamreader = csv.reader(csvfile, delimiter=';', quotechar='|')
        first_line = True
        for row in spamreader:
            if first_line:
                first_line = False
                continue
            defects["BIN"].append(int(row[1]))
            defects["X"].append(int(float(row[2])))
            defects["Y"].append(int(float(row[3])))
            defects["WIDTH"].append(int(float(row[4])))
            defects["HEIGHT"].append(int(float(row[5])))
        write_dict(defects, filename + "_defects.csv")


def convert_martin2019b(filename):
    # Note: the files contain values for number of copies, but they are not
    # used in the corresponding paper.
    w = words(filename)
    items = []

    bins = {"WIDTH": [], "HEIGHT": []}
    bins["WIDTH"].append(int(next(w)))
    bins["HEIGHT"].append(int(next(w)))
    write_dict(bins, filename + "_bins.csv")
    number_of_item_types = int(next(w))

    items = {"WIDTH": [], "HEIGHT": [], "COPIES": []}
    for i in range(0, number_of_item_types):
        items["WIDTH"].append(int(next(w)))
        items["HEIGHT"].append(int(next(w)))
        items["PROFIT"].append(int(next(w)))
        int(next(w))
    write_dict(items, filename + "_items.csv")

    number_of_defects = int(next(w))
    defects = {"BIN": [], "X": [], "Y": [], "WIDTH": [], "HEIGHT": []}
    for i in range(0, number_of_defects):
        x1 = int(next(w))
        y1 = int(next(w))
        x2 = int(next(w))
        y2 = int(next(w))
        defects["BIN"].append(0)
        defects["X"].append(x1)
        defects["Y"].append(y1)
        defects["WIDTH"].append(x2 - x1)
        defects["HEIGHT"].append(y2 - y1)
    write_dict(defects, filename + "_defects.csv")


def convert_long2020(filename):
    w = words(filename)

    bins = {"WIDTH": [], "HEIGHT": []}
    items = {"WIDTH": [], "HEIGHT": [], "COPIES": []}

    for _ in range(48):
        next(w)

    first = True
    while next(w, None):
        items["COPIES"].append(int(next(w)))
        items["HEIGHT"].append(int(next(w)))
        items["WIDTH"].append(int(next(w)))
        if first:
            max1cut = int(next(w))
            bins["HEIGHT"].append(int(next(w)))
            bins["WIDTH"].append(int(next(w)))
        else:
            for _ in range(3):
                next(w)
        first = False

    write_dict(bins, filename + "_bins.csv")
    write_dict(items, filename + "_items.csv")
    p = os.path.join("data", "rectangle", filename.replace(" ", "_"))
    with open(p + "_parameters.csv", "w") as params_file:
        params_file.write("NAME,VALUE\n")
        params_file.write("max1cut," + str(max1cut) + "\n")


###############################################################################


if __name__ == "__main__":

    convert_generic("herz1972/H", "whn", "wh")

    for f in ["christofides1977/cgcut" + str(i) + ".txt" for i in range(1, 4)]:
        convert_generic(f, "nwh", "whcp")

    for f in ["beng1982/BENG" + str(i) for i in range(1, 11)]:
        convert_generic(f, "nwh", "xwh")

    for f in ["wang1983/" + i for i in ["WANG1", "WANG2", "WANG3"]]:
        convert_generic(f, "xnwh", "whc")
    convert_generic("wang1983/W", "whn", "whc")
    for f in ["wang1983/" + i for i in ["WANGM1", "WANGM2"]]:
        convert_vbpp(f, "mn", "whc", "", "whc")

    for f in ["beasley1985/gcut" + str(i) + ".txt" for i in range(1, 14)]:
        convert_generic(f, "nwh", "whp")

    for f in [
            "berkey1987/Class_" + "{:02d}".format(c) + ".2bp"
            for c in range(1, 7)]:
        convert_berkey1987(f)

    for f in ["oliveira1990/" + i for i in ["OF1", "OF2"]]:
        convert_generic(f, "whn", "whc")

    for f in ["morabito1992/M" + str(i) for i in range(1, 6)]:
        convert_generic(f, "whn", "wh")

    for f in ["tschoke1995/" + i for i in ["STS2", "STS4"]]:
        convert_generic(f, "whn", "whpc")
    for f in ["tschoke1995/" + i for i in ["STS2s", "STS4s"]]:
        convert_generic(f, "whn", "whc")

    for f in ["hadjiconstantinou1995/" + i for i in ["HADCHR3", "HADCHR11"]]:
        convert_generic(f, "whn", "whcp")

    for f in [
            "kroger1995/KR-" + "{:02d}".format(i) + ".txt"
            for i in range(1, 13)]:
        convert_generic(f, "whn", "wh")

    for f in [
            "jakobs1996/" + i
            for i in ["j1", "j2",
                      "JAKOBS1", "JAKOBS2", "JAKOBS3", "JAKOBS4", "JAKOBS5"]]:
        convert_generic(f, "nwh", "wh")

    convert_generic("fayard1996/HZ1", "whn", "wh")

    for f in ["fekete1997/okp" + str(i) for i in range(1, 6)]:
        convert_generic(f, "whn", "whpc")

    for f in ["lai1997/" + i for i in ["1", "2", "3"]]:
        convert_generic(f, "nwh", "wh")

    for f in ["hifi1997a/" + i for i in ["2", "3"]]:
        convert_generic(f, "whnx", "whpc")
    for f in ["hifi1997a/" + i for i in ["HH", "A1", "A2"]]:
        convert_generic(f, "whn", "whpc")
    for f in ["hifi1997a/" + i for i in ["2s", "3s"]]:
        convert_generic(f, "whnx", "whc")
    for f in ["hifi1997a/" + i for i in ["A1s", "A2s", "A3", "A4", "A5"]]:
        convert_generic(f, "whn", "whc")

    for f in ["hifi1997b/" + i for i in ["W1", "W2", "W3"]]:
        convert_generic(f, "whn", "whp")
    for f in ["hifi1997b/" + i for i in ["U1", "U2", "U3"]]:
        convert_generic(f, "whn", "wh")

    for f in ["hifi1998/SCP" + str(i) for i in range(1, 26)]:
        convert_generic(f, "whn", "whc")

    for f in ["fayard1998/CW" + str(i) for i in range(1, 12)]:
        convert_generic(f, "whn", "whpc")
    for f in ["fayard1998/CU" + str(i) for i in range(1, 12)]:
        convert_generic(f, "whn", "whc")
    for f in ["fayard1998/UW" + str(i) for i in range(1, 12)]:
        convert_generic(f, "whn", "whp")
    for f in ["fayard1998/UU" + str(i) for i in range(1, 12)]:
        convert_generic(f, "whn", "wh")

    for f in [
            "martello1998/Class_" + "{:02d}".format(c) + ".2bp"
            for c in range(7, 11)]:
        convert_berkey1987(f)

    for f in [
            "hopper2000/" + a + str(b) + c
            for a in ["n", "t"]
            for b in range(1, 8)
            for c in ["a", "b", "c", "d", "e"]]:
        convert_generic(f, "nwh", "wh")

    for f in ["cung2000/" + i for i in ["CHL2", "CHL3", "CHL4"]]:
        convert_generic(f, "whn", "whpc")
    for f in ["cung2000/" + i for i in ["CHL1", "Hchl1", "Hchl2", "Hchl9"]]:
        convert_generic(f, "whnx", "whpc")
    for f in [
            "cung2000/" + i
            for i in ["CHL1s", "CHL2s", "CHL3s", "CHL4s",
                      "CHL5", "CHL6", "CHL7"]]:
        convert_generic(f, "whn", "whc")
    for f in [
            "cung2000/" + i
            for i in ["Hchl3s", "Hchl4s", "Hchl5s",
                      "Hchl6s", "Hchl7s", "Hchl8s"]]:
        convert_generic(f, "whnx", "whc")

    convert_generic("hifi2001/U4", "whn", "wh")
    convert_generic("hifi2001/W4", "whn", "whp")
    for f in ["hifi2001/" + i for i in ["MW1", "MW2", "MW3", "MW4", "MW5",
                                        "LW1", "LW2", "LW3", "LW4"]]:
        convert_generic(f, "whnx", "whp")
    for f in ["hifi2001/" + i for i in ["LU1", "LU2", "LU3", "LU4"]]:
        convert_generic(f, "whnx", "wh")

    for f in [
            "hopper2001a/C" + str(i) + "_" + str(j)
            for i in range(1, 8) for j in range(1, 4)]:
        convert_generic(f, "nwh", "wh")
    for f in [
            "hopper2001b/M" + str(i) + a
            for i in range(1, 4) for a in ['a', 'b', 'c', 'd', 'e']]:
        convert_vbpp(f, "m", "whc", "n", "wh")

    for f in ["alvarez2002/ATP1" + str(i) for i in range(0, 10)]:
        convert_generic(f, "whn", "wh")
    for f in ["alvarez2002/ATP2" + str(i) for i in range(0, 10)]:
        convert_generic(f, "whn", "whp")
    for f in ["alvarez2002/ATP3" + str(i) for i in range(0, 10)]:
        convert_generic(f, "whn", "whc")
    for f in ["alvarez2002/ATP4" + str(i) for i in range(0, 10)]:
        convert_generic(f, "whn", "whpc")

    for f in ["leung2003/" + i for i in ["P9", "P10"]]:
        convert_generic(f, "nwh", "wh")

    convert_beasley2004("beasley2004/ngcutap.txt")
    for f in ["beasley2004/ngcutfs" + str(i) + ".txt" for i in range(1, 4)]:
        convert_beasley2004(f)

    for f in ["burke2004/BKW" + str(i) for i in range(1, 14)]:
        convert_generic(f, "nwh", "xwh")

    for f in [
            "imahori2005/" + a + b + c
            for a in ["A", "B", "C", "D"]
            for b in ["L", "S", "V"]
            for c in ["X", "Y", "Z", "ZZ", "ZZZ"]]:
        convert_generic(f, "whn", "whc")

    for f in [
            "pinto2005/" + str(i)
            for i in [50, 100, 500, 1000, 5000, 10000, 15000]]:
        convert_generic(f, "whn", "wh")

    for f in [
            "pisinger2005/MB_C" + str(i) + "_" + str(j)
            for i in range(1, 11)
            for j in range(1, 51)]:
        convert_vbpp(f, "nmx", "whxp", "", "xwhx")

    for f in ["bortfeldt2006/AH" + str(i) for i in range(1, 361)]:
        convert_generic(f, "whn", "wh")

    for f in ["cui2008/" + str(i) for i in range(1, 21)]:
        convert_generic(f, "nwh", "whcxp")

    for f in ["cintra2008/gcut" + str(i) + "d.txt" for i in range(1, 13)]:
        convert_cintra2008(f)
        convert_vbpp(f, "xmnxxx", "whp", "", "whcx")

    for f in [
            "egeblad2009/ep2-" + i + "-" + a + "-" + b + "-" + j + ".2kp"
            for i in ["30", "50", "100", "200"]
            for a in ["D", "S", "T", "U", "W"]
            for b in ["C", "R"]
            for j in ["25", "75"]]:
        convert_egeblad2009(f)

    # for f in [
    #         "imahori2010/i" + str(i) + "-" + str(j)
    #         for i in range(4, 21)
    #         for j in range(1, 11)]:
    #     convert_generic(f, "nw", "xwh")

    for f in [
            "morabito2010/random class " + str(c) + "/R_" + str(n)
            + "_" + t1 + "/" + str(i) + "_" + str(n) + "_100_" + t2 + ".dat"
            for c in [1, 2, 3] for t1, t2 in [("S", "10_50"), ("L", "25_75")]
            for n in [10, 20, 30, 40, 50] for i in range(1, 16)]:
        convert_generic(f, "nwh", "pwhc")

    for f in [
            "ortmann2010/" + t + str(n) + "i" + str(m) + "b" + str(i)
            for t in ["Nice", "Path"]
            for n in [25, 50, 100, 200, 300, 400, 500]
            for m in [2, 3, 4, 5, 6]
            for i in range(1, 6)
            if n != 25 or m != 6]:
        convert_vbpp(f, "mn", "whcxx", "", "whxx")

    for f in ["leung2011/zdf" + str(i) for i in range(1, 17)]:
        convert_generic(f, "nw", "xwh")

    for f in [
            "cui2012/" + str(i) + c + "-" + str(j)
            for c in ["A", "B"]
            for i in range(1, 4)
            for j in range(1, 11)]:
        convert_generic(f, "whn", "whcxp")

    for f in ["hifi2012/UL" + i + "H.txt" for i in ["1", "2", "3"]]:
        convert_generic(f, "whnxx", "whc")
    for f in ["hifi2012/WL" + i + "H.txt" for i in ["1", "2", "3"]]:
        convert_generic(f, "whnxx", "whpc")

    for f in [
            "silveira2013/2lcvrp/mod_2l_cvrp" + "{:02d}".format(i)
            + "{:02d}".format(j) + ".txt"
            for i in range(1, 37)
            for j in range(1, 6)]:
        convert_silveira2013(f)
    for f in [
            "silveira2013/bea/T" + str(k) + "/GCUT"
            + "{:02d}".format(i) + ".TXT"
            for k in [20, 40, 60, 80, 100]
            for i in range(1, 14)]:
        convert_silveira2013(f)
    for f in [
            "silveira2013/bea/T" + str(k) + "/NGCUT"
            + "{:02d}".format(i) + ".TXT"
            for k in [20, 40, 60, 80, 100]
            for i in range(1, 13)]:
        convert_silveira2013(f)
    for f in [
            "silveira2013/ben/T" + str(k) + "/BENG"
            + "{:02d}".format(i) + ".TXT"
            for k in [20, 40, 60, 80, 100]
            for i in range(1, 11)]:
        convert_silveira2013(f)
    for f in [
            "silveira2013/bke/T" + str(k) + "/N" + str(i) + "Burke.txt"
            for k in [20, 40, 60, 80, 100]
            for i in range(1, 13)]:
        convert_silveira2013(f)
    for f in [
            "silveira2013/chr/T" + str(k) + "/CGCUT"
            + "{:02d}".format(i) + ".TXT"
            for k in [20, 40, 60, 80, 100]
            for i in range(1, 4)]:
        convert_silveira2013(f)
    for f in [
            "silveira2013/hop/T" + str(k) + "/Hopper" + a + str(b) + c + ".txt"
            for k in [20, 40, 60, 80, 100]
            for a in ["N", "T"]
            for b in range(1, 8)
            for c in ["a", "b", "c", "d", "e"]]:
        convert_silveira2013(f)
    for f in [
            "silveira2013/htu/T" + str(k) + "/c" + str(i) + "-p" + str(j)
            + "(Hopper).txt"
            for k in [20, 40, 60, 80, 100]
            for i in range(1, 8)
            for j in range(1, 4)]:
        convert_silveira2013(f)

    for f in [
            "afsharian2014/" + i
            for i in ["75-75.txt", "112-50.txt", "150-150.txt",
                      "225-100.txt", "300-300.txt", "450-200.txt"]]:
        convert_afsharian2014(f)

    for wh in ["W500H1000", "W1000H2000"]:
        for n in [50, 100, 150]:
            for f in [
                    "clautiaux2018/a/" + f.strip()
                    for f in open(
                        "data/rectangle_raw/clautiaux2018/A_"
                        + wh + "I" + str(n))]:
                convert_generic(f, "whn", "whpc")
            for f in [
                    "clautiaux2018/p/" + f.strip()
                    for f in open(
                        "data/rectangle_raw/clautiaux2018/P_"
                        + wh + "I" + str(n))]:
                convert_generic(f, "whn", "whpc")

    for f in ["roadef2018/A" + str(i) for i in range(1, 21)]:
        convert_roadef2018(f)
    for f in ["roadef2018/B" + str(i) for i in range(1, 16)]:
        convert_roadef2018(f)
    for f in ["roadef2018/X" + str(i) for i in range(1, 16)]:
        convert_roadef2018(f)

    # for f in [
    #         "martin2019a/os" + o + "_is" + i + "_m" + m + "_" + j
    #         for o in ["02", "06"]
    #         for i in ["01", "02", "03", "04", "06",
    #                   "07", "08", "11", "12", "16"]
    #         for m in ["10", "20", "40"]
    #         for j in ["01", "02", "03", "04", "05"]]:
    #     convert_generic(f, "whn", "whc")
    # for f in [
    #         "martin2019b/inst_" + LW + "_" + str(m) + "_" + str(rho)
    #         + "_" + str(i) + "_" + str(d)
    #         for LW in ["75_75", "125_50", "150_150",
    #                    "225_100", "300_300", "450_200"]
    #         for m in [5, 10, 15, 20, 25]
    #         for rho in [6, 8, 10]
    #         for i in range(1, 16)
    #         for d in [1, 2, 3, 4]]:
    #     convert_martin2019b(f)

    for f in [
            "velasco2019/P" + str(cl) + "_" + str(l) + "_" + str(h) + "_"
            + str(m) + "_" + str(i) + ".txt"
            for cl, l, h in [(1, 100, 200), (1, 100, 400), (2, 200, 100),
                             (2, 400, 100), (3, 150, 150), (3, 250, 250),
                             (4, 150, 150), (4, 250, 250)]
            for m in [25, 50] for i in range(1, 6)]:
        convert_generic(f, "whn", "whpc")

    for f in ["long2020/Instance " + str(i) + ".txt" for i in range(1, 26)]:
        convert_long2020(f)

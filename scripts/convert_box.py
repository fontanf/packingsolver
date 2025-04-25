import os
import os.path


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


def convert_bischoff1995(filename):
    f = open("data/box_raw/" + filename, "r")
    line = f.readline().strip().split(" ")
    number_of_instances = int(line[0])
    for _ in range(number_of_instances):
        line = f.readline().strip().split(" ")
        instance_id = int(line[0])
        path = filename + "_" + str(instance_id)
        # Bin.
        bins = {"X": [], "Y": [], "Z": []}
        line = f.readline().strip().split(" ")
        bins["X"].append(int(line[0]))
        bins["Y"].append(int(line[1]))
        bins["Z"].append(int(line[2]))
        # Items.
        items = {"X": [], "Y": [], "Z": [], "ROTATIONS": [], "COPIES": []}
        line = f.readline().strip().split(" ")
        number_of_item_types = int(line[0])
        for j in range(number_of_item_types):
            line = f.readline().replace("\t", " ").strip().split(" ")
            if len(line) == 8:
                items["X"].append(int(line[1]))
                items["Y"].append(int(line[3]))
                items["Z"].append(int(line[5]))
                items["COPIES"].append(int(line[7]))
                rx = int(line[2])
                ry = int(line[4])
                rz = int(line[6])
            else:  # Because on one buggy line in thpack9.txt instance 17.
                items["X"].append(int(line[1]))
                items["Y"].append(int(line[3]))
                items["Z"].append(int(line[4]))
                items["COPIES"].append(int(line[6]))
                rx = int(line[2])
                ry = 0
                rz = int(line[5])
            if rx == 0 and ry == 0 and rz == 1:
                items["ROTATIONS"].append(int("000011", 2))
            elif rx == 1 and ry == 0 and rz == 1:
                items["ROTATIONS"].append(int("001111", 2))
            elif rx == 0 and ry == 1 and rz == 1:
                items["ROTATIONS"].append(int("110011", 2))
            elif rx == 1 and ry == 1 and rz == 1:
                items["ROTATIONS"].append(int("111111", 2))
            else:
                print(rx, ry, rz)
        write_dict(bins, path + "_bins.csv")
        write_dict(items, path + "_items.csv")


def convert_egeblad2009(filename):
    bins = {"X": [], "Y": [], "Z": []}
    items = {"X": [], "Y": [], "Z": [], "PROFIT": [], "COPIES": []}

    f = open("data/box_raw/" + filename, "r")
    line = f.readline().split(",")
    bins["X"].append(int(line[1]))
    bins["Y"].append(int(line[2]))
    bins["Z"].append(int(line[3]))
    while True:
        line = f.readline()
        if not line:
            break
        line = line.split(",")
        items["X"].append(int(line[2]))
        items["Y"].append(int(line[3]))
        items["Z"].append(int(line[4]))
        items["PROFIT"].append(int(line[5]))
        items["COPIES"].append(int(line[6]))

    write_dict(bins, filename + "_bins.csv")
    write_dict(items, filename + "_items.csv")


if __name__ == "__main__":

    for f in ["bischoff1995/BR" + str(i) + ".txt" for i in range(1, 8)]:
        convert_bischoff1995(f)
    for f in ["davies1999/BR" + str(i) + ".txt" for i in [0] + list(range(8, 16))]:
        convert_bischoff1995(f)
    convert_bischoff1995("loh1992/thpack8.txt")
    convert_bischoff1995("ivancic1989/thpack9.txt")

    for f in [
            "egeblad2009/ep3d-" + i + "-" + a + "-" + b + "-" + j + ".3kp"
            for i in ["20", "40", "60"]
            for a in ["F", "L", "C", "U", "D"]
            for b in ["C", "R"]
            for j in ["50", "90"]]:
        convert_egeblad2009(f)

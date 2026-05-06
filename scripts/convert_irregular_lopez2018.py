import os
import os.path
import json


def words(filename):
    f = open(os.path.join("data", "irregular_raw", filename), "r")
    for line in f:
        for word in line.split():
            yield word


def write_dict(dic, filename):
    p = os.path.join("data", "irregular", filename.replace(" ", "_") + ".json")
    d = os.path.dirname(p)
    if not os.path.exists(d):
        os.makedirs(d)
    print("Create " + p)
    with open(p, "w") as f:
        json.dump(dic, f, indent=4)


def convert_lopez2018(filename, number_of_radii, squares=False):
    w = words(filename)
    dic = {
        "objective": "knapsack",
        "bin_types": [{
            "type": "circle",
            "radius": None,
            }],
        "item_types": [],
    }
    number_of_items = int(next(w))
    radii = []
    for _ in range(number_of_radii):
        radii.append(float(next(w)))
    for _ in range(number_of_items):
        l1 = float(next(w))
        if squares:
            l2 = l1
        else:
            l2 = float(next(w))
        dic["item_types"].append({
            "type": "rectangle",
            "height": l1,
            "width": l2,
            "profit": None,
            "allowed_rotations": None})

    for obj in ["max_num_items", "max_area"]:
        for rotation in [False, True]:
            if squares and rotation:
                continue
            name = filename
            allowed_rotations = [{"start": 0, "end": 0, "mirror": False}]
            if rotation:
                allowed_rotations.append({"start": 90, "end": 90, "mirror": False})
                name += "_rotation"
            else:
                name += "_oriented"
            if obj == "max_num_items":
                name += "_maxnumitems"
            else:
                name += "_maxarea"

            for item_id in range(number_of_items):
                item = dic["item_types"][item_id]
                if obj == "max_num_items":
                    item["profit"] = 1
                else:
                    item["profit"] = item["width"] * item["height"]
                item["allowed_rotations"] = allowed_rotations
            if number_of_radii == 1:
                write_dict(dic, name)
            else:
                for radius in radii:
                    dic["bin_types"][0]["radius"] = radius
                    write_dict(dic, name + "_" + str(radius))


if __name__ == "__main__":
    convert_lopez2018(os.path.join("lopez2018", "square1.txt"), 3, True)
    convert_lopez2018(os.path.join("lopez2018", "square2.txt"), 3, True)
    convert_lopez2018(os.path.join("lopez2018", "square3.txt"), 3, True)
    convert_lopez2018(os.path.join("lopez2018", "rect1.txt"), 3, False)
    convert_lopez2018(os.path.join("lopez2018", "rect2.txt"), 3, False)
    convert_lopez2018(os.path.join("lopez2018", "rect3.txt"), 3, False)
    convert_lopez2018(os.path.join("bouzid2020", "square100.txt"), 3, True)
    convert_lopez2018(os.path.join("bouzid2020", "square150.txt"), 3, True)
    convert_lopez2018(os.path.join("bouzid2020", "square200.txt"), 3, True)
    convert_lopez2018(os.path.join("bouzid2020", "rect100.txt"), 3, False)
    convert_lopez2018(os.path.join("bouzid2020", "rect150.txt"), 3, False)
    convert_lopez2018(os.path.join("bouzid2020", "rect200.txt"), 3, False)
    convert_lopez2018(os.path.join("silva2021", "rect30_S1.txt"), 1, False)
    convert_lopez2018(os.path.join("silva2021", "rect30_S2.txt"), 1, False)
    convert_lopez2018(os.path.join("silva2021", "rect30_S3.txt"), 1, False)
    convert_lopez2018(os.path.join("silva2021", "rect30_S4.txt"), 1, False)
    convert_lopez2018(os.path.join("silva2021", "rect30_S5.txt"), 1, False)

import os
import os.path
import json
import re


def write_dict(dic, filename):
    p = os.path.join("data", "irregular", filename.replace(" ", "_") + ".json")
    d = os.path.dirname(p)
    if not os.path.exists(d):
        os.makedirs(d)
    print("Create " + p)
    with open(p, 'w') as f:
        json.dump(dic, f, indent=4)


def convert_packomania_coop():

    widths = {}
    with open("data/irregular_raw/packomania/coop/width.txt") as f:
        for line in f:
            l = re.sub(' +', ' ', line).strip().split(" ")
            widths[l[0]] = float(l[1])

    instance_names = [
            "SY1-30",
            "SY2-20",
            "SY3-25",
            "SY4-35",
            "SY5-100",
            "SY6-100",
            "SY12-50",
            "SY13-55",
            "SY14-65",
            "SY23-45",
            "SY24-55",
            "SY34-60",
            "SY36-125",
            "SY56-200",
            "SY123-75",
            "SY124-85",
            "SY125-150",
            "SY134-90",
            "SY234-80",
            "SY356-225",
            "SY565-300",
            "SY1234-110",
            "SY1236-175",
            "SY1256-250",
            "SY12356-275",
            "KBG1-25",
            "KBG2-25",
            "KBG3-25",
            "KBG4-25",
            "KBG5-25",
            "KBG6-25",
            "KBG7-25",
            "KBG8-25",
            "KBG9-25",
            "KBG10-25",
            "KBG11-25",
            "KBG12-25",
            "KBG13-25",
            "KBG14-25",
            "KBG15-25",
            "KBG16-25",
            "KBG17-25",
            "KBG18-25",
            "KBG19-25",
            "KBG20-25",
            "KBG21-25",
            "KBG22-25",
            "KBG23-25",
            "KBG24-25",
            "KBG25-25",
            "KBG26-25",
            "KBG27-25",
            "KBG28-25",
            "KBG29-25",
            "KBG30-25",
            "KBG31-25",
            "KBG32-25",
            "KBG33-50",
            "KBG34-50",
            "KBG35-50",
            "KBG36-50",
            "KBG37-50",
            "KBG38-50",
            "KBG39-50",
            "KBG40-50",
            "KBG41-50",
            "KBG42-50",
            "KBG43-50",
            "KBG44-50",
            "KBG45-50",
            "KBG46-50",
            "KBG47-50",
            "KBG48-50",
            "KBG49-50",
            "KBG50-50",
            "KBG51-50",
            "KBG52-50",
            "KBG53-50",
            "KBG54-50",
            "KBG55-50",
            "KBG56-50",
            "KBG57-50",
            "KBG58-50",
            "KBG59-50",
            "KBG60-50",
            "KBG61-50",
            "KBG62-50",
            "KBG63-50",
            "KBG64-50",
            "KBG65-75",
            "KBG66-75",
            "KBG67-75",
            "KBG68-75",
            "KBG69-75",
            "KBG70-75",
            "KBG71-75",
            "KBG72-75",
            "KBG73-75",
            "KBG74-75",
            "KBG75-75",
            "KBG76-75",
            "KBG77-75",
            "KBG78-75",
            "KBG79-75",
            "KBG80-75",
            "KBG81-75",
            "KBG82-75",
            "KBG83-75",
            "KBG84-75",
            "KBG85-75",
            "KBG86-75",
            "KBG87-75",
            "KBG88-75",
            "KBG89-75",
            "KBG90-75",
            "KBG91-75",
            "KBG92-75",
            "KBG93-75",
            "KBG94-75",
            "KBG95-75",
            "KBG96-75",
            "KBG97-100",
            "KBG98-100",
            "KBG99-100",
            "KBG100-100",
            "KBG101-100",
            "KBG102-100",
            "KBG103-100",
            "KBG104-100",
            "KBG105-100",
            "KBG106-100",
            "KBG107-100",
            "KBG108-100",
            "KBG109-100",
            "KBG110-100",
            "KBG111-100",
            "KBG112-100",
            "KBG113-100",
            "KBG114-100",
            "KBG115-100",
            "KBG116-100",
            "KBG117-100",
            "KBG118-100",
            "KBG119-100",
            "KBG120-100",
            "KBG121-100",
            "KBG122-100",
            "KBG123-100",
            "KBG124-100",
            "KBG125-100",
            "KBG126-100",
            "KBG127-100",
            "KBG128-100"]

    for instance_name in instance_names:

        path = os.path.join("packomania", "coop", instance_name + ".txt")

        dic = {
            "bin_types": [
                {
                    "type": "rectangle",
                    "height": widths[instance_name.split('-', 1)[0]],
                    "length": 100,
                }
            ],
            "item_types": [
            ]
        }
        with open("data/irregular_raw/" + path) as f:
            for line in f:
                l = re.sub(' +', ' ', line).strip().split(" ")
                r = float(l[3])
                dic["item_types"].append({
                    "type": "circle",
                    "radius": r,
                })

        write_dict(dic, path)


if __name__ == "__main__":

    convert_packomania_coop()

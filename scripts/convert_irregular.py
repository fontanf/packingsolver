import os
import os.path
import json
import xml.etree.ElementTree as ET


def write_dict(dic, filename):
    p = os.path.join("data", "irregular", filename.replace(" ", "_") + ".json")
    d = os.path.dirname(p)
    if not os.path.exists(d):
        os.makedirs(d)
    print("Create " + p)
    with open(p, "w") as f:
        json.dump(dic, f, indent=4)


def convert_esicup(
        filename,
        url = "http://www.fe.up.pt/~esicup/nesting.xsd"):
    path = os.path.join("data", "irregular_raw", filename)

    dic = {
        "objective": "open-dimension-x",
        "bin_types": [],
        "item_types": [],
    }

    tree = ET.parse(path)
    root = tree.getroot()

    ns = '{' + url + '}'

    # Read polygons.
    polygons = {}
    for sec_polygon in root.find(ns + 'polygons').iter(ns + "polygon"):
        key = sec_polygon.get('id')
        elements = []
        for sec_segment in sec_polygon.find(ns + 'lines').iter(ns + "segment"):
            elements.append({
                    "type": "line_segment",
                    "start": {
                        "x": float(sec_segment.get('x0')),
                        "y": float(sec_segment.get('y0'))},
                    "end": {
                        "x": float(sec_segment.get('x1')),
                        "y": float(sec_segment.get('y1'))}})
        polygons[key] = elements

    # Read bin types.
    for sec_piece in root.find(ns + 'problem').find(ns + 'boards').iter(ns + "piece"):
        copies = int(sec_piece.get('quantity'))
        polygon = sec_piece.find(ns + 'component').get('idPolygon')
        dic["bin_types"].append(
            {
                "type": "general",
                "copies": copies,
                "elements": polygons[polygon],
            }
        )

    # Read item types.
    for sec_piece in root.find(ns + 'problem').find(ns + 'lot').iter(ns + "piece"):
        copies = int(sec_piece.get('quantity'))
        allowed_rotations = []
        for sec_angle in sec_piece.find(ns + 'orientation').iter(ns + "enumeration"):
            angle = float(sec_angle.get('angle'))
            allowed_rotations.append({"start": angle, "end": angle, "mirror": False})
        polygon = sec_piece.find(ns + 'component').get('idPolygon')
        dic["item_types"].append(
            {
                "type": "general",
                "copies": copies,
                "allowed_rotations": allowed_rotations,
                "elements": polygons[polygon],
            }
        )

    write_dict(dic, filename)


def convert_instance(input_path, output_path, conversion):
    import subprocess
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    print("Create " + output_path)
    subprocess.run(
        [
            os.path.join("install", "bin", "irregular_convert"),
            "--input", input_path,
            "--output", output_path,
            "--conversion", conversion,
        ],
        check=True,
    )



if __name__ == "__main__":

    convert_esicup(os.path.join("albano1980", "albano_2007-05-15", "albano.xml"))
    convert_esicup(os.path.join("bounsaythip1997", "mao_2007-04-23", "mao.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("dighe1996", "dighe_2007-05-15", "dighe1.xml"))
    convert_esicup(os.path.join("dighe1996", "dighe_2007-05-15", "dighe2.xml"))
    convert_esicup(os.path.join("fujita1993", "fu_2007-05-15", "fu.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("han1996", "han_2007-04-23", "han.xml"))
    convert_esicup(os.path.join("hopper2000", "poly1a.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("hopper2000", "poly2a.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("hopper2000", "poly2b.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("hopper2000", "poly3a.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("hopper2000", "poly3b.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("hopper2000", "poly4a.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("hopper2000", "poly4b.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("hopper2000", "poly5a.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("hopper2000", "poly5b.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("jakobs1996", "jakobs_2007-04-23", "jakobs1.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("jakobs1996", "jakobs_2007-04-23", "jakobs2.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("marques1991", "marques_2007-05-15", "marques.xml"), "http://globalnest.fe.up.pt/nesting")
    convert_esicup(os.path.join("oliveira2000", "blaz_2007-04-23", "blaz.xml"))
    convert_esicup(os.path.join("oliveira2000", "shapes_2007-04-23", "shapes0.xml"))
    convert_esicup(os.path.join("oliveira2000", "shapes_2007-04-23", "shapes1.xml"))
    convert_esicup(os.path.join("oliveira2000", "shirts_2007-05-15", "shirts.xml"))
    convert_esicup(os.path.join("oliveira2000", "swim_2007-05-15", "swim.xml"))
    convert_esicup(os.path.join("oliveira2000", "trousers_2007-05-15", "trousers.xml"))
    convert_esicup(os.path.join("ratanapan1997", "dagli_2007-05-15", "dagli.xml"), "http://globalnest.fe.up.pt/nesting")



    song2014_instances = [
        ("albano1980/albano_2007-05-15/albano.xml.json",                    "albano.json"),
        ("dighe1996/dighe_2007-05-15/dighe1.xml.json",                      "dighe1.json"),
        ("dighe1996/dighe_2007-05-15/dighe2.xml.json",                      "dighe2.json"),
        ("fujita1993/fu_2007-05-15/fu.xml.json",                            "fu.json"),
        ("jakobs1996/jakobs_2007-04-23/jakobs1.xml.json",                   "jakobs1.json"),
        ("bounsaythip1997/mao_2007-04-23/mao.xml.json",                     "mao.json"),
        ("marques1991/marques_2007-05-15/marques.xml.json",                 "marques.json"),
        ("hopper2000/poly5b.xml.json",                                       "poly5b.json"),
        ("oliveira2000/shapes_2007-04-23/shapes0.xml.json",                 "shapes1.json"),
        ("oliveira2000/shapes_2007-04-23/shapes1.xml.json",                 "shapes2.json"),
        ("oliveira2000/shirts_2007-05-15/shirts.xml.json",                  "shirts.json"),
        ("oliveira2000/swim_2007-05-15/swim.xml.json",                      "swim.json"),
        ("oliveira2000/trousers_2007-05-15/trousers.xml.json",              "trousers.json"),
        ("ratanapan1997/dagli_2007-05-15/dagli.xml.json",                   "dagli.json"),
    ]
    for input_rel, output_name in song2014_instances:
        convert_instance(
            os.path.join("data", "irregular", input_rel),
            os.path.join("data", "irregular", "song2014", output_name),
            "song2014")

    martinez2017_instances = [
        ("albano1980/albano_2007-05-15/albano.xml.json",                    "albano"),
        ("oliveira2000/blaz_2007-04-23/blaz.xml.json",                      "shapes2"),
        ("oliveira2000/trousers_2007-05-15/trousers.xml.json",              "trousers"),
        ("oliveira2000/shapes_2007-04-23/shapes0.xml.json",                 "shapes0"),
        ("oliveira2000/shapes_2007-04-23/shapes1.xml.json",                 "shapes1"),
        ("oliveira2000/shirts_2007-05-15/shirts.xml.json",                  "shirts"),
        ("dighe1996/dighe_2007-05-15/dighe2.xml.json",                      "dighe2"),
        ("dighe1996/dighe_2007-05-15/dighe1.xml.json",                      "dighe1"),
        ("fujita1993/fu_2007-05-15/fu.xml.json",                            "fu"),
        ("han1996/han_2007-04-23/han.xml.json",                             "han"),
        ("jakobs1996/jakobs_2007-04-23/jakobs1.xml.json",                   "jakobs1"),
        ("jakobs1996/jakobs_2007-04-23/jakobs2.xml.json",                   "jakobs2"),
        ("bounsaythip1997/mao_2007-04-23/mao.xml.json",                     "mao"),
        ("hopper2000/poly1a.xml.json",                                       "poly1a"),
        ("hopper2000/poly2a.xml.json",                                       "poly2a"),
        ("hopper2000/poly3a.xml.json",                                       "poly3a"),
        ("hopper2000/poly4a.xml.json",                                       "poly4a"),
        ("hopper2000/poly5a.xml.json",                                       "poly5a"),
        ("hopper2000/poly2b.xml.json",                                       "poly2b"),
        ("hopper2000/poly3b.xml.json",                                       "poly3b"),
        ("hopper2000/poly4b.xml.json",                                       "poly4b"),
        ("hopper2000/poly5b.xml.json",                                       "poly5b"),
        ("oliveira2000/swim_2007-05-15/swim.xml.json",                      "swimm"),
    ]
    for input_rel, output_name in martinez2017_instances:
        for suffix, conversion in [
                ("sb", "martinez2017_sb"),
                ("mb", "martinez2017_mb"),
                ("lb", "martinez2017_lb")]:
            convert_instance(
                os.path.join("data", "irregular", input_rel),
                os.path.join("data", "irregular", "martinez2017",
                             output_name + "_" + suffix + ".json"),
                conversion)

import os
import os.path
import json
import re
import xml.etree.ElementTree as ET


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


def convert_packomania_coop():

    widths = {}
    with open("data/irregular_raw/packomania/coop/width.txt") as f:
        for line in f:
            l = re.sub(" +", " ", line).strip().split(" ")
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
        "KBG128-100",
    ]

    for instance_name in instance_names:

        path = os.path.join("packomania", "coop", instance_name + ".txt")

        dic = {
            "bin_types": [
                {
                    "type": "rectangle",
                    "height": widths[instance_name.split("-", 1)[0]],
                    "length": 100,
                }
            ],
            "item_types": [],
        }
        with open("data/irregular_raw/" + path) as f:
            for line in f:
                l = re.sub(" +", " ", line).strip().split(" ")
                r = float(l[3])
                dic["item_types"].append(
                    {
                        "type": "circle",
                        "radius": r,
                    }
                )

        write_dict(dic, path)


def convert_cgshop2024():

    instance_names = {
        os.path.join("examples_2023-08-17", "examples_00"): [
            "jigsaw_a_1376.cgshop2024_instance.json",
            "jigsaw_a_146.cgshop2024_instance.json",
            "jigsaw_a_155.cgshop2024_instance.json",
            "jigsaw_a_367.cgshop2024_instance.json",
            "jigsaw_a_36.cgshop2024_instance.json",
            "jigsaw_a_380.cgshop2024_instance.json",
            "jigsaw_a_47.cgshop2024_instance.json",
            "jigsaw_a_713.cgshop2024_instance.json",
            "jigsaw_a_72.cgshop2024_instance.json",
            "jigsaw_a_83.cgshop2024_instance.json",
            "jigsaw_b_1395.cgshop2024_instance.json",
            "jigsaw_b_153.cgshop2024_instance.json",
            "jigsaw_b_34.cgshop2024_instance.json",
            "jigsaw_b_366.cgshop2024_instance.json",
            "jigsaw_b_376.cgshop2024_instance.json",
            "jigsaw_b_40.cgshop2024_instance.json",
            "jigsaw_b_659.cgshop2024_instance.json",
            "jigsaw_b_74.cgshop2024_instance.json",
            "jigsaw_b_90.cgshop2024_instance.json",
            "random_a_10000.cgshop2024_instance.json",
            "random_a_1000.cgshop2024_instance.json",
            "random_a_100.cgshop2024_instance.json",
            "random_a_2000.cgshop2024_instance.json",
            "random_a_200.cgshop2024_instance.json",
            "random_a_50000.cgshop2024_instance.json",
            "random_a_5000.cgshop2024_instance.json",
            "random_a_500.cgshop2024_instance.json",
            "random_a_50.cgshop2024_instance.json",
            "random_b_10000.cgshop2024_instance.json",
            "random_b_1000.cgshop2024_instance.json",
            "random_b_100.cgshop2024_instance.json",
            "random_b_2000.cgshop2024_instance.json",
            "random_b_200.cgshop2024_instance.json",
            "random_b_50000.cgshop2024_instance.json",
            "random_b_5000.cgshop2024_instance.json",
            "random_b_500.cgshop2024_instance.json",
            "random_b_50.cgshop2024_instance.json",
            "random_c_10000.cgshop2024_instance.json",
            "random_c_1000.cgshop2024_instance.json",
            "random_c_100.cgshop2024_instance.json",
            "random_c_2000.cgshop2024_instance.json",
            "random_c_200.cgshop2024_instance.json",
            "random_c_50000.cgshop2024_instance.json",
            "random_c_5000.cgshop2024_instance.json",
            "random_c_500.cgshop2024_instance.json",
            "random_c_50.cgshop2024_instance.json",
        ],
        os.path.join("examples_2023-08-17", "examples_01"): [
            "atris42.cgshop2024_instance.json",
            "atris1064.cgshop2024_instance.json",
            "atris1121.cgshop2024_instance.json",
            "atris1207.cgshop2024_instance.json",
            "atris1310.cgshop2024_instance.json",
            "atris1382.cgshop2024_instance.json",
            "atris1416.cgshop2024_instance.json",
            "atris1594.cgshop2024_instance.json",
            "atris1802.cgshop2024_instance.json",
            "atris1992.cgshop2024_instance.json",
            "atris2050.cgshop2024_instance.json",
            "atris2264.cgshop2024_instance.json",
            "atris2685.cgshop2024_instance.json",
            "atris2971.cgshop2024_instance.json",
            "atris10133.cgshop2024_instance.json",
            "atris10683.cgshop2024_instance.json",
            "atris11081.cgshop2024_instance.json",
            "atris12066.cgshop2024_instance.json",
            "atris13376.cgshop2024_instance.json",
            "atris14117.cgshop2024_instance.json",
            "atris15164.cgshop2024_instance.json",
            "atris16469.cgshop2024_instance.json",
            "atris17637.cgshop2024_instance.json",
            "atris18736.cgshop2024_instance.json",
            "atris20713.cgshop2024_instance.json",
            "atris21402.cgshop2024_instance.json",
            "atris22621.cgshop2024_instance.json",
            "atris22900.cgshop2024_instance.json",
            "atris24253.cgshop2024_instance.json",
            "atris25847.cgshop2024_instance.json",
            "atris26655.cgshop2024_instance.json",
            "atris27305.cgshop2024_instance.json",
            "atris28735.cgshop2024_instance.json",
            "atris29936.cgshop2024_instance.json",
            "atris31492.cgshop2024_instance.json",
            "atris3155.cgshop2024_instance.json",
            "atris32352.cgshop2024_instance.json",
            "atris35600.cgshop2024_instance.json",
            "atris3579.cgshop2024_instance.json",
            "atris3623.cgshop2024_instance.json",
            "atris36950.cgshop2024_instance.json",
            "atris38752.cgshop2024_instance.json",
            "atris39880.cgshop2024_instance.json",
            "atris4051.cgshop2024_instance.json",
            "atris42100.cgshop2024_instance.json",
            "atris4344.cgshop2024_instance.json",
            "atris44685.cgshop2024_instance.json",
            "atris47360.cgshop2024_instance.json",
            "atris4753.cgshop2024_instance.json",
            "atris49366.cgshop2024_instance.json",
            "atris5423.cgshop2024_instance.json",
            "atris5677.cgshop2024_instance.json",
            "atris5907.cgshop2024_instance.json",
            "atris6351.cgshop2024_instance.json",
            "atris6396.cgshop2024_instance.json",
            "atris7374.cgshop2024_instance.json",
            "atris7452.cgshop2024_instance.json",
            "atris8110.cgshop2024_instance.json",
            "atris9174.cgshop2024_instance.json",
            "atris9936.cgshop2024_instance.json",
            "jigsaw_a_1000_2_3315.cgshop2024_instance.json",
            "jigsaw_a_1000_2_6549.cgshop2024_instance.json",
            "jigsaw_a_1000_2_72.cgshop2024_instance.json",
            "jigsaw_a_1000_3_122.cgshop2024_instance.json",
            "jigsaw_a_1000_3_32.cgshop2024_instance.json",
            "jigsaw_a_1000_3_674.cgshop2024_instance.json",
            "jigsaw_a_1000_4_1348.cgshop2024_instance.json",
            "jigsaw_a_1000_4_32845.cgshop2024_instance.json",
            "jigsaw_a_1000_4_347.cgshop2024_instance.json",
            "jigsaw_a_100_1_32878.cgshop2024_instance.json",
            "jigsaw_a_100_2_131.cgshop2024_instance.json",
            "jigsaw_a_100_2_3280.cgshop2024_instance.json",
            "jigsaw_a_100_2_355.cgshop2024_instance.json",
            "jigsaw_a_100_2_657.cgshop2024_instance.json",
            "jigsaw_a_100_2_6612.cgshop2024_instance.json",
            "jigsaw_a_100_3_74.cgshop2024_instance.json",
            "jigsaw_a_100_4_1335.cgshop2024_instance.json",
            "jigsaw_a_100_4_38.cgshop2024_instance.json",
            "jigsaw_a_10_2_1310.cgshop2024_instance.json",
            "jigsaw_a_10_2_33.cgshop2024_instance.json",
            "jigsaw_a_10_3_149.cgshop2024_instance.json",
            "jigsaw_a_10_3_32791.cgshop2024_instance.json",
            "jigsaw_a_10_3_3316.cgshop2024_instance.json",
            "jigsaw_a_10_3_66.cgshop2024_instance.json",
            "jigsaw_a_10_3_689.cgshop2024_instance.json",
            "jigsaw_a_10_4_357.cgshop2024_instance.json",
            "jigsaw_a_10_4_6646.cgshop2024_instance.json",
            "jigsaw_b_1000_1_32715.cgshop2024_instance.json",
            "jigsaw_b_1000_2_134.cgshop2024_instance.json",
            "jigsaw_b_1000_2_34.cgshop2024_instance.json",
            "jigsaw_b_1000_2_6595.cgshop2024_instance.json",
            "jigsaw_b_1000_3_3276.cgshop2024_instance.json",
            "jigsaw_b_1000_3_330.cgshop2024_instance.json",
            "jigsaw_b_1000_3_668.cgshop2024_instance.json",
            "jigsaw_b_1000_4_1355.cgshop2024_instance.json",
            "jigsaw_b_1000_4_78.cgshop2024_instance.json",
            "jigsaw_b_100_2_1334.cgshop2024_instance.json",
            "jigsaw_b_100_2_138.cgshop2024_instance.json",
            "jigsaw_b_100_2_32750.cgshop2024_instance.json",
            "jigsaw_b_100_2_3284.cgshop2024_instance.json",
            "jigsaw_b_100_2_334.cgshop2024_instance.json",
            "jigsaw_b_100_2_62.cgshop2024_instance.json",
            "jigsaw_b_100_3_34.cgshop2024_instance.json",
            "jigsaw_b_100_3_662.cgshop2024_instance.json",
            "jigsaw_b_100_4_6606.cgshop2024_instance.json",
            "jigsaw_b_10_2_142.cgshop2024_instance.json",
            "jigsaw_b_10_2_30.cgshop2024_instance.json",
            "jigsaw_b_10_2_3292.cgshop2024_instance.json",
            "jigsaw_b_10_3_1320.cgshop2024_instance.json",
            "jigsaw_b_10_3_32916.cgshop2024_instance.json",
            "jigsaw_b_10_3_337.cgshop2024_instance.json",
            "jigsaw_b_10_3_680.cgshop2024_instance.json",
            "jigsaw_b_10_3_77.cgshop2024_instance.json",
            "jigsaw_b_10_4_6646.cgshop2024_instance.json",
            "random_a_298134_500.cgshop2024_instance.json",
            "random_a_350917_50.cgshop2024_instance.json",
            "random_a_366032_5000.cgshop2024_instance.json",
            "random_a_464951_2000.cgshop2024_instance.json",
            "random_a_657195_10000.cgshop2024_instance.json",
            "random_a_880854_1000.cgshop2024_instance.json",
            "random_a_970407_200.cgshop2024_instance.json",
            "random_a_978906_50000.cgshop2024_instance.json",
            "random_a_994489_100.cgshop2024_instance.json",
            "random_b_040139_50000.cgshop2024_instance.json",
            "random_b_082939_500.cgshop2024_instance.json",
            "random_b_228742_1000.cgshop2024_instance.json",
            "random_b_244251_10000.cgshop2024_instance.json",
            "random_b_309845_100.cgshop2024_instance.json",
            "random_b_373099_50.cgshop2024_instance.json",
            "random_b_446608_2000.cgshop2024_instance.json",
            "random_b_465665_200.cgshop2024_instance.json",
            "random_b_747025_5000.cgshop2024_instance.json",
            "random_c_006136_200.cgshop2024_instance.json",
            "random_c_097466_50.cgshop2024_instance.json",
            "random_c_169102_50000.cgshop2024_instance.json",
            "random_c_219514_1000.cgshop2024_instance.json",
            "random_c_332893_100.cgshop2024_instance.json",
            "random_c_458887_500.cgshop2024_instance.json",
            "random_c_518219_10000.cgshop2024_instance.json",
            "random_c_562370_2000.cgshop2024_instance.json",
            "random_c_964335_5000.cgshop2024_instance.json",
        ],
        os.path.join("cgshop2024_benchmark", "instances"): [
            "atris10619.cgshop2024_instance.json",
            "atris10681.cgshop2024_instance.json",
            "atris11358.cgshop2024_instance.json",
            "atris11997.cgshop2024_instance.json",
            "atris1240.cgshop2024_instance.json",
            "atris1266.cgshop2024_instance.json",
            "atris13846.cgshop2024_instance.json",
            "atris15979.cgshop2024_instance.json",
            "atris1660.cgshop2024_instance.json",
            "atris1672.cgshop2024_instance.json",
            "atris1797.cgshop2024_instance.json",
            "atris18921.cgshop2024_instance.json",
            "atris19260.cgshop2024_instance.json",
            "atris20367.cgshop2024_instance.json",
            "atris2090.cgshop2024_instance.json",
            "atris2812.cgshop2024_instance.json",
            "atris2890.cgshop2024_instance.json",
            "atris2986.cgshop2024_instance.json",
            "atris3249.cgshop2024_instance.json",
            "atris3323.cgshop2024_instance.json",
            "atris3934.cgshop2024_instance.json",
            "atris39445.cgshop2024_instance.json",
            "atris41643.cgshop2024_instance.json",
            "atris4630.cgshop2024_instance.json",
            "atris49252.cgshop2024_instance.json",
            "atris49462.cgshop2024_instance.json",
            "atris5288.cgshop2024_instance.json",
            "atris6238.cgshop2024_instance.json",
            "atris7260.cgshop2024_instance.json",
            "jigsaw_cf1_1ae14c9d_1379.cgshop2024_instance.json",
            "jigsaw_cf1_203072aa_32622.cgshop2024_instance.json",
            "jigsaw_cf1_26916b6b_6598.cgshop2024_instance.json",
            "jigsaw_cf1_4a53ec61_3293.cgshop2024_instance.json",
            "jigsaw_cf1_4fd4c46e_6548.cgshop2024_instance.json",
            "jigsaw_cf1_504b1421_159.cgshop2024_instance.json",
            "jigsaw_cf1_7b534d0f_30.cgshop2024_instance.json",
            "jigsaw_cf1_fa228ab6_337.cgshop2024_instance.json",
            "jigsaw_cf1_x151a4e0_343.cgshop2024_instance.json",
            "jigsaw_cf1_x47a0fe7_1363.cgshop2024_instance.json",
            "jigsaw_cf2_5db5d75a_34.cgshop2024_instance.json",
            "jigsaw_cf2_ebad6b02_335.cgshop2024_instance.json",
            "jigsaw_cf2_x1e8d2b3_39.cgshop2024_instance.json",
            "jigsaw_cf2_x338dc47_32816.cgshop2024_instance.json",
            "jigsaw_cf2_x38b54e9_31.cgshop2024_instance.json",
            "jigsaw_cf2_x53e7313_32896.cgshop2024_instance.json",
            "jigsaw_cf2_x70a1e53_1324.cgshop2024_instance.json",
            "jigsaw_cf2_xe148943_75.cgshop2024_instance.json",
            "jigsaw_cf2_xf42cb20_670.cgshop2024_instance.json",
            "jigsaw_cf3_12497ba7_32852.cgshop2024_instance.json",
            "jigsaw_cf3_x21b52c8_130.cgshop2024_instance.json",
            "jigsaw_cf3_xcd14250_28.cgshop2024_instance.json",
            "jigsaw_cf4_273db689_28.cgshop2024_instance.json",
            "jigsaw_cf4_x252babb_6578.cgshop2024_instance.json",
            "jigsaw_cf4_x412b2f8_32731.cgshop2024_instance.json",
            "jigsaw_cf4_x632ed77_3287.cgshop2024_instance.json",
            "jigsaw_rcf1_2197ecdd_6585.cgshop2024_instance.json",
            "jigsaw_rcf1_3335bb71_327.cgshop2024_instance.json",
            "jigsaw_rcf1_576b224d_3288.cgshop2024_instance.json",
            "jigsaw_rcf1_6d3a7c3b_1325.cgshop2024_instance.json",
            "jigsaw_rcf1_7d46e9d4_6582.cgshop2024_instance.json",
            "jigsaw_rcf1_e4f6ad12_331.cgshop2024_instance.json",
            "jigsaw_rcf1_x200a98a_3349.cgshop2024_instance.json",
            "jigsaw_rcf1_x251b042_3318.cgshop2024_instance.json",
            "jigsaw_rcf1_x25cf0d3_6582.cgshop2024_instance.json",
            "jigsaw_rcf1_x3d166d9_70.cgshop2024_instance.json",
            "jigsaw_rcf1_x73eed8f_663.cgshop2024_instance.json",
            "jigsaw_rcf1_x7c5577e_36.cgshop2024_instance.json",
            "jigsaw_rcf2_31a965a0_29.cgshop2024_instance.json",
            "jigsaw_rcf2_4646c11e_33039.cgshop2024_instance.json",
            "jigsaw_rcf2_534443c6_61.cgshop2024_instance.json",
            "jigsaw_rcf2_x401106d_683.cgshop2024_instance.json",
            "jigsaw_rcf2_x79af493_139.cgshop2024_instance.json",
            "jigsaw_rcf3_27e14196_6595.cgshop2024_instance.json",
            "jigsaw_rcf3_5a515084_1325.cgshop2024_instance.json",
            "jigsaw_rcf3_x1142c4a_3315.cgshop2024_instance.json",
            "jigsaw_rcf3_x1944562_3300.cgshop2024_instance.json",
            "jigsaw_rcf3_x438c9fe_328.cgshop2024_instance.json",
            "jigsaw_rcf3_x699083b_71.cgshop2024_instance.json",
            "jigsaw_rcf4_37b8d52e_1338.cgshop2024_instance.json",
            "jigsaw_rcf4_45bf920f_31.cgshop2024_instance.json",
            "jigsaw_rcf4_5b859f68_336.cgshop2024_instance.json",
            "jigsaw_rcf4_6de1b3b7_1363.cgshop2024_instance.json",
            "jigsaw_rcf4_7702a097_70.cgshop2024_instance.json",
            "jigsaw_rcf4_944ca2fb_34.cgshop2024_instance.json",
            "jigsaw_rcf4_x11655bb_6573.cgshop2024_instance.json",
            "jigsaw_rcf4_x296c58c_70.cgshop2024_instance.json",
            "jigsaw_rcf4_x4ee7385_1339.cgshop2024_instance.json",
            "jigsaw_rcf4_x6f71c05_3333.cgshop2024_instance.json",
            "jigsaw_rcf4_xb9f4bdd_33015.cgshop2024_instance.json",
            "random_cf1_1943fa88_50000.cgshop2024_instance.json",
            "random_cf1_27383a82_5000.cgshop2024_instance.json",
            "random_cf1_327d5170_10000.cgshop2024_instance.json",
            "random_cf1_54081766_500.cgshop2024_instance.json",
            "random_cf1_64ac4991_50.cgshop2024_instance.json",
            "random_cf1_6de164e1_200.cgshop2024_instance.json",
            "random_cf1_720892de_5000.cgshop2024_instance.json",
            "random_cf1_x15dff42_50.cgshop2024_instance.json",
            "random_cf1_x1869fdd_100.cgshop2024_instance.json",
            "random_cf1_x32f7db7_200.cgshop2024_instance.json",
            "random_cf1_x51ab828_2000.cgshop2024_instance.json",
            "random_cf1_x53b606b_1000.cgshop2024_instance.json",
            "random_cf1_x5c10455_10000.cgshop2024_instance.json",
            "random_cf1_x665a736_100.cgshop2024_instance.json",
            "random_cf1_x6c375be_50000.cgshop2024_instance.json",
            "random_cf1_x6dad719_500.cgshop2024_instance.json",
            "random_cf2_6d9eb964_10000.cgshop2024_instance.json",
            "random_cf2_74ae2d30_50.cgshop2024_instance.json",
            "random_cf2_x55f520f_50000.cgshop2024_instance.json",
            "random_cf2_x7e61ca2_5000.cgshop2024_instance.json",
            "random_cf3_1b0922a9_2000.cgshop2024_instance.json",
            "random_cf3_1c1ba767_50.cgshop2024_instance.json",
            "random_cf3_1ec21f4b_100.cgshop2024_instance.json",
            "random_cf3_327b5d4c_5000.cgshop2024_instance.json",
            "random_cf3_512f73fe_500.cgshop2024_instance.json",
            "random_cf3_6b0e5702_50000.cgshop2024_instance.json",
            "random_cf3_x21f5def_200.cgshop2024_instance.json",
            "random_cf3_x4b49fe2_10000.cgshop2024_instance.json",
            "random_cf3_x570e994_10000.cgshop2024_instance.json",
            "random_cf3_x6a9d8c0_2000.cgshop2024_instance.json",
            "random_cf4_2fb3bbda_10000.cgshop2024_instance.json",
            "random_cf4_50e0d4d9_100.cgshop2024_instance.json",
            "random_cf4_5f36961f_500.cgshop2024_instance.json",
            "random_cf4_x414b785_100.cgshop2024_instance.json",
            "random_cf4_x72dca06_50.cgshop2024_instance.json",
            "random_rcf1_340f4443_500.cgshop2024_instance.json",
            "random_rcf1_477eb796_50.cgshop2024_instance.json",
            "random_rcf1_5005b6d4_100.cgshop2024_instance.json",
            "random_rcf1_70921d4c_50000.cgshop2024_instance.json",
            "random_rcf1_78877e61_200.cgshop2024_instance.json",
            "random_rcf1_x1c1c14b_500.cgshop2024_instance.json",
            "random_rcf1_x57ba9ab_1000.cgshop2024_instance.json",
            "random_rcf1_x6e7ed44_5000.cgshop2024_instance.json",
            "random_rcf1_x7dc2687_200.cgshop2024_instance.json",
            "random_rcf1_x7effebe_1000.cgshop2024_instance.json",
            "random_rcf1_x831dfdb_10000.cgshop2024_instance.json",
            "random_rcf2_5c2f5d8b_200.cgshop2024_instance.json",
            "random_rcf2_x25afb10_2000.cgshop2024_instance.json",
            "random_rcf2_x2871624_50.cgshop2024_instance.json",
            "random_rcf2_x4022b68_500.cgshop2024_instance.json",
            "random_rcf2_x4db924e_5000.cgshop2024_instance.json",
            "random_rcf2_x58d9352_100.cgshop2024_instance.json",
            "random_rcf3_75bcfc60_10000.cgshop2024_instance.json",
            "random_rcf3_x3d52d59_2000.cgshop2024_instance.json",
            "random_rcf3_x7651267_1000.cgshop2024_instance.json",
            "random_rcf4_32e6daf1_10000.cgshop2024_instance.json",
            "random_rcf4_69d930c3_500.cgshop2024_instance.json",
            "random_rcf4_6e323d40_100.cgshop2024_instance.json",
            "random_rcf4_7cbb0868_50000.cgshop2024_instance.json",
            "random_rcf4_x540254f_50.cgshop2024_instance.json",
            "satris13382.cgshop2024_instance.json",
            "satris14660.cgshop2024_instance.json",
            "satris15666.cgshop2024_instance.json",
            "satris16074.cgshop2024_instance.json",
            "satris16183.cgshop2024_instance.json",
            "satris1685.cgshop2024_instance.json",
            "satris16903.cgshop2024_instance.json",
            "satris1786.cgshop2024_instance.json",
            "satris1811.cgshop2024_instance.json",
            "satris1959.cgshop2024_instance.json",
            "satris21978.cgshop2024_instance.json",
            "satris2205.cgshop2024_instance.json",
            "satris24015.cgshop2024_instance.json",
            "satris2885.cgshop2024_instance.json",
            "satris3041.cgshop2024_instance.json",
            "satris3079.cgshop2024_instance.json",
            "satris3576.cgshop2024_instance.json",
            "satris3582.cgshop2024_instance.json",
            "satris37349.cgshop2024_instance.json",
            "satris3754.cgshop2024_instance.json",
            "satris3794.cgshop2024_instance.json",
            "satris38870.cgshop2024_instance.json",
            "satris4555.cgshop2024_instance.json",
            "satris4681.cgshop2024_instance.json",
            "satris48291.cgshop2024_instance.json",
            "satris6466.cgshop2024_instance.json",
            "satris7039.cgshop2024_instance.json",
            "satris7107.cgshop2024_instance.json",
            "satris7684.cgshop2024_instance.json",
            "satris8180.cgshop2024_instance.json",
            "satris9986.cgshop2024_instance.json",
        ],
    }

    for instance_dir, instance_names in instance_names.items():
        for instance_name in instance_names:
            path = os.path.join("cgshop2024", instance_dir, instance_name)
            with open("data/irregular_raw/" + path, "r") as f:
                data = json.load(f)
                dic = {
                    "objective": "knapsack",
                    "bin_types": [
                        {
                            "type": "polygon",
                            "vertices": [],
                        }
                    ],
                    "item_types": [],
                }
                for x, y in zip(data["container"]["x"], data["container"]["y"]):
                    dic["bin_types"][0]["vertices"].append({"x": x, "y": y})
                for item in data["items"]:
                    dic["item_types"].append(
                        {
                            "type": "polygon",
                            "profit": item["value"],
                            "copies": item["quantity"],
                            "vertices": [],
                        }
                    )
                    for x, y in zip(item["x"], item["y"]):
                        dic["item_types"][-1]["vertices"].append({"x": x, "y": y})
                write_dict(dic, path)


def convert_oliveira2000(filename):
    path = os.path.join("data", "irregular_raw", filename)

    dic = {
        "objective": "open-dimension-x",
        "bin_types": [],
        "item_types": [],
    }

    tree = ET.parse(path)
    root = tree.getroot()

    ns = '{http://www.fe.up.pt/~esicup/nesting.xsd}'

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
            allowed_rotations.append({"start": angle, "end": angle})
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


if __name__ == "__main__":

    convert_packomania_coop()
    convert_cgshop2024()

    convert_oliveira2000(os.path.join("oliveira2000", "blaz_2007-04-23", "blaz.xml"))
    convert_oliveira2000(os.path.join("oliveira2000", "shapes_2007-04-23", "shapes0.xml"))
    convert_oliveira2000(os.path.join("oliveira2000", "shapes_2007-04-23", "shapes1.xml"))
    convert_oliveira2000(os.path.join("oliveira2000", "shirts_2007-05-15", "shirts.xml"))
    convert_oliveira2000(os.path.join("oliveira2000", "swim_2007-05-15", "swim.xml"))
    convert_oliveira2000(os.path.join("oliveira2000", "trousers_2007-05-15", "trousers.xml"))

import sys
import os
import os.path
import json

###############################################################################

datas_rectangle = {}

datas_rectangle["roadef2018_A"] = [
        "roadef2018/A" + str(i)
        for i in range(1, 21)]
datas_rectangle["roadef2018_B"] = [
        "roadef2018/B" + str(i)
        for i in range(1, 16)]
datas_rectangle["roadef2018_X"] = [
        "roadef2018/X" + str(i)
        for i in range(1, 16)]

datas_rectangle["christofides1977"] = [
        "christofides1977/cgcut" + str(i) + ".txt"
        for i in range(1, 4)]

datas_rectangle["beng1982"] = [
        "beng1982/BENG" + str(i)
        for i in range(1, 11)]

datas_rectangle["wang1983"] = [
        "wang1983/" + i
        for i in ["WANG1", "WANG2", "WANG3", "W"]]
datas_rectangle["wang1983_vbpp"] = [
        "wang1983/" + i
        for i in ["WANGM1", "WANGM2"]]

datas_rectangle["beasley1985"] = [
        "beasley1985/gcut" + str(i) + ".txt"
        for i in range(1, 14)]

datas_rectangle["berkey1987"] = [
        "berkey1987/Class_" + "{:02d}".format(c) + ".2bp_"
        + str(n) + "_" + str(i)
        for c in range(1, 7)
        for n in [20, 40, 60, 80, 100]
        for i in range(1, 11)]

datas_rectangle["oliveira1990"] = [
        "oliveira1990/" + i
        for i in ["OF1", "OF2"]]

datas_rectangle["morabito1992"] = [
        "morabito1992/M" + str(i)
        for i in range(1, 6)]

datas_rectangle["tschoke1995_cw"] = [
        "tschoke1995/" + i
        for i in ["STS2", "STS4"]]
datas_rectangle["tschoke1995_cu"] = [
        "tschoke1995/" + i
        for i in ["STS2s", "STS4s"]]

datas_rectangle["hadjiconstantinou1995"] = [
        "hadjiconstantinou1995/" + i
        for i in ["HADCHR3", "HADCHR11"]]

datas_rectangle["kroger1995"] = [
        "kroger1995/KR-" + "{:02d}".format(i) + ".txt"
        for i in range(1, 13)]

datas_rectangle["jakobs1996"] = [
        "jakobs1996/" + i
        for i in ["j1", "j2",
                  "JAKOBS1", "JAKOBS2", "JAKOBS3", "JAKOBS4", "JAKOBS5"]]

datas_rectangle["fayard1996"] = ["fayard1996/HZ1"]

datas_rectangle["fekete1997"] = [
        "fekete1997/okp" + str(i)
        for i in range(1, 6)]

datas_rectangle["lai1997"] = [
        "lai1997/" + i
        for i in ["1", "2", "3"]]

datas_rectangle["hifi1997a_cw"] = [
        "hifi1997a/" + i
        for i in ["2", "3", "A1", "A2"]]
datas_rectangle["hifi1997a_cu"] = [
        "hifi1997a/" + i
        for i in ["2s", "3s", "A1s", "A2s", "A3", "A4", "A5", "HH"]]

datas_rectangle["hifi1998"] = [
        "hifi1998/SCP" + str(i)
        for i in range(1, 26)]

datas_rectangle["fayard1998_cw"] = [
        "fayard1998/CW" + str(i)
        for i in range(1, 12)]
datas_rectangle["fayard1998_cu"] = [
        "fayard1998/CU" + str(i)
        for i in range(1, 12)]
datas_rectangle["fayard1998_uw"] = [
        "fayard1998/UW" + str(i)
        for i in range(1, 12)]
datas_rectangle["fayard1998_uu"] = [
        "fayard1998/UU" + str(i)
        for i in range(1, 12)]

datas_rectangle["martello1998"] = [
        "martello1998/Class_" + "{:02d}".format(c) + ".2bp_"
        + str(n) + "_" + str(i)
        for c in range(7, 11)
        for n in [20, 40, 60, 80, 100]
        for i in range(1, 11)]

datas_rectangle["hopper2000_n"] = [
        "hopper2000/n" + str(b) + c
        for b in range(1, 8)
        for c in ["a", "b", "c", "d", "e"]]
datas_rectangle["hopper2000_t"] = [
        "hopper2000/t" + str(b) + c
        for b in range(1, 8)
        for c in ["a", "b", "c", "d", "e"]]

datas_rectangle["cung2000_cw"] = [
        "cung2000/" + i
        for i in ["CHL1", "CHL2", "CHL3", "CHL4", "Hchl1", "Hchl2", "Hchl9"]]
datas_rectangle["cung2000_cu"] = [
        "cung2000/" + i
        for i in ["CHL1s", "CHL2s", "CHL3s", "CHL4s", "CHL5", "CHL6", "CHL7",
                  "Hchl3s", "Hchl4s", "Hchl5s", "Hchl6s", "Hchl7s", "Hchl8s"]]

datas_rectangle["hifi2001_cu"] = [
        "hifi2001/" + i
        for i in ["U4", "LU1", "LU2", "LU3", "LU4"]]
datas_rectangle["hifi2001_cw"] = [
        "hifi2001/" + i
        for i in ["W4",
                  "MW1", "MW2", "MW3", "MW4", "MW5",
                  "LW1", "LW2", "LW3", "LW4"]]

datas_rectangle["hopper2001a"] = [
        "hopper2001a/C" + str(i) + "_" + str(j)
        for i in range(1, 8)
        for j in range(1, 4)]
datas_rectangle["hopper2001b"] = [
        "hopper2001b/M" + str(i) + a
        for i in range(1, 4)
        for a in ['a', 'b', 'c', 'd', 'e']]

datas_rectangle["alvarez2002_uu"] = [
        "alvarez2002/ATP1" + str(i)
        for i in range(0, 10)]
datas_rectangle["alvarez2002_uw"] = [
        "alvarez2002/ATP2" + str(i)
        for i in range(0, 10)]
datas_rectangle["alvarez2002_cu"] = [
        "alvarez2002/ATP3" + str(i)
        for i in range(0, 10)]
datas_rectangle["alvarez2002_cw"] = [
        "alvarez2002/ATP4" + str(i)
        for i in range(0, 10)]

datas_rectangle["leung2003"] = [
        "leung2003/" + i
        for i in ["P9", "P10"]]

datas_rectangle["beasley2004_ngcutap"] = [
        "beasley2004/ngcutap.txt_" + str(i)
        for i in range(1, 22)]
# datas_rectangle["beasley2004_ngcutcon"] = [
#         "beasley2004/ngcutcon.txt_" + str(i)
#         for i in range(1, 22)]
datas_rectangle["beasley2004_ngcutfs"] = [
        "beasley2004/ngcutfs" + str(i) + ".txt_" + str(j)
        for i in range(1, 4)
        for j in range(1, 211)]

datas_rectangle["burke2004"] = [
        "burke2004/BKW" + str(i)
        for i in range(1, 14)]

datas_rectangle["imahori2005"] = [
        "imahori2005/" + a + b + c
        for a in ["A", "B", "C", "D"]
        for b in ["L", "S", "V"]
        for c in ["X", "Y", "Z", "ZZ", "ZZZ"]]

datas_rectangle["pisinger2005"] = [
        "pisinger2005/MB_C" + str(i) + "_" + str(j)
        for i in range(1, 11)
        for j in range(1, 51)]

datas_rectangle["pinto2005"] = [
        "pinto2005/" + str(i)
        for i in [50, 100, 500, 1000, 5000, 10000, 15000]]

datas_rectangle["bortfeldt2006"] = [
        "bortfeldt2006/AH" + str(i)
        for i in range(1, 361)]

datas_rectangle["hifi2008_cu"] = [
        "hifi2008/" + t + "T_" + i + "H.txt"
        for t in ["nice", "path"]
        for i in ["25", "50", "100", "200", "500", "1000"]]
datas_rectangle["hifi2008_cw"] = [
        "hifi2008/" + t + "T_" + i + "PH.txt"
        for t in ["nice", "path"]
        for i in ["25", "50", "100", "200", "500"]]

datas_rectangle["cui2008"] = [
        "cui2008/" + str(i)
        for i in range(1, 21)]

datas_rectangle["cintra2008_bpp"] = [
        "cintra2008/gcut" + str(i) + "d.txt_1bintype"
        for i in range(1, 13)]
datas_rectangle["cintra2008_vbpp"] = [
        "cintra2008/gcut" + str(i) + "d.txt"
        for i in range(1, 13)]

datas_rectangle["egeblad2009"] = [
        "egeblad2009/ep-" + i + "-" + a + "-" + b + "-" + j + ".2kp"
        for i in ["30", "50", "100", "200"]
        for a in ["D", "S", "T", "U", "W"]
        for b in ["C", "R"]
        for j in ["25", "75"]]

datas_rectangle["imahori2010"] = [
        "imahori2010/i" + str(i) + "-" + str(j)
        for i in range(4, 21)
        for j in range(1, 11)]

datas_rectangle["morabito2010"] = [
        "morabito2010/random_class_" + str(c) + "/R_" + str(n) + "_" + t1
        + "/" + str(i) + "_" + str(n) + "_100_" + t2 + ".dat"
        for c in [1, 2, 3]
        for t1, t2 in [("S", "10_50"), ("L", "25_75")]
        for n in [10, 20, 30, 40, 50]
        for i in range(1, 16)]

datas_rectangle["ortmann2010"] = [
        "ortmann2010/" + t + str(n) + "i" + str(m) + "b" + str(i)
        for t in ["Nice", "Path"]
        for n in [25, 50, 100, 200, 300, 400, 500]
        for m in [2, 3, 4, 5, 6]
        for i in range(1, 6)
        if n != 25 or m != 6]

datas_rectangle["leung2011"] = [
        "leung2011/zdf" + str(i)
        for i in range(1, 17)]

datas_rectangle["cui2012"] = [
        "cui2012/" + str(i) + c + "-" + str(j)
        for c in ["A", "B"]
        for i in range(1, 4)
        for j in range(1, 11)]

datas_rectangle["hifi2012_cu"] = [
        "hifi2012/UL" + i + "H.txt"
        for i in ["1", "2", "3"]]
datas_rectangle["hifi2012_cw"] = [
        "hifi2012/WL" + i + "H.txt"
        for i in ["1", "2", "3"]]

datas_rectangle["silveira2013"] = [
        "silveira2013/2lcvrp/mod_2l_cvrp" + "{:02d}".format(i)
        + "{:02d}".format(j) + ".txt"
        for i in range(1, 37)
        for j in range(1, 6)] + [
        "silveira2013/bea/T" + str(k) + "/GCUT" + "{:02d}".format(i) + ".TXT"
        for k in [20, 40, 60, 80, 100]
        for i in range(1, 14)] + [
        "silveira2013/bea/T" + str(k) + "/NGCUT" + "{:02d}".format(i) + ".TXT"
        for k in [20, 40, 60, 80, 100]
        for i in range(1, 13)] + [
        "silveira2013/ben/T" + str(k) + "/BENG" + "{:02d}".format(i) + ".TXT"
        for k in [20, 40, 60, 80, 100]
        for i in range(1, 11)] + [
        "silveira2013/bke/T" + str(k) + "/N" + str(i) + "Burke.txt"
        for k in [20, 40, 60, 80, 100]
        for i in range(1, 13)] + [
        "silveira2013/chr/T" + str(k) + "/CGCUT" + "{:02d}".format(i) + ".TXT"
        for k in [20, 40, 60, 80, 100]
        for i in range(1, 4)] + [
        "silveira2013/hop/T" + str(k) + "/Hopper" + a + str(b) + c + ".txt"
        for k in [20, 40, 60, 80, 100]
        for a in ["N", "T"]
        for b in range(1, 8)
        for c in ["a", "b", "c", "d", "e"]] + [
        "silveira2013/htu/T" + str(k) + "/c" + str(i) + "-p" + str(j)
        + "(Hopper).txt"
        for k in [20, 40, 60, 80, 100]
        for i in range(1, 8)
        for j in range(1, 4)]

datas_rectangle["afsharian2014"] = [
        "afsharian2014/" + i + "M" + str(m) + "R" + str(r) + "N" + str(n)
        + "_D" + str(d)
        for i in ["75-75.txt/C12", "112-50.txt/C13", "150-150.txt/C11",
                  "225-100.txt/C12", "300-300.txt/C21", "450-200.txt/C22"]
        for m in [5, 10, 15, 20, 25]
        for r in [6, 8, 10]
        for n in range(1, 16)
        for d in range(5)]

datas_rectangle["clautiaux2018_cu"] = []
datas_rectangle["clautiaux2018_cw"] = []
for wh in ["W500H1000", "W1000H2000"]:
    for n in [50, 100, 150]:
        datas_rectangle["clautiaux2018_cu"] += [
                "clautiaux2018/a/" + f.strip()
                for f in open("data/rectangle_raw/clautiaux2018/A_"
                              + wh + "I" + str(n))]
        datas_rectangle["clautiaux2018_cw"] += [
                "clautiaux2018/p/" + f.strip()
                for f in open("data/rectangle_raw/clautiaux2018/P_"
                              + wh + "I" + str(n))]

# TODO clautiaux2019

# datas_rectangle["martin2019a"] = [
#         "martin2019a/os" + o + "_is" + i + "_m" + m + "_" + j
#         for o in ["02", "06"]
#         for i in ["01", "02", "03", "04", "06", "07", "08", "11", "12", "16"]
#         for m in ["10", "20", "40"]
#         for j in ["01", "02", "03", "04", "05"]]
# datas_rectangle["martin2019b"] = [
#         "martin2019b/inst_" + LW + "_" + str(m) + "_" + str(rho)
#         + "_" + str(i) + "_" + str(d)
#         for LW in ["75_75", "125_50", "150_150",
#                    "225_100", "300_300", "450_200"]
#         for m in [5, 10, 15, 20, 25]
#         for rho in [6, 8, 10] for i in range(1, 16)
#         for d in [1, 2, 3, 4]]

datas_rectangle["velasco2019"] = [
        "velasco2019/P" + str(cl) + "_" + str(l) + "_" + str(h)
        + "_" + str(m) + "_" + str(i) + ".txt"
        for cl, l, h in [(1, 100, 200), (1, 100, 400), (2, 200, 100),
                         (2, 400, 100), (3, 150, 150), (3, 250, 250),
                         (4, 150, 150), (4, 250, 250)]
        for m in [25, 50]
        for i in range(1, 6)]

datas_rectangle["goncalves2020"] = [
        "afsharian2014/" + i + "M" + str(m) + "R" + str(r) + "N15_D4"
        for i in ["75-75.txt/C12", "112-50.txt/C13", "150-150.txt/C11",
                  "225-100.txt/C12", "300-300.txt/C21", "450-200.txt/C22"]
        for r in [6, 8, 10]
        for m in [5, 10, 15, 20, 25]]

datas_rectangle["long2020"] = [
        "long2020/Instance_" + str(i) + ".txt"
        for i in range(1, 26)]

###############################################################################


def get_tests(problem):

    #############
    # Rectangle #
    #############

    # BPP
    if problem == "BPP-O":
        # lodi1999 boschetti2003 faroe2003 monaci2006
        # parreno2010 zhu2012 wei2013
        pass
    if problem == "BPP-R":
        # lodi1999 boschetti2003 harwig2006 hayek2008 cui2015 cui2018
        pass

    # KP
    if problem == "KP-O":
        # beasley2004 alvarez2005 alvarez2007 goncalves2007
        # hadjiconstantinou2007 huang2007 bortfeldt2009 egeblad2009 he2012
        # leung2012
        pass
    if problem == "KP-R":
        # hopper2001 wu2002 bortfeldt2009 egeblad2009 he2012
        pass

    # SPP
    if problem == "SPP-O":
        # bortfeldt2006 zhang2006 alvarez2008 belov2008 leung2011b wei2011
        # zhang2012 yang2013 wei2016 zhang2016 wei2017
        pass
    if problem == "SPP-R":
        # bortfeldt2006 zhang2007 burke2009 leung2011a wei2011 zhang2012 he2013
        pass

    # VBPP
    if problem == "VBPP-O":
        #
        pass

    # UKP
    if problem == "UKP-O":
        # birgin2012 wei2018
        pass
    if problem == "UKP-R":
        # wei2018
        pass
    if problem == "UKP-O-D":
        # goncalves2020
        pass

    ########################
    # Rectangle guillotine #
    ########################

    # BPPL

    if problem == "roadef2018_A":
        # parreno2020
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective bin-packing-with-leftovers"
                 " --time-limit 3600"
                 " -q \"RG -p roadef2018 -c 0\" -a \"IBS -f 1.33\""
                 " -q \"RG -p roadef2018 -c 1\" -a \"IBS -f 1.33\""
                 " -q \"RG -p roadef2018 -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p roadef2018 -c 1\" -a \"IBS -f 1.5\""
                 ) for f in datas_rectangle["roadef2018_A"]]
    if problem == "roadef2018_B":
        # parreno2020
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective bin-packing-with-leftovers"
                 " --time-limit 3600"
                 " -q \"RG -p roadef2018 -c 0\" -a \"IBS -f 1.33\""
                 " -q \"RG -p roadef2018 -c 1\" -a \"IBS -f 1.33\""
                 " -q \"RG -p roadef2018 -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p roadef2018 -c 1\" -a \"IBS -f 1.5\""
                 ) for f in datas_rectangle["roadef2018_B"]]
    if problem == "roadef2018_X":
        # parreno2020
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective bin-packing-with-leftovers"
                 " --time-limit 3600"
                 " -q \"RG -p roadef2018 -c 0\" -a \"IBS -f 1.33\""
                 " -q \"RG -p roadef2018 -c 1\" -a \"IBS -f 1.33\""
                 " -q \"RG -p roadef2018 -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p roadef2018 -c 1\" -a \"IBS -f 1.5\""
                 ) for f in datas_rectangle["roadef2018_X"]]

    # BPP

    elif problem == "3NEGH-BPP-O":
        # alvelos2009
        # [G-BPP-O] charalambous2011 fleszar2013 hong2014 lodi2017 cui2018
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 3NHO -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NHO -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NHO -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["christofides1977"]
                + datas_rectangle["beasley1985"]
                + datas_rectangle["beasley2004_ngcutap"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "3NEGH-CSP-O":
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 3NHO -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3NHO -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["christofides1977"]
                + datas_rectangle["beasley1985"]
                + datas_rectangle["beasley2004_ngcutap"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "3NEGH-BPP-R":
        # [G-BPP-R] charalambous2011 fleszar2013 cui2015 cui2018
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 3NHR -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NHR -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NHR -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "3NEGH-CSP-R":
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 3NHR -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3NHR -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "long2020_BPP":
        # long2020
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective bin-packing"
                 " --time-limit 10"
                 " -q \"RG -p 3NVO --one2cut -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NVO --one2cut -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NVO --one2cut -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["long2020"]
                ]
    elif problem == "long2020_CSP":
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 15"
                 " -q \"RG -p 3NVO --one2cut -c 4\""
                 " -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3NVO --one2cut -c 5\""
                 " -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["long2020"]
                ]
    elif problem == "3GH-BPP-O":
        # puchinger2007 alvelos2009
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 3EHO -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3EHO -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3EHO -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["christofides1977"]
                + datas_rectangle["beasley1985"]
                + datas_rectangle["beasley2004_ngcutap"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "3GH-CSP-O":
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 3EHO -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3EHO -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["christofides1977"]
                + datas_rectangle["beasley1985"]
                + datas_rectangle["beasley2004_ngcutap"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "3HG-BPP-O":
        # chen2016
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 3HAO -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3HAO -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3HAO -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["imahori2005"]
                + datas_rectangle["cintra2008_bpp"]
                ]
    elif problem == "3HG-CSP-O":
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 3HVO -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3HVO -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3HHO -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3HHO -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["imahori2005"]
                + datas_rectangle["cintra2008_bpp"]
                ]
    elif problem == "3HGV-BPP-O":
        # chen2016
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective bin-packing"
                 " --time-limit 5"
                 " -q \"RG -p 3HVO -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3HVO -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3HVO -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["wang1983"]
                + datas_rectangle["oliveira1990"]
                + datas_rectangle["tschoke1995_cw"]
                + datas_rectangle["tschoke1995_cu"]
                + datas_rectangle["fekete1997"]
                + datas_rectangle["hifi1997a_cw"]
                + datas_rectangle["hifi1997a_cu"]
                + datas_rectangle["fayard1998_cw"]
                + datas_rectangle["fayard1998_cu"]
                + datas_rectangle["cung2000_cw"]
                + datas_rectangle["cung2000_cu"]
                + datas_rectangle["alvarez2002_cu"]
                + datas_rectangle["alvarez2002_cw"]
                ]
    elif problem == "3HGV-CSP-O":
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 30"
                 " -q \"RG -p 3HVO -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3HVO -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["wang1983"]
                + datas_rectangle["oliveira1990"]
                + datas_rectangle["tschoke1995_cw"]
                + datas_rectangle["tschoke1995_cu"]
                + datas_rectangle["fekete1997"]
                + datas_rectangle["hifi1997a_cw"]
                + datas_rectangle["hifi1997a_cu"]
                + datas_rectangle["fayard1998_cw"]
                + datas_rectangle["fayard1998_cu"]
                + datas_rectangle["cung2000_cw"]
                + datas_rectangle["cung2000_cu"]
                + datas_rectangle["alvarez2002_cu"]
                + datas_rectangle["alvarez2002_cw"]
                ]

    elif problem == "2NEGH-BPP-O":
        # cintra2008 alvelos2009 cui2013 alvelos2014
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective bin-packing"
                 " --time-limit " + str(t) +
                 " -q \"RG -p 2NHO -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NHO -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NHO -c 3\" -a \"IBS -f 1.5\""
                 ) for f, t in []
                + [(ff, 10) for ff in []
                   + datas_rectangle["christofides1977"]
                   + datas_rectangle["wang1983"]
                   + datas_rectangle["beasley1985"]
                   + datas_rectangle["oliveira1990"]
                   + datas_rectangle["tschoke1995_cw"]
                   + datas_rectangle["tschoke1995_cu"]
                   + datas_rectangle["fekete1997"]
                   + datas_rectangle["hifi1997a_cw"]
                   + datas_rectangle["hifi1997a_cu"]
                   + datas_rectangle["fayard1998_cw"]
                   + datas_rectangle["fayard1998_cu"]
                   + datas_rectangle["cung2000_cw"]
                   + datas_rectangle["cung2000_cu"]
                   + datas_rectangle["beasley2004_ngcutap"]
                   + datas_rectangle["alvarez2002_cu"]
                   + datas_rectangle["alvarez2002_cw"]
                   ]
                + [(ff, 60) for ff in []
                   + datas_rectangle["cintra2008_bpp"]
                   + datas_rectangle["berkey1987"]
                   + datas_rectangle["martello1998"]
                   ]
                ]
    elif problem == "2NEGH-CSP-O":
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 2NHO -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 2NHO -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["christofides1977"]
                + datas_rectangle["wang1983"]
                + datas_rectangle["beasley1985"]
                + datas_rectangle["oliveira1990"]
                + datas_rectangle["tschoke1995_cw"]
                + datas_rectangle["tschoke1995_cu"]
                + datas_rectangle["fekete1997"]
                + datas_rectangle["hifi1997a_cw"]
                + datas_rectangle["hifi1997a_cu"]
                + datas_rectangle["fayard1998_cw"]
                + datas_rectangle["fayard1998_cu"]
                + datas_rectangle["cung2000_cw"]
                + datas_rectangle["cung2000_cu"]
                + datas_rectangle["beasley2004_ngcutap"]
                + datas_rectangle["alvarez2002_cu"]
                + datas_rectangle["alvarez2002_cw"]
                + datas_rectangle["cintra2008_bpp"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "2NEGH-BPP-R":
        # cintra2008 cui2013 cui2016
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 2NHR -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NHR -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NHR -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["cintra2008_bpp"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "2NEGH-CSP-R":
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 2NHR -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 2NHR -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["cintra2008_bpp"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "2GH-BPP-O":  # alvelos2009 cui2013
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 2EHO -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2EHO -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2EHO -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["christofides1977"]
                + datas_rectangle["beasley1985"]
                + datas_rectangle["beasley2004_ngcutap"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "2GH-CSP-O":
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 2EHO -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 2EHO -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["christofides1977"]
                + datas_rectangle["beasley1985"]
                + datas_rectangle["beasley2004_ngcutap"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]

    # KP

    elif problem == "3NEG-KP-O":
        # [G-KP-O] fayard1998 alvarez2002 chen2007
        #          bortfeldt2009 morabito2010 dolatabadi2012
        #          wei2015 (furini2016) velasco2019 (martin2019) (martin2020)
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 " --time-limit " + str(t) +
                 " -q \"RG -p 3NHO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NVO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NHO -c 5\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NVO -c 5\" -a \"IBS -f 1.5\""
                 ) for f, t in []
                + [(ff, 10) for ff in []
                   + datas_rectangle["christofides1977"]
                   + datas_rectangle["wang1983"]
                   + datas_rectangle["beasley1985"]
                   + datas_rectangle["oliveira1990"]
                   + datas_rectangle["tschoke1995_cw"]
                   + datas_rectangle["tschoke1995_cu"]
                   + datas_rectangle["hadjiconstantinou1995"]
                   + datas_rectangle["jakobs1996"]
                   + datas_rectangle["fekete1997"]
                   + datas_rectangle["hifi1997a_cw"]
                   + datas_rectangle["hifi1997a_cu"]
                   + datas_rectangle["lai1997"]
                   + datas_rectangle["fayard1998_cw"]
                   + datas_rectangle["fayard1998_cu"]
                   + datas_rectangle["cung2000_cw"]
                   + datas_rectangle["cung2000_cu"]
                   + datas_rectangle["hopper2001a"]
                   ]
                + [(ff, 60) for ff in []
                   + datas_rectangle["alvarez2002_cu"]
                   + datas_rectangle["alvarez2002_cw"]
                   ]
                + [(ff, 10) for ff in []
                   + datas_rectangle["leung2003"]
                   + datas_rectangle["beasley2004_ngcutap"]
                   + datas_rectangle["morabito2010"]
                   ]
                + [(ff, 120) for ff in []
                   + datas_rectangle["velasco2019"]
                   ]
                ]
    elif problem == "3NEG-KP-R":
        # [G-KP-R] bortfeldt2009 wei2015 velasco2019
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 " --time-limit " + str(t) +
                 " -q \"RG -p 3NHR -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NVR -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NHR -c 5\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NVR -c 5\" -a \"IBS -f 1.5\""
                 ) for f, t in []
                + [(ff, 30) for ff in []
                   + datas_rectangle["christofides1977"]
                   + datas_rectangle["wang1983"]
                   + datas_rectangle["hadjiconstantinou1995"]
                   + datas_rectangle["jakobs1996"]
                   + datas_rectangle["fekete1997"]
                   + datas_rectangle["lai1997"]
                   + datas_rectangle["fayard1998_cw"]
                   + datas_rectangle["fayard1998_cu"]
                   + datas_rectangle["hopper2001a"]
                   + datas_rectangle["leung2003"]
                   + datas_rectangle["beasley2004_ngcutap"]
                   ]
                + [(ff, 120) for ff in []
                   + datas_rectangle["velasco2019"]
                   ]
                ]
    elif problem == "3NEGV-KP-O":
        # cui2015
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 " --time-limit 60"
                 " -q \"RG -p 3NVO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NVO -c 5\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["alvarez2002_cu"]
                + datas_rectangle["alvarez2002_cw"]
                + datas_rectangle["cui2012"]
                ]
    elif problem == "3HG-KP-O":
        # cui2008
        return [(f + ("_unweighted" if unweighted else ""),
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 + (" --unweighted" if unweighted else "") +
                 " --time-limit " + str(t) +
                 " -q \"RG -p 3HHO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3HVO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3HHO -c 5\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3HVO -c 5\" -a \"IBS -f 1.5\""
                 ) for f, t, unweighted in []
                + [(ff, 2, False) for ff in []
                   + datas_rectangle["wang1983"]
                   + datas_rectangle["oliveira1990"]
                   + datas_rectangle["tschoke1995_cw"]
                   + datas_rectangle["tschoke1995_cu"]
                   + datas_rectangle["hifi1997a_cw"]
                   + datas_rectangle["hifi1997a_cu"]
                   + datas_rectangle["fayard1998_cw"]
                   + datas_rectangle["fayard1998_cu"]
                   + datas_rectangle["cung2000_cw"]
                   + datas_rectangle["cung2000_cu"]
                   ]
                + [(ff, 180, False) for ff in []
                   + datas_rectangle["cui2008"]
                   ]
                + [(ff, 180, True) for ff in []
                   + datas_rectangle["cui2008"]
                   ]
                ]

    elif problem == "2NEG-KP-O":
        # hifi2001
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 " --time-limit " + str(t) +
                 " -q \"RG -p 2NAO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NAO -c 5\" -a \"IBS -f 1.5\""
                 ) for f, t in []
                + [(ff, 1) for ff in []
                   + datas_rectangle["christofides1977"]
                   + datas_rectangle["wang1983"]
                   + datas_rectangle["oliveira1990"]
                   + datas_rectangle["beasley1985"]
                   + datas_rectangle["tschoke1995_cw"]
                   + datas_rectangle["tschoke1995_cu"]
                   + datas_rectangle["hifi1997a_cw"]
                   + datas_rectangle["hifi1997a_cu"]
                   + datas_rectangle["fayard1998_cw"]
                   + datas_rectangle["fayard1998_cu"]
                   ]
                + [(ff, 3) for ff in []
                   + datas_rectangle["cung2000_cw"]
                   + datas_rectangle["cung2000_cu"]
                   ]
                ]
    elif problem == "2NEGH-KP-O":
        # hifi2001 lodi2003 belov2006 hifi2006 alvarez2007 hifi2008 hifi2012
        # (martin2020)
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 " --time-limit " + str(t) +
                 " -q \"RG -p 2NHO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NHO -c 5\" -a \"IBS -f 1.5\""
                 ) for f, t in []
                + [(ff, 1) for ff in []
                   + datas_rectangle["christofides1977"]
                   + datas_rectangle["wang1983"]
                   + datas_rectangle["oliveira1990"]
                   + datas_rectangle["beasley1985"]
                   + datas_rectangle["tschoke1995_cw"]
                   + datas_rectangle["tschoke1995_cu"]
                   + datas_rectangle["hifi1997a_cw"]
                   + datas_rectangle["hifi1997a_cu"]
                   + datas_rectangle["fayard1998_cw"]
                   + datas_rectangle["fayard1998_cu"]
                   ]
                + [(ff, 3) for ff in []
                   + datas_rectangle["cung2000_cw"]
                   + datas_rectangle["cung2000_cu"]
                   ]
                + [(ff, 5) for ff in []
                   + datas_rectangle["alvarez2002_cu"]
                   + datas_rectangle["alvarez2002_cw"]
                   ]
                + [(ff, 300) for ff in []
                   + datas_rectangle["hifi2012_cu"]
                   + datas_rectangle["hifi2012_cw"]
                   ]
                ]
    elif problem == "2NEGV-KP-O":
        # hifi2001 lodi2003 hifi2006 alvarez2007 hifi2008 hifi2012
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 " --time-limit " + str(t) +
                 " -q \"RG -p 2NVO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NVO -c 5\" -a \"IBS -f 1.5\""
                 ) for f, t in []
                + [(ff, 1) for ff in []
                   + datas_rectangle["christofides1977"]
                   + datas_rectangle["wang1983"]
                   + datas_rectangle["oliveira1990"]
                   + datas_rectangle["beasley1985"]
                   + datas_rectangle["tschoke1995_cw"]
                   + datas_rectangle["tschoke1995_cu"]
                   + datas_rectangle["hifi1997a_cw"]
                   + datas_rectangle["hifi1997a_cu"]
                   + datas_rectangle["fayard1998_cw"]
                   + datas_rectangle["fayard1998_cu"]
                   ]
                + [(ff, 3) for ff in []
                   + datas_rectangle["cung2000_cw"]
                   + datas_rectangle["cung2000_cu"]
                   ]
                + [(ff, 5) for ff in []
                   + datas_rectangle["alvarez2002_cu"]
                   + datas_rectangle["alvarez2002_cw"]
                   ]
                + [(ff, 300) for ff in []
                   + datas_rectangle["hifi2012_cu"]
                   + datas_rectangle["hifi2012_cw"]
                   ]
                ]
    elif problem == "2NEGH-KP-R":
        # lodi2003
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 " --time-limit " + str(t) +
                 " -q \"RG -p 2NHR -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NHR -c 5\" -a \"IBS -f 1.5\""
                 ) for f, t in []
                + [(ff, 1) for ff in []
                   + datas_rectangle["wang1983"]
                   + datas_rectangle["oliveira1990"]
                   + datas_rectangle["tschoke1995_cw"]
                   + datas_rectangle["tschoke1995_cu"]
                   + datas_rectangle["hifi1997a_cw"]
                   + datas_rectangle["hifi1997a_cu"]
                   + datas_rectangle["fayard1998_cw"]
                   + datas_rectangle["fayard1998_cu"]
                   ]
                + [(ff, 3) for ff in []
                   + datas_rectangle["cung2000_cw"]
                   + datas_rectangle["cung2000_cu"]
                   ]
                ]

    elif problem == "2G-KP-O":
        # hifi2001
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 " --time-limit 1"
                 " -q \"RG -p 2EAO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2EAO -c 5\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["wang1983"]
                + datas_rectangle["oliveira1990"]
                + datas_rectangle["tschoke1995_cw"]
                + datas_rectangle["tschoke1995_cu"]
                + datas_rectangle["hifi1997a_cw"]
                + datas_rectangle["hifi1997a_cu"]
                + datas_rectangle["fayard1998_cw"]
                + datas_rectangle["fayard1998_cu"]
                + datas_rectangle["cung2000_cw"]
                + datas_rectangle["cung2000_cu"]
                ]
    elif problem == "2GH-KP-O":
        # hifi2001
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 " --time-limit 1"
                 " -q \"RG -p 2EHO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2EHO -c 5\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["wang1983"]
                + datas_rectangle["oliveira1990"]
                + datas_rectangle["tschoke1995_cw"]
                + datas_rectangle["tschoke1995_cu"]
                + datas_rectangle["hifi1997a_cw"]
                + datas_rectangle["hifi1997a_cu"]
                + datas_rectangle["fayard1998_cw"]
                + datas_rectangle["fayard1998_cu"]
                + datas_rectangle["cung2000_cw"]
                + datas_rectangle["cung2000_cu"]
                ]
    elif problem == "2GV-KP-O":
        # hifi2001
        return [(f,
                 " --items data/rectangle/" + f +
                 " --objective knapsack"
                 " --time-limit 1"
                 " -q \"RG -p 2EVO -c 4\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2EVO -c 5\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["wang1983"]
                + datas_rectangle["oliveira1990"]
                + datas_rectangle["tschoke1995_cw"]
                + datas_rectangle["tschoke1995_cu"]
                + datas_rectangle["hifi1997a_cw"]
                + datas_rectangle["hifi1997a_cu"]
                + datas_rectangle["fayard1998_cw"]
                + datas_rectangle["fayard1998_cu"]
                + datas_rectangle["cung2000_cw"]
                + datas_rectangle["cung2000_cu"]
                ]

    # SPP

    elif problem == "3NEGH-SPP-O":
        # [G-SPP-O] bortfeldt2012 wei2014
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-height"
                 " --objective strip-packing-height"
                 " --time-limit 60"
                 " -q \"RG -p 3NHO -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NHO -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NHO -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["kroger1995"]
                + datas_rectangle["hopper2000_n"]
                + datas_rectangle["hopper2000_t"]
                + datas_rectangle["hopper2001a"]
                + datas_rectangle["burke2004"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "3NEGH-SPP-R":
        # [G-SPP-R] kroger1993 schneke1996 mumford2003 zhang2006 bortfeldt2006
        #           cui2008 bortfeldt2012 cui2013 wei2014
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-height"
                 " --objective strip-packing-height"
                 " --time-limit 60"
                 " -q \"RG -p 3NHR -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NHR -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 3NHR -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["kroger1995"]
                + datas_rectangle["hopper2000_n"]
                + datas_rectangle["hopper2000_t"]
                + datas_rectangle["hopper2001a"]
                + datas_rectangle["burke2004"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "2NEGH-SPP-O":
        # lodi2004 cintra2008 mrad2015 cui2017
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-height"
                 " --objective strip-packing-height"
                 " --time-limit 10"
                 " -q \"RG -p 2NHO -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NHO -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NHO -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["cintra2008_bpp"]
                + datas_rectangle["alvarez2002_cu"]
                + datas_rectangle["alvarez2002_cw"]
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]
    elif problem == "2NEGH-SPP-R":
        #
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-height"
                 " --objective strip-packing-height"
                 " --time-limit 10"
                 " -q \"RG -p 2NHR -c 0\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NHR -c 2\" -a \"IBS -f 1.5\""
                 " -q \"RG -p 2NHR -c 3\" -a \"IBS -f 1.5\""
                 ) for f in []
                + datas_rectangle["berkey1987"]
                + datas_rectangle["martello1998"]
                ]

    # VBPP

    if problem == "3NEG-VBPP-O":
        # [G-VBPP-O] cintra2008 ortmann2010 liu2011 hong2014
        # [4GH-VBPP-O] cintra2008
        return [(f,
                 " --items data/rectangle/" + f +
                 x +
                 y +
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 3NHO -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3NHO -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3NVO -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3NVO -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f, x, y in []
                + [(ff, "", "") for ff in []
                   + datas_rectangle["wang1983_vbpp"]
                   ]
                + [(ff, "", "") for ff in []
                   + datas_rectangle["hopper2001b"]
                   ]
                + [(ff, " --bin-infinite-copies", "") for ff in []
                   + datas_rectangle["cintra2008_vbpp"]
                   ]
                + [(ff,
                    " --bin-infinite-copies",
                    " --bin-unweighted") for ff in []
                   + datas_rectangle["pisinger2005"]
                   ]
                + [(ff, " --bin-infinite-copies", "") for ff in []
                   + datas_rectangle["ortmann2010"]
                   ]
                ]
    if problem == "3NEG-VBPP-R":
        # [G-VBPP-R] cintra2008
        # [4GH-VBPP-R] cintra2008
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 3NHR -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3NHR -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3NVR -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 3NVR -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["cintra2008_vbpp"]
                ]
    if problem == "2GH-VBPP-O":
        # cintra2008
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 2NHO -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 2NHO -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["cintra2008_vbpp"]
                ]
    if problem == "2GH-VBPP-R":
        # cintra2008
        return [(f,
                 " --items data/rectangle/" + f +
                 " --bin-infinite-copies"
                 " --objective variable-sized-bin-packing"
                 " --time-limit 60"
                 " -q \"RG -p 2NHR -c 4\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 " -q \"RG -p 2NHR -c 5\" -a \"IBS -f 1.5 -m 128 -M 128\""
                 ) for f in []
                + datas_rectangle["cintra2008_vbpp"]
                ]

    # UKP

    if problem == "G-UKP-O":
        # alvarez2002 bortfeldt2009
        pass
    if problem == "G-UKP-O-D":
        # afsharian2014
        pass
    if problem == "4GH-UKP-O":
        # cintra2008
        pass

    if problem == "3NEGH-UKP-O":
        # hifi2001
        pass
    if problem == "3NEGV-UKP-O":
        # hifi2001
        pass
    if problem == "3GH-UKP-O":
        # hifi2001
        pass
    if problem == "3GV-UKP-O":
        # hifi2001 cui2012
        pass

    if problem == "2NEGH-UKP-O":
        # hifi2001
        pass
    if problem == "2NEGV-UKP-O":
        # hifi2001
        pass
    if problem == "2GH-UKP-O":
        # hifi2001 cintra2008
        pass
    if problem == "2GV-UKP-O":
        # hifi2001
        pass

###############################################################################


if __name__ == "__main__":

    for problem in sys.argv[1:]:
        print("Problem:", problem)
        print()

        directory_out = os.path.join("output", "rectangle", problem)
        if not os.path.exists(directory_out):
            os.makedirs(directory_out)
        results_file = open(os.path.join(directory_out, "results.csv"), "w")
        first_loop = True

        for f, args in get_tests(problem):
            output_filepath = os.path.join(directory_out, f + "_output.json")
            cert_filepath = os.path.join(directory_out, f + "_solution.csv")
            if not os.path.exists(os.path.dirname(output_filepath)):
                os.makedirs(os.path.dirname(output_filepath))
            command = (
                    "./bazel-bin/packingsolver/main"
                    " -p rectangleguillotine -v -e"
                    + args
                    + " -c \"" + cert_filepath + "\""
                    + " -o \"" + output_filepath + "\""
                    )
            print(command)
            os.system(command)
            print()

            output_file = open(output_filepath, "r")
            data = json.load(output_file)

            # Find last solution
            k = 1
            while "Solution" + str(k + 1) in data.keys():
                k += 1

            # Write header (only in the first loop)
            if first_loop:
                first_loop = False
                results_file.write("Name,Arguments")
                for key in data["Solution" + str(k)].keys():
                    results_file.write(";" + key)
                results_file.write("\n")

            # Write test informations
            results_file.write(f + "," + args)
            for key, value in data["Solution" + str(k)].items():
                results_file.write(";" + str(value))
            results_file.write("\n")

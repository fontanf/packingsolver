import os
import subprocess
import sys

ex = os.path.join("doc", "examples")
img = os.path.join("doc", "img")
bin_dir = os.path.join("install", "bin")

############
# Rectangle
############

print("rectangle")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangle", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "solution.csv"),
    "--time-limit", "30",
], stdout=open(os.path.join(ex, "rectangle", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "solution.csv"),
    "--output", os.path.join(img, "rectangle_example_solution.png"),
])

######################
# RectangleGuillotine
######################

print("rectangleguillotine")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangleguillotine"),
    "--items", os.path.join(ex, "rectangleguillotine", "items.csv"),
    "--bins", os.path.join(ex, "rectangleguillotine", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangleguillotine", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangleguillotine", "solution.csv"),
    "--time-limit", "30",
], stdout=open(os.path.join(ex, "rectangleguillotine", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangleguillotine.py"),
    os.path.join(ex, "rectangleguillotine", "solution.csv"),
    "--output", os.path.join(img, "rectangleguillotine_example_solution.png"),
])

#####
# Box
#####

print("box")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_box"),
    "--items", os.path.join(ex, "box", "items.csv"),
    "--bins", os.path.join(ex, "box", "bins.csv"),
    "--parameters", os.path.join(ex, "box", "parameters.csv"),
    "--certificate", os.path.join(ex, "box", "solution.csv"),
    "--time-limit", "30",
], stdout=open(os.path.join(ex, "box", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_box.py"),
    os.path.join(ex, "box", "solution.csv"),
    "--output", os.path.join(img, "box_example_solution.png"),
])

###########
# BoxStacks
###########

print("boxstacks")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_boxstacks"),
    "--items", os.path.join(ex, "boxstacks", "items.csv"),
    "--bins", os.path.join(ex, "boxstacks", "bins.csv"),
    "--parameters", os.path.join(ex, "boxstacks", "parameters.csv"),
    "--certificate", os.path.join(ex, "boxstacks", "solution.csv"),
    "--time-limit", "30",
], stdout=open(os.path.join(ex, "boxstacks", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_boxstacks.py"),
    os.path.join(ex, "boxstacks", "solution.csv"),
    "--output", os.path.join(img, "boxstacks_example_solution.png"),
])

##########
# Irregular
##########

print("irregular")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_irregular"),
    "--input", os.path.join(ex, "irregular", "instance.json"),
    "--certificate", os.path.join(ex, "irregular", "solution.json"),
    "--time-limit", "30",
], stdout=open(os.path.join(ex, "irregular", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_irregular.py"),
    os.path.join(ex, "irregular", "solution.json"),
    "--output", os.path.join(img, "irregular_example_solution.png"),
])

################
# OneDimensional
################

print("onedimensional")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_onedimensional"),
    "--items", os.path.join(ex, "onedimensional", "items.csv"),
    "--bins", os.path.join(ex, "onedimensional", "bins.csv"),
    "--parameters", os.path.join(ex, "onedimensional", "parameters.csv"),
    "--certificate", os.path.join(ex, "onedimensional", "solution.csv"),
    "--time-limit", "30",
], stdout=open(os.path.join(ex, "onedimensional", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_solution.png"),
])

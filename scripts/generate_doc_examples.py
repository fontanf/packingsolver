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
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "solution.csv"),
    "--output", os.path.join(img, "rectangle_example_solution.png"),
])

print("rectangle_defects_no")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "defects_no", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "defects_no", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangle", "defects_no", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "defects_no", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "defects_no", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "defects_no", "solution.csv"),
    "--output", os.path.join(img, "rectangle_defects_no.png"),
])

print("rectangle_defects_yes")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "defects_yes", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "defects_yes", "bins.csv"),
    "--defects", os.path.join(ex, "rectangle", "defects_yes", "defects.csv"),
    "--parameters", os.path.join(ex, "rectangle", "defects_yes", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "defects_yes", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "defects_yes", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "defects_yes", "solution.csv"),
    "--output", os.path.join(img, "rectangle_defects_yes.png"),
])

print("rectangle_rotation_no")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "rotation_no", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "rotation_no", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangle", "rotation_no", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "rotation_no", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "rotation_no", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "rotation_no", "solution.csv"),
    "--output", os.path.join(img, "rectangle_rotation_no.png"),
])

print("rectangle_rotation_yes")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "rotation_yes", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "rotation_yes", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangle", "rotation_yes", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "rotation_yes", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "rotation_yes", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "rotation_yes", "solution.csv"),
    "--output", os.path.join(img, "rectangle_rotation_yes.png"),
])

print("rectangle_maximum_weight_no")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "maximum_weight_no", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "maximum_weight_no", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangle", "maximum_weight_no", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "maximum_weight_no", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "maximum_weight_no", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "maximum_weight_no", "solution.csv"),
    "--output", os.path.join(img, "rectangle_maximum_weight_no.png"),
])

print("rectangle_maximum_weight_yes")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "maximum_weight_yes", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "maximum_weight_yes", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangle", "maximum_weight_yes", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "maximum_weight_yes", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "maximum_weight_yes", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "maximum_weight_yes", "solution.csv"),
    "--output", os.path.join(img, "rectangle_maximum_weight_yes.png"),
])

print("rectangle_unloading_no")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "unloading_no", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "unloading_no", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangle", "unloading_no", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "unloading_no", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "unloading_no", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "unloading_no", "solution.csv"),
     "GROUP_ID",
    "--output", os.path.join(img, "rectangle_unloading_no.png"),
])

print("rectangle_unloading_yes")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "unloading_yes", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "unloading_yes", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangle", "unloading_yes", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "unloading_yes", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "unloading_yes", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "unloading_yes", "solution.csv"),
    "GROUP_ID",
    "--output", os.path.join(img, "rectangle_unloading_yes.png"),
])

print("rectangle_unloading_x_movements")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "unloading_x_movements", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "unloading_x_movements", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangle", "unloading_x_movements", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "unloading_x_movements", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "unloading_x_movements", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "unloading_x_movements", "solution.csv"),
    "GROUP_ID",
    "--output", os.path.join(img, "rectangle_unloading_x_movements.png"),
])

print("rectangle_unloading_increasing_x")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "rectangle", "unloading_increasing_x", "items.csv"),
    "--bins", os.path.join(ex, "rectangle", "unloading_increasing_x", "bins.csv"),
    "--parameters", os.path.join(ex, "rectangle", "unloading_increasing_x", "parameters.csv"),
    "--certificate", os.path.join(ex, "rectangle", "unloading_increasing_x", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "unloading_increasing_x", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "unloading_increasing_x", "solution.csv"),
    "GROUP_ID",
    "--output", os.path.join(img, "rectangle_unloading_increasing_x.png"),
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
    "--time-limit", "5",
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
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "box", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_box.py"),
    os.path.join(ex, "box", "solution.csv"),
    "--output", os.path.join(img, "box_example_solution.png"),
])

print("box_maximum_weight_no")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_box"),
    "--items", os.path.join(ex, "box", "maximum_weight_no", "items.csv"),
    "--bins", os.path.join(ex, "box", "maximum_weight_no", "bins.csv"),
    "--parameters", os.path.join(ex, "box", "maximum_weight_no", "parameters.csv"),
    "--certificate", os.path.join(ex, "box", "maximum_weight_no", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "box", "maximum_weight_no", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_box.py"),
    os.path.join(ex, "box", "maximum_weight_no", "solution.csv"),
    "--output", os.path.join(img, "box_maximum_weight_no.png"),
])

print("box_maximum_weight_yes")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_box"),
    "--items", os.path.join(ex, "box", "maximum_weight_yes", "items.csv"),
    "--bins", os.path.join(ex, "box", "maximum_weight_yes", "bins.csv"),
    "--parameters", os.path.join(ex, "box", "maximum_weight_yes", "parameters.csv"),
    "--certificate", os.path.join(ex, "box", "maximum_weight_yes", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "box", "maximum_weight_yes", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_box.py"),
    os.path.join(ex, "box", "maximum_weight_yes", "solution.csv"),
    "--output", os.path.join(img, "box_maximum_weight_yes.png"),
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
    "--time-limit", "5",
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
    "--time-limit", "5",
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
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_solution.png"),
])

print("onedimensional_maximum_weight_no")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_onedimensional"),
    "--items", os.path.join(ex, "onedimensional", "maximum_weight_no", "items.csv"),
    "--bins", os.path.join(ex, "onedimensional", "maximum_weight_no", "bins.csv"),
    "--parameters", os.path.join(ex, "onedimensional", "maximum_weight_no", "parameters.csv"),
    "--certificate", os.path.join(ex, "onedimensional", "maximum_weight_no", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "maximum_weight_no", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "maximum_weight_no", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_maximum_weight_no.png"),
])

print("onedimensional_maximum_weight_yes")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_onedimensional"),
    "--items", os.path.join(ex, "onedimensional", "maximum_weight_yes", "items.csv"),
    "--bins", os.path.join(ex, "onedimensional", "maximum_weight_yes", "bins.csv"),
    "--parameters", os.path.join(ex, "onedimensional", "maximum_weight_yes", "parameters.csv"),
    "--certificate", os.path.join(ex, "onedimensional", "maximum_weight_yes", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "maximum_weight_yes", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "maximum_weight_yes", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_maximum_weight_yes.png"),
])

print("onedimensional_nesting_length_no")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_onedimensional"),
    "--items", os.path.join(ex, "onedimensional", "nesting_length_no", "items.csv"),
    "--bins", os.path.join(ex, "onedimensional", "nesting_length_no", "bins.csv"),
    "--parameters", os.path.join(ex, "onedimensional", "nesting_length_no", "parameters.csv"),
    "--certificate", os.path.join(ex, "onedimensional", "nesting_length_no", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "nesting_length_no", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "nesting_length_no", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_nesting_length_no.png"),
])

print("onedimensional_nesting_length_yes")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_onedimensional"),
    "--items", os.path.join(ex, "onedimensional", "nesting_length_yes", "items.csv"),
    "--bins", os.path.join(ex, "onedimensional", "nesting_length_yes", "bins.csv"),
    "--parameters", os.path.join(ex, "onedimensional", "nesting_length_yes", "parameters.csv"),
    "--certificate", os.path.join(ex, "onedimensional", "nesting_length_yes", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "nesting_length_yes", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "nesting_length_yes", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_nesting_length_yes.png"),
])

print("onedimensional_maximum_stackability_no")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_onedimensional"),
    "--items", os.path.join(ex, "onedimensional", "maximum_stackability_no", "items.csv"),
    "--bins", os.path.join(ex, "onedimensional", "maximum_stackability_no", "bins.csv"),
    "--parameters", os.path.join(ex, "onedimensional", "maximum_stackability_no", "parameters.csv"),
    "--certificate", os.path.join(ex, "onedimensional", "maximum_stackability_no", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "maximum_stackability_no", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "maximum_stackability_no", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_maximum_stackability_no.png"),
])

print("onedimensional_maximum_stackability_yes")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_onedimensional"),
    "--items", os.path.join(ex, "onedimensional", "maximum_stackability_yes", "items.csv"),
    "--bins", os.path.join(ex, "onedimensional", "maximum_stackability_yes", "bins.csv"),
    "--parameters", os.path.join(ex, "onedimensional", "maximum_stackability_yes", "parameters.csv"),
    "--certificate", os.path.join(ex, "onedimensional", "maximum_stackability_yes", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "maximum_stackability_yes", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "maximum_stackability_yes", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_maximum_stackability_yes.png"),
])

print("onedimensional_maximum_weight_after_no")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_onedimensional"),
    "--items", os.path.join(ex, "onedimensional", "maximum_weight_after_no", "items.csv"),
    "--bins", os.path.join(ex, "onedimensional", "maximum_weight_after_no", "bins.csv"),
    "--parameters", os.path.join(ex, "onedimensional", "maximum_weight_after_no", "parameters.csv"),
    "--certificate", os.path.join(ex, "onedimensional", "maximum_weight_after_no", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "maximum_weight_after_no", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "maximum_weight_after_no", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_maximum_weight_after_no.png"),
])

print("onedimensional_maximum_weight_after_yes")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_onedimensional"),
    "--items", os.path.join(ex, "onedimensional", "maximum_weight_after_yes", "items.csv"),
    "--bins", os.path.join(ex, "onedimensional", "maximum_weight_after_yes", "bins.csv"),
    "--parameters", os.path.join(ex, "onedimensional", "maximum_weight_after_yes", "parameters.csv"),
    "--certificate", os.path.join(ex, "onedimensional", "maximum_weight_after_yes", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "maximum_weight_after_yes", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "maximum_weight_after_yes", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_maximum_weight_after_yes.png"),
])

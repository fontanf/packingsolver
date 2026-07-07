import os
import subprocess
import sys

ex = os.path.join("doc", "examples")
img = os.path.join("doc", "img")
internals = os.path.join("doc", "internals")
bin_dir = os.path.join("install", "bin")

####################
# Internals diagrams
####################

for filename in sorted(os.listdir(internals)):
    if not filename.endswith(".drawio"):
        continue
    name = filename[:-len(".drawio")]
    print(name)
    subprocess.run([
        "drawio", "-x", "-f", "png",
        "-o", os.path.join(img, name + ".png"),
        os.path.join(internals, filename),
    ])

############
# Objectives
############

print("objective_bin_packing_with_leftovers")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "objectives", "bin_packing_with_leftovers", "items.csv"),
    "--bins", os.path.join(ex, "objectives", "bin_packing_with_leftovers", "bins.csv"),
    "--parameters", os.path.join(ex, "objectives", "bin_packing_with_leftovers", "parameters.csv"),
    "--certificate", os.path.join(ex, "objectives", "bin_packing_with_leftovers", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "objectives", "bin_packing_with_leftovers", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "objectives", "bin_packing_with_leftovers", "solution.csv"),
    "--output", os.path.join(img, "objective_bin_packing_with_leftovers_solution.png"),
    "--columns", "1",
    "--scale", "0.5",
])

print("objective_variable_sized_bin_packing")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "objectives", "variable_sized_bin_packing", "items.csv"),
    "--bins", os.path.join(ex, "objectives", "variable_sized_bin_packing", "bins.csv"),
    "--parameters", os.path.join(ex, "objectives", "variable_sized_bin_packing", "parameters.csv"),
    "--certificate", os.path.join(ex, "objectives", "variable_sized_bin_packing", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "objectives", "variable_sized_bin_packing", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "objectives", "variable_sized_bin_packing", "solution.csv"),
    "--output", os.path.join(img, "objective_variable_sized_bin_packing_solution.png"),
    "--columns", "1",
    "--scale", "0.5",
])

print("objective_open_dimension_x")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "objectives", "open_dimension_x", "items.csv"),
    "--bins", os.path.join(ex, "objectives", "open_dimension_x", "bins.csv"),
    "--parameters", os.path.join(ex, "objectives", "open_dimension_x", "parameters.csv"),
    "--certificate", os.path.join(ex, "objectives", "open_dimension_x", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "objectives", "open_dimension_x", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "objectives", "open_dimension_x", "solution.csv"),
    "--output", os.path.join(img, "objective_open_dimension_x_solution.png"),
    "--columns", "1",
    "--scale", "0.5",
])

print("objective_knapsack")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(ex, "objectives", "knapsack", "items.csv"),
    "--bins", os.path.join(ex, "objectives", "knapsack", "bins.csv"),
    "--parameters", os.path.join(ex, "objectives", "knapsack", "parameters.csv"),
    "--certificate", os.path.join(ex, "objectives", "knapsack", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "objectives", "knapsack", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "objectives", "knapsack", "solution.csv"),
    "--output", os.path.join(img, "objective_knapsack_solution.png"),
    "--columns", "1",
    "--scale", "0.5",
])

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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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
    "--columns", "1",
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

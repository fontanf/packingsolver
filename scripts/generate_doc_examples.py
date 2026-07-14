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
    "--scale", "1",
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
    "--scale", "0.5",
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
    "--scale", "0.5",
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
    "--scale", "0.5",
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
    "--scale", "0.5",
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
    "--scale", "0.5",
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
    "--scale", "0.5",
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
    "--scale", "0.5",
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
    "--scale", "0.5",
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
    "--scale", "0.5",
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
    "--scale", "0.5",
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
    "--scale", "1",
])

for name in ["stages_2", "stages_3", "stages_unlimited"]:
    print("rectangleguillotine_" + name)
    subprocess.run([
        os.path.join(bin_dir, "packingsolver_rectangleguillotine"),
        "--items", os.path.join(ex, "rectangleguillotine", name, "items.csv"),
        "--bins", os.path.join(ex, "rectangleguillotine", name, "bins.csv"),
        "--parameters", os.path.join(ex, "rectangleguillotine", name, "parameters.csv"),
        "--certificate", os.path.join(ex, "rectangleguillotine", name, "solution.csv"),
        "--time-limit", "10",
    ], stdout=open(os.path.join(ex, "rectangleguillotine", name, "output.txt"), "w"), stderr=subprocess.STDOUT)
    subprocess.run([
        sys.executable, os.path.join("scripts", "visualize_rectangleguillotine.py"),
        os.path.join(ex, "rectangleguillotine", name, "solution.csv"),
        "--output", os.path.join(img, "rectangleguillotine_" + name + ".png"),
        "--columns", "1",
        "--scale", "7",
    ])

for name in [
        "cutthickness_no", "cutthickness_yes",
        "cutdefects_no", "cutdefects_yes",
        "defects_no", "defects_yes",
        "maxcuts2_no", "maxcuts2_yes",
        "maxcuts_no", "maxcuts_yes",
        "orientation_no", "orientation_yes",
        "rotation_no", "rotation_yes",
        "stacks_no", "stacks_yes",
        "trims_no", "trims_yes"]:
    print("rectangleguillotine_" + name)
    command = [
        os.path.join(bin_dir, "packingsolver_rectangleguillotine"),
        "--items", os.path.join(ex, "rectangleguillotine", name, "items.csv"),
        "--bins", os.path.join(ex, "rectangleguillotine", name, "bins.csv"),
        "--parameters", os.path.join(ex, "rectangleguillotine", name, "parameters.csv"),
        "--certificate", os.path.join(ex, "rectangleguillotine", name, "solution.csv"),
        "--time-limit", "5",
    ]
    defects_path = os.path.join(ex, "rectangleguillotine", name, "defects.csv")
    if os.path.exists(defects_path):
        command += ["--defects", defects_path]
    subprocess.run(command, stdout=open(os.path.join(ex, "rectangleguillotine", name, "output.txt"), "w"), stderr=subprocess.STDOUT)
    subprocess.run([
        sys.executable, os.path.join("scripts", "visualize_rectangleguillotine.py"),
        os.path.join(ex, "rectangleguillotine", name, "solution.csv"),
        "--output", os.path.join(img, "rectangleguillotine_" + name + ".png"),
        "--columns", "1",
        "--scale", "20",
    ])

for name in ["cuttype_roadef2018", "cuttype_nonexact", "cuttype_exact", "cuttype_homogenous"]:
    print("rectangleguillotine_" + name)
    subprocess.run([
        os.path.join(bin_dir, "packingsolver_rectangleguillotine"),
        "--items", os.path.join(ex, "rectangleguillotine", name, "items.csv"),
        "--bins", os.path.join(ex, "rectangleguillotine", name, "bins.csv"),
        "--parameters", os.path.join(ex, "rectangleguillotine", name, "parameters.csv"),
        "--certificate", os.path.join(ex, "rectangleguillotine", name, "solution.csv"),
        "--time-limit", "10",
    ], stdout=open(os.path.join(ex, "rectangleguillotine", name, "output.txt"), "w"), stderr=subprocess.STDOUT)
    subprocess.run([
        sys.executable, os.path.join("scripts", "visualize_rectangleguillotine.py"),
        os.path.join(ex, "rectangleguillotine", name, "solution.csv"),
        "--output", os.path.join(img, "rectangleguillotine_" + name + ".png"),
        "--columns", "1",
        "--width", "720",
        "--height", "900",
    ])

print("rectangleguillotine_guillotine_vs_non_guillotine_rectangle")
gvng_dir = os.path.join(ex, "rectangleguillotine", "guillotine_vs_non_guillotine_rectangle")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--items", os.path.join(gvng_dir, "items.csv"),
    "--bins", os.path.join(gvng_dir, "bins.csv"),
    "--parameters", os.path.join(gvng_dir, "parameters.csv"),
    "--certificate", os.path.join(gvng_dir, "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(gvng_dir, "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(gvng_dir, "solution.csv"),
    "--output", os.path.join(img, "rectangleguillotine_guillotine_vs_non_guillotine_rectangle.png"),
    "--columns", "1",
    "--scale", "20",
])

print("rectangleguillotine_guillotine_vs_non_guillotine_rectangleguillotine")
gvng_dir = os.path.join(ex, "rectangleguillotine", "guillotine_vs_non_guillotine_rectangleguillotine")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangleguillotine"),
    "--items", os.path.join(gvng_dir, "items.csv"),
    "--bins", os.path.join(gvng_dir, "bins.csv"),
    "--parameters", os.path.join(gvng_dir, "parameters.csv"),
    "--certificate", os.path.join(gvng_dir, "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(gvng_dir, "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangleguillotine.py"),
    os.path.join(gvng_dir, "solution.csv"),
    "--output", os.path.join(img, "rectangleguillotine_guillotine_vs_non_guillotine_rectangleguillotine.png"),
    "--columns", "1",
    "--scale", "20",
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
    "--scale", "5",
])

for name in ["maximum_weight_no", "maximum_weight_yes", "rotation_no", "rotation_yes"]:
    print("box_" + name)
    subprocess.run([
        os.path.join(bin_dir, "packingsolver_box"),
        "--items", os.path.join(ex, "box", name, "items.csv"),
        "--bins", os.path.join(ex, "box", name, "bins.csv"),
        "--parameters", os.path.join(ex, "box", name, "parameters.csv"),
        "--certificate", os.path.join(ex, "box", name, "solution.csv"),
        "--time-limit", "5",
    ], stdout=open(os.path.join(ex, "box", name, "output.txt"), "w"), stderr=subprocess.STDOUT)
    subprocess.run([
        sys.executable, os.path.join("scripts", "visualize_box.py"),
        os.path.join(ex, "box", name, "solution.csv"),
        "--output", os.path.join(img, "box_" + name + ".png"),
        "--columns", "1",
        "--scale", "20",
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
    "--width", "1060",
    "--height", "890",
])

print("boxstacks_box_vs_boxstacks_box")
bvb_box_dir = os.path.join(ex, "boxstacks", "box_vs_boxstacks_box")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_box"),
    "--items", os.path.join(bvb_box_dir, "items.csv"),
    "--bins", os.path.join(bvb_box_dir, "bins.csv"),
    "--parameters", os.path.join(bvb_box_dir, "parameters.csv"),
    "--certificate", os.path.join(bvb_box_dir, "solution.csv"),
    "--time-limit", "10",
], stdout=open(os.path.join(bvb_box_dir, "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_box.py"),
    os.path.join(bvb_box_dir, "solution.csv"),
    "--output", os.path.join(img, "boxstacks_box_vs_boxstacks_box.png"),
    "--width", "1000",
    "--height", "550",
    "--no-legend",
    "--autocrop",
])

print("boxstacks_box_vs_boxstacks_boxstacks")
bvb_boxstacks_dir = os.path.join(ex, "boxstacks", "box_vs_boxstacks_boxstacks")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_boxstacks"),
    "--items", os.path.join(bvb_boxstacks_dir, "items.csv"),
    "--bins", os.path.join(bvb_boxstacks_dir, "bins.csv"),
    "--parameters", os.path.join(bvb_boxstacks_dir, "parameters.csv"),
    "--certificate", os.path.join(bvb_boxstacks_dir, "solution.csv"),
    "--time-limit", "10",
], stdout=open(os.path.join(bvb_boxstacks_dir, "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_boxstacks.py"),
    os.path.join(bvb_boxstacks_dir, "solution.csv"),
    "--output", os.path.join(img, "boxstacks_box_vs_boxstacks_boxstacks.png"),
    "--width", "1000",
    "--height", "550",
    "--no-legend",
    "--autocrop",
])

for name in [
        "maximum_stack_density_no", "maximum_stack_density_yes",
        "maximum_stackability_no", "maximum_stackability_yes",
        "maximum_weight_above_no", "maximum_weight_above_yes",
        "nesting_height_no", "nesting_height_yes",
        "rotation_no", "rotation_yes"]:
    print("boxstacks_" + name)
    subprocess.run([
        os.path.join(bin_dir, "packingsolver_boxstacks"),
        "--items", os.path.join(ex, "boxstacks", name, "items.csv"),
        "--bins", os.path.join(ex, "boxstacks", name, "bins.csv"),
        "--parameters", os.path.join(ex, "boxstacks", name, "parameters.csv"),
        "--certificate", os.path.join(ex, "boxstacks", name, "solution.csv"),
        "--time-limit", "5",
    ], stdout=open(os.path.join(ex, "boxstacks", name, "output.txt"), "w"), stderr=subprocess.STDOUT)
    # Examples whose solution uses more than one bin: display them one per
    # row (a single column) instead of collapsing/side-by-side. "expand" is
    # needed when the solver collapsed identical bins into one BIN row with
    # COPIES > 1; it is not needed when the CSV already lists separate rows
    # (e.g. because the bins differ).
    multi_bin_rows = {
        "rotation_no": (2, True),
        "nesting_height_no": (2, False),
        "maximum_stackability_yes": (2, False),
        "maximum_weight_above_yes": (2, False),
        "maximum_stack_density_yes": (2, False),
    }
    # These examples all share the same 7500x2400x3000 bin. A single bin
    # needs width 550 to avoid clipping the axis tick labels at the default
    # camera zoom; stacking bins vertically (columns=1) needs extra width
    # (650) on top of that, even though each row's own height (400) doesn't
    # change, to leave room for the axis labels of the stacked scenes.
    if name in multi_bin_rows:
        n, expand = multi_bin_rows[name]
        width, height = 650, 400 * n
    else:
        width, height = 550, 400
    cmd = [
        sys.executable, os.path.join("scripts", "visualize_boxstacks.py"),
        os.path.join(ex, "boxstacks", name, "solution.csv"),
        "--output", os.path.join(img, "boxstacks_" + name + ".png"),
        "--width", str(width),
        "--height", str(height),
        "--no-legend",
        "--autocrop",
    ]
    if name in multi_bin_rows:
        cmd += ["--columns", "1"]
        if multi_bin_rows[name][1]:
            cmd.append("--expand-copies")
    subprocess.run(cmd)

for name in ["axle_weight_no", "axle_weight_yes"]:
    print("boxstacks_" + name)
    subprocess.run([
        os.path.join(bin_dir, "packingsolver_boxstacks"),
        "--items", os.path.join(ex, "boxstacks", name, "items.csv"),
        "--bins", os.path.join(ex, "boxstacks", name, "bins.csv"),
        "--parameters", os.path.join(ex, "boxstacks", name, "parameters.csv"),
        "--certificate", os.path.join(ex, "boxstacks", name, "solution.csv"),
        "--time-limit", "10",
    ], stdout=open(os.path.join(ex, "boxstacks", name, "output.txt"), "w"), stderr=subprocess.STDOUT)
    # axle_weight_yes uses 2 bins: stack them one per row.
    height = 700 if name == "axle_weight_yes" else 350
    cmd = [
        sys.executable, os.path.join("scripts", "visualize_boxstacks.py"),
        os.path.join(ex, "boxstacks", name, "solution.csv"),
        "--output", os.path.join(img, "boxstacks_" + name + ".png"),
        "--width", "900",
        "--height", str(height),
        "--no-legend",
        "--autocrop",
    ]
    if name == "axle_weight_yes":
        cmd += ["--columns", "1"]
    subprocess.run(cmd)

for name, height in [("unloading_no", 410), ("unloading_yes", 650)]:
    print("boxstacks_" + name)
    subprocess.run([
        os.path.join(bin_dir, "packingsolver_boxstacks"),
        "--items", os.path.join(ex, "boxstacks", name, "items.csv"),
        "--bins", os.path.join(ex, "boxstacks", name, "bins.csv"),
        "--parameters", os.path.join(ex, "boxstacks", name, "parameters.csv"),
        "--certificate", os.path.join(ex, "boxstacks", name, "solution.csv"),
        "--time-limit", "5",
    ], stdout=open(os.path.join(ex, "boxstacks", name, "output.txt"), "w"), stderr=subprocess.STDOUT)
    subprocess.run([
        sys.executable, os.path.join("scripts", "visualize_boxstacks.py"),
        os.path.join(ex, "boxstacks", name, "solution.csv"),
        "--output", os.path.join(img, "boxstacks_" + name + ".png"),
        "--columns", "1",
        "--width", "580",
        "--height", str(height),
        "--no-legend",
        "--autocrop",
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
    "--width", "640",
    "--height", "700",
])

irregular_examples = [
        ("defects_no", 5, 4), ("defects_yes", 5, 4),
        ("holes_no", 5, 4), ("holes_yes", 5, 4),
        ("item_bin_spacing_no", 10, 3), ("item_bin_spacing_yes", 10, 3),
        ("item_defect_spacing_no", 8, 3), ("item_defect_spacing_yes", 8, 3),
        ("item_item_spacing_no", 10, 3), ("item_item_spacing_yes", 10, 3),
        ("mirroring_no", 5, 2.5), ("mirroring_yes", 5, 2.5),
        ("rotation_no", 5, 4), ("rotation_yes", 5, 4),
        ("rotation_continuous", 10, 3)]
for name, time_limit, scale in irregular_examples:
    print("irregular_" + name)
    subprocess.run([
        os.path.join(bin_dir, "packingsolver_irregular"),
        "--input", os.path.join(ex, "irregular", name, "instance.json"),
        "--certificate", os.path.join(ex, "irregular", name, "solution.json"),
        "--time-limit", str(time_limit),
    ], stdout=open(os.path.join(ex, "irregular", name, "output.txt"), "w"), stderr=subprocess.STDOUT)
    subprocess.run([
        sys.executable, os.path.join("scripts", "visualize_irregular.py"),
        os.path.join(ex, "irregular", name, "solution.json"),
        "--output", os.path.join(img, "irregular_" + name + ".png"),
        "--columns", "1",
        "--scale", str(scale),
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
    "--expand-copies",
])

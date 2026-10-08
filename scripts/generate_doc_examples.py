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
    "--input", os.path.join(ex, "rectangle", "objective_bin_packing_with_leftovers", "instance.json"),
    "--certificate", os.path.join(ex, "rectangle", "objective_bin_packing_with_leftovers", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "objective_bin_packing_with_leftovers", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "objective_bin_packing_with_leftovers", "solution.csv"),
    "--output", os.path.join(img, "objective_bin_packing_with_leftovers_solution.png"),
    "--columns", "1",
    "--scale", "0.5",
])

print("objective_variable_sized_bin_packing")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--input", os.path.join(ex, "rectangle", "objective_variable_sized_bin_packing", "instance.json"),
    "--certificate", os.path.join(ex, "rectangle", "objective_variable_sized_bin_packing", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "objective_variable_sized_bin_packing", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "objective_variable_sized_bin_packing", "solution.csv"),
    "--output", os.path.join(img, "objective_variable_sized_bin_packing_solution.png"),
    "--columns", "1",
    "--scale", "0.5",
])

print("objective_open_dimension_x")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--input", os.path.join(ex, "rectangle", "objective_open_dimension_x", "instance.json"),
    "--certificate", os.path.join(ex, "rectangle", "objective_open_dimension_x", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "objective_open_dimension_x", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "objective_open_dimension_x", "solution.csv"),
    "--output", os.path.join(img, "objective_open_dimension_x_solution.png"),
    "--columns", "1",
    "--scale", "0.5",
])

print("objective_knapsack")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--input", os.path.join(ex, "rectangle", "objective_knapsack", "instance.json"),
    "--certificate", os.path.join(ex, "rectangle", "objective_knapsack", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangle", "objective_knapsack", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
    os.path.join(ex, "rectangle", "objective_knapsack", "solution.csv"),
    "--output", os.path.join(img, "objective_knapsack_solution.png"),
    "--columns", "1",
    "--scale", "0.5",
])

############
# Rectangle
############

# The Python and C++ versions of the instances.
subprocess.run([sys.executable, os.path.join("scripts", "generate_doc_snippets.py")])

# Name, directory, colored by group, scale.
rectangle_examples = [
    ("rectangle_example_solution", "basic", False, "1"),
    ("rectangle_defects_no", "defects_no", False, "0.5"),
    ("rectangle_defects_yes", "defects_yes", False, "0.5"),
    ("rectangle_rotation_no", "rotation_no", False, "0.5"),
    ("rectangle_rotation_yes", "rotation_yes", False, "0.5"),
    ("rectangle_maximum_weight_no", "maximum_weight_no", False, "0.5"),
    ("rectangle_maximum_weight_yes", "maximum_weight_yes", False, "0.5"),
    ("rectangle_unloading_no", "unloading_no", True, "0.5"),
    ("rectangle_unloading_yes", "unloading_yes", True, "0.5"),
    ("rectangle_unloading_x_movements", "unloading_x_movements", True, "0.5"),
    ("rectangle_unloading_increasing_x", "unloading_increasing_x", True, "0.5"),
]
for name, directory, by_group, scale in rectangle_examples:
    print(name)
    example = os.path.join(ex, "rectangle", directory)
    subprocess.run([
        os.path.join(bin_dir, "packingsolver_rectangle"),
        "--input", os.path.join(example, "instance.json"),
        "--certificate", os.path.join(example, "solution.csv"),
        "--time-limit", "5",
    ], stdout=open(os.path.join(example, "output.txt"), "w"), stderr=subprocess.STDOUT)
    subprocess.run([
        sys.executable, os.path.join("scripts", "visualize_rectangle.py"),
        os.path.join(example, "solution.csv"),
        *(["GROUP_ID"] if by_group else []),
        "--output", os.path.join(img, name + ".png"),
        "--columns", "1",
        "--scale", scale,
    ])

######################
# RectangleGuillotine
######################

print("rectangleguillotine")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangleguillotine"),
    "--input", os.path.join(ex, "rectangleguillotine", "basic", "instance.json"),
    "--certificate", os.path.join(ex, "rectangleguillotine", "basic", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "rectangleguillotine", "basic", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_rectangleguillotine.py"),
    os.path.join(ex, "rectangleguillotine", "basic", "solution.csv"),
    "--output", os.path.join(img, "rectangleguillotine_example_solution.png"),
    "--columns", "1",
    "--scale", "1",
])

for name in ["stages_2", "stages_3", "stages_unlimited"]:
    print("rectangleguillotine_" + name)
    subprocess.run([
        os.path.join(bin_dir, "packingsolver_rectangleguillotine"),
        "--input", os.path.join(ex, "rectangleguillotine", name, "instance.json"),
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
        "--input", os.path.join(ex, "rectangleguillotine", name, "instance.json"),
        "--certificate", os.path.join(ex, "rectangleguillotine", name, "solution.csv"),
        "--time-limit", "5",
    ]
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
        "--input", os.path.join(ex, "rectangleguillotine", name, "instance.json"),
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
gvng_dir = os.path.join(ex, "rectangle", "guillotine_vs_non_guillotine")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangle"),
    "--input", os.path.join(gvng_dir, "instance.json"),
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
gvng_dir = os.path.join(ex, "rectangleguillotine", "guillotine_vs_non_guillotine")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_rectangleguillotine"),
    "--input", os.path.join(gvng_dir, "instance.json"),
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
    "--input", os.path.join(ex, "box", "basic", "instance.json"),
    "--certificate", os.path.join(ex, "box", "basic", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "box", "basic", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_box.py"),
    os.path.join(ex, "box", "basic", "solution.csv"),
    "--output", os.path.join(img, "box_example_solution.png"),
    "--columns", "1",
    "--scale", "5",
])

for name in ["maximum_weight_no", "maximum_weight_yes", "rotation_no", "rotation_yes"]:
    print("box_" + name)
    subprocess.run([
        os.path.join(bin_dir, "packingsolver_box"),
        "--input", os.path.join(ex, "box", name, "instance.json"),
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
    "--input", os.path.join(ex, "boxstacks", "basic", "instance.json"),
    "--certificate", os.path.join(ex, "boxstacks", "basic", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "boxstacks", "basic", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_boxstacks.py"),
    os.path.join(ex, "boxstacks", "basic", "solution.csv"),
    "--output", os.path.join(img, "boxstacks_example_solution.png"),
    "--columns", "1",
    "--width", "1060",
    "--height", "890",
])

print("boxstacks_box_vs_boxstacks_box")
bvb_box_dir = os.path.join(ex, "box", "box_vs_boxstacks")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_box"),
    "--input", os.path.join(bvb_box_dir, "instance.json"),
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
bvb_boxstacks_dir = os.path.join(ex, "boxstacks", "box_vs_boxstacks")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_boxstacks"),
    "--input", os.path.join(bvb_boxstacks_dir, "instance.json"),
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
        "--input", os.path.join(ex, "boxstacks", name, "instance.json"),
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
        "--input", os.path.join(ex, "boxstacks", name, "instance.json"),
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
        "--input", os.path.join(ex, "boxstacks", name, "instance.json"),
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
    "--input", os.path.join(ex, "irregular", "basic", "instance.json"),
    "--certificate", os.path.join(ex, "irregular", "basic", "solution.json"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "irregular", "basic", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_irregular.py"),
    os.path.join(ex, "irregular", "basic", "solution.json"),
    "--output", os.path.join(img, "irregular_example_solution.png"),
    "--columns", "1",
    "--width", "640",
    "--height", "700",
])

irregular_examples = [
        ("defects_no", 5, 4), ("defects_yes", 5, 4),
        ("holes_no", 5, 4), ("holes_yes", 5, 4),
        ("irregular_bin", 10, 4),
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
    "--input", os.path.join(ex, "onedimensional", "basic", "instance.json"),
    "--certificate", os.path.join(ex, "onedimensional", "basic", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "basic", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "basic", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_solution.png"),
])

print("onedimensional_maximum_weight_no")
subprocess.run([
    os.path.join(bin_dir, "packingsolver_onedimensional"),
    "--input", os.path.join(ex, "onedimensional", "maximum_weight_no", "instance.json"),
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
    "--input", os.path.join(ex, "onedimensional", "maximum_weight_yes", "instance.json"),
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
    "--input", os.path.join(ex, "onedimensional", "nesting_length_no", "instance.json"),
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
    "--input", os.path.join(ex, "onedimensional", "nesting_length_yes", "instance.json"),
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
    "--input", os.path.join(ex, "onedimensional", "maximum_stackability_no", "instance.json"),
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
    "--input", os.path.join(ex, "onedimensional", "maximum_stackability_yes", "instance.json"),
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
    "--input", os.path.join(ex, "onedimensional", "maximum_weight_after_no", "instance.json"),
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
    "--input", os.path.join(ex, "onedimensional", "maximum_weight_after_yes", "instance.json"),
    "--certificate", os.path.join(ex, "onedimensional", "maximum_weight_after_yes", "solution.csv"),
    "--time-limit", "5",
], stdout=open(os.path.join(ex, "onedimensional", "maximum_weight_after_yes", "output.txt"), "w"), stderr=subprocess.STDOUT)
subprocess.run([
    sys.executable, os.path.join("scripts", "visualize_onedimensional.py"),
    os.path.join(ex, "onedimensional", "maximum_weight_after_yes", "solution.csv"),
    "--output", os.path.join(img, "onedimensional_maximum_weight_after_yes.png"),
    "--expand-copies",
])

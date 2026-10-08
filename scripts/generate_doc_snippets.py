"""Write the Python and C++ versions of the JSON instances of the documentation.

For each example listed in 'doc/examples/<problem type>/examples.json', writes
next to its 'instance.json':
- 'instance.py': the instance built with the Python package;
- 'instance.cpp': the instance built with the C++ library.

The documentation shows them in the tabs of its examples (see
'doc/_ext/example_tabs.py'), and 'python/tests/test_doc_examples.py' checks
that the Python versions build the same instances as the JSON files.

Usage:

    python3 scripts/generate_doc_snippets.py
"""

import json
import os

EXAMPLES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "doc", "examples")

# The alias of the Python module of each problem type.
ALIASES = {
    "rectangleguillotine": "psg",
    "rectangle": "psr",
    "box": "psb",
    "boxstacks": "psbs",
    "onedimensional": "pso",
    "irregular": "psi",
}


def camel_case(value):
    """'bin-packing-with-leftovers' -> 'BinPackingWithLeftovers'."""
    return "".join(word.upper() if len(word) <= 2 else word[0].upper() + word[1:]
                   for word in value.split("-"))


def python_value(value):
    return repr(value)


def cpp_value(value):
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value)


def python_call(function, positional, keywords):
    """A call, on several lines if it is long."""
    arguments = [str(value) for value in positional]
    arguments += [f"{key}={value}" for key, value in keywords]
    call = f"{function}({', '.join(arguments)})"
    if len(call) <= 79:
        return call
    return f"{function}(\n" + "".join(f"        {argument},\n" for argument in arguments) + ")"


class Snippet:
    """The lines of the Python and of the C++ versions of an instance."""

    def __init__(self, problem_type):
        self.alias = ALIASES[problem_type]
        self.python = [
            f"import packingsolver.{problem_type} as {self.alias}",
            "",
            f"instance_builder = {self.alias}.InstanceBuilder()",
        ]
        self.cpp = [
            f'#include "packingsolver/{problem_type}/instance_builder.hpp"',
            "",
            "using namespace packingsolver;",
            f"using namespace packingsolver::{problem_type};",
            "",
            "InstanceBuilder instance_builder;",
        ]
        # The C++ variables, declared at their first use.
        self.declared = set()

    def declare(self, declaration):
        """'Type name' the first time, 'name' then."""
        if declaration in self.declared:
            return declaration.split()[-1]
        self.declared.add(declaration)
        return declaration

    def objective(self, instance):
        objective = camel_case(instance["objective"])
        self.python.append(f"instance_builder.set_objective({self.alias}.Objective.{objective})")
        self.cpp.append(f"instance_builder.set_objective(Objective::{objective});")

    def setter(self, name, python_value_, cpp_value_):
        self.python.append(f"instance_builder.{name}({python_value_})")
        self.cpp.append(f"instance_builder.{name}({cpp_value_});")

    def end(self):
        self.python.append("instance = instance_builder.build()")
        self.cpp.append("Instance instance = instance_builder.build();")
        return "\n".join(self.python) + "\n", "\n".join(self.cpp) + "\n"


def add_type(snippet, kind, positional, python_keywords, cpp_setters,
             cpp_positional=None, after=None):
    """Add a bin type or an item type ('kind' "bin" or "item").

    'python_keywords': '(keyword, Python value)' pairs; 'cpp_setters':
    '(setter, C++ value)' pairs, called with the id of the type;
    'cpp_positional': the arguments of the C++ 'add_<kind>_type' (default:
    'positional'); 'after(python_lines, cpp_lines)': lines using the id of
    the type, after it is added (e.g. its defects).
    """
    if cpp_positional is None:
        cpp_positional = positional
    python_after = []
    cpp_after = []
    if after is not None:
        after(python_after, cpp_after)
    call = python_call(f"instance_builder.add_{kind}_type", positional, python_keywords)
    if python_after:
        snippet.python.append(f"{kind}_type_id = {call}")
        snippet.python += python_after
    else:
        snippet.python.append(call)
    call = f"instance_builder.add_{kind}_type({', '.join(str(v) for v in cpp_positional)});"
    if not cpp_setters and not cpp_after:
        snippet.cpp.append(call)
        return
    type_name = "BinTypeId" if kind == "bin" else "ItemTypeId"
    snippet.cpp.append(f"{snippet.declare(f'{type_name} {kind}_type_id')} = {call}")
    for setter, value in cpp_setters:
        snippet.cpp.append(f"instance_builder.{setter}({kind}_type_id, {value});")
    snippet.cpp += cpp_after


def optional_fields(json_type, fields, setters=None):
    """The optional fields of a type: Python keywords and C++ setters."""
    setters = setters or {}
    python = []
    cpp = []
    for field in fields:
        if field not in json_type:
            continue
        python.append((field, python_value(json_type[field])))
        cpp.append((setters.get(field, f"set_{{kind}}_type_{field}"), cpp_value(json_type[field])))
    return python, cpp


def with_kind(setters, kind):
    return [(setter.format(kind=kind), value) for setter, value in setters]


def rectangular_defects(json_bin_type):
    """The defects of a bin type of 'rectangle', 'rectangleguillotine' or
    'boxstacks'."""
    def after(python, cpp):
        for defect in json_bin_type.get("defects", []):
            arguments = "{}, {}, {}, {}".format(defect["x"], defect["y"], defect["width"], defect["height"])
            python.append(f"instance_builder.add_defect(bin_type_id, {arguments})")
            cpp.append(f"instance_builder.add_defect(bin_type_id, {arguments});")
    return after


###############################################################################
# rectangle
###############################################################################

def rectangle(instance):
    s = Snippet("rectangle")
    s.objective(instance)
    if "unloading_constraint" in instance:
        value = camel_case(instance["unloading_constraint"])
        s.python.append(f"instance_builder.set_unloading_constraint(psr.UnloadingConstraint.{value if value != 'None' else 'None_'})")
        s.cpp.append(f"instance_builder.set_unloading_constraint(UnloadingConstraint::{value});")
    if "leftover_mode" in instance:
        value = camel_case(instance["leftover_mode"])
        s.setter("set_leftover_mode", f"psr.LeftoverMode.{value}", f"LeftoverMode::{value}")
    for bin_type in instance["bin_types"]:
        python, cpp = optional_fields(bin_type, ["cost", "maximum_weight", "copies", "copies_min"])
        if "eligibility_ids" in bin_type:
            python.append(("eligibility_ids", python_value(bin_type["eligibility_ids"])))
            cpp += [("add_{kind}_type_eligibility", value) for value in bin_type["eligibility_ids"]]
        add_type(s, "bin", [bin_type["x"], bin_type["y"]], python, with_kind(cpp, "bin"),
                 after=rectangular_defects(bin_type))
    for item_type in instance["item_types"]:
        python, cpp = optional_fields(
                item_type, ["group_id", "weight", "eligibility_id", "profit", "copies", "copies_min"],
                {"group_id": "set_{kind}_type_group", "eligibility_id": "set_{kind}_type_eligibility"})
        positional = [item_type["x"], item_type["y"]]
        cpp_positional = list(positional)
        if item_type.get("oriented", False):
            python.insert(0, ("oriented", "True"))
            cpp_positional.append("true")
        add_type(s, "item", positional, python, with_kind(cpp, "item"), cpp_positional)
    return s.end()


###############################################################################
# rectangleguillotine
###############################################################################

TRIMS = [("left", "hard"), ("right", "soft"), ("bottom", "hard"), ("top", "soft")]


def rectangleguillotine(instance):
    s = Snippet("rectangleguillotine")
    s.objective(instance)
    if "number_of_stages" in instance:
        if instance["number_of_stages"] == "unlimited":
            s.setter("set_number_of_stages_unlimited", "", "")
        else:
            s.setter("set_number_of_stages", instance["number_of_stages"], instance["number_of_stages"])
    if "cut_type" in instance:
        value = camel_case(instance["cut_type"])
        s.setter("set_cut_type", f"psg.CutType.{value}", f"CutType::{value}")
    if "first_stage_orientation" in instance:
        value = camel_case(instance["first_stage_orientation"])
        s.setter("set_first_stage_orientation", f"psg.CutOrientation.{value}", f"CutOrientation::{value}")
    for key in ["minimum_distance_1_cuts", "maximum_distance_1_cuts",
                "minimum_distance_2_cuts", "maximum_distance_2_cuts",
                "minimum_waste_length", "maximum_number_1_cuts", "maximum_number_2_cuts",
                "cut_through_defects", "cut_thickness"]:
        if key in instance:
            s.setter(f"set_{key}", python_value(instance[key]), cpp_value(instance[key]))
    for bin_type in instance["bin_types"]:
        python, cpp = optional_fields(bin_type, ["cost", "copies", "copies_min"])
        trims = {f"{side}_trim": bin_type.get(f"{side}_trim", 0) for side, _ in TRIMS}
        trim_types = {f"{side}_trim_type": bin_type.get(f"{side}_trim_type", default) for side, default in TRIMS}
        with_trims = any(value != 0 for value in trims.values()) \
            or any(f"{side}_trim_type" in bin_type for side, _ in TRIMS)

        def after(python_lines, cpp_lines, bin_type=bin_type, trims=trims, trim_types=trim_types,
                  with_trims=with_trims):
            if with_trims:
                arguments = []
                for side, _ in TRIMS:
                    arguments += [str(trims[f"{side}_trim"]),
                                  "TrimType::" + camel_case(trim_types[f"{side}_trim_type"])]
                cpp_lines.append("instance_builder.add_trims(")
                cpp_lines.append("        bin_type_id,")
                for i in range(0, len(arguments), 2):
                    end = ");" if i + 2 == len(arguments) else ","
                    cpp_lines.append(f"        {arguments[i]}, {arguments[i + 1]}{end}")
            rectangular_defects(bin_type)(python_lines, cpp_lines)

        if with_trims:
            entries = []
            for side, default in TRIMS:
                if trims[f"{side}_trim"] != 0:
                    entries.append(f'"{side}_trim": {trims[f"{side}_trim"]}')
                if f"{side}_trim_type" in bin_type:
                    entries.append(f'"{side}_trim_type": psg.TrimType.{camel_case(bin_type[f"{side}_trim_type"])}')
            python.insert(0, ("trims", "{" + ", ".join(entries) + "}"))
        add_type(s, "bin", [bin_type["width"], bin_type["height"]], python, with_kind(cpp, "bin"), after=after)
    for item_type in instance["item_types"]:
        python, cpp = optional_fields(item_type, ["profit", "copies", "copies_min"])
        positional = [item_type["width"], item_type["height"]]
        cpp_positional = list(positional)
        oriented = item_type.get("oriented", False)
        stack_id = item_type.get("stack_id", -1)
        if stack_id != -1:
            python.insert(0, ("stack_id", str(stack_id)))
            cpp_positional += [cpp_value(oriented), str(stack_id)]
        elif oriented:
            cpp_positional.append("true")
        if oriented:
            python.insert(0, ("oriented", "True"))
        add_type(s, "item", positional, python, with_kind(cpp, "item"), cpp_positional)
    return s.end()


###############################################################################
# box and boxstacks
###############################################################################

def rotations(snippet, item_type, rotation_type):
    """The rotations of an item type: Python keyword, C++ lines."""
    if "rotations" not in item_type:
        return None, []
    python = "[" + ", ".join(f"{snippet.alias}.Rotation.{r}" for r in item_type["rotations"]) + "]"
    cpp = [f"instance_builder.add_item_type_rotation(item_type_id, {rotation_type}::{r});"
           for r in item_type["rotations"]]
    return python, cpp


def box(instance):
    s = Snippet("box")
    s.objective(instance)
    for bin_type in instance["bin_types"]:
        python, cpp = optional_fields(bin_type, ["cost", "maximum_weight", "copies", "copies_min"])
        add_type(s, "bin", [bin_type["x"], bin_type["y"], bin_type["z"]], python, with_kind(cpp, "bin"))
    for item_type in instance["item_types"]:
        python, cpp = optional_fields(item_type, ["weight", "profit", "copies", "copies_min"])
        python_rotations, cpp_rotations = rotations(s, item_type, "Rotation")
        if python_rotations is not None:
            python.insert(0, ("rotations", python_rotations))
        add_type(s, "item", [item_type["x"], item_type["y"], item_type["z"]], python,
                 with_kind(cpp, "item"), after=lambda p, c, r=cpp_rotations: c.extend(r))
    return s.end()


def boxstacks(instance):
    s = Snippet("boxstacks")
    s.objective(instance)
    if "unloading_constraint" in instance:
        value = camel_case(instance["unloading_constraint"])
        s.python.append(f"instance_builder.set_unloading_constraint(psbs.UnloadingConstraint.{value if value != 'None' else 'None_'})")
        s.cpp.append(f"instance_builder.set_unloading_constraint(rectangle::UnloadingConstraint::{value});")
    for group_id in instance.get("no_check_weight_constraints", []):
        s.python.append(f"instance_builder.set_group_weight_constraints({group_id}, False)")
        s.cpp.append(f"instance_builder.set_group_weight_constraints({group_id}, false);")
    for bin_type in instance["bin_types"]:
        python, cpp = optional_fields(
                bin_type, ["cost", "maximum_weight", "maximum_stack_density", "copies", "copies_min"])
        truck = bin_type.get("semi_trailer_truck")

        def after(python_lines, cpp_lines, bin_type=bin_type, truck=truck):
            if truck is not None:
                variable = s.declare("SemiTrailerTruckData semi_trailer_truck_data")
                if variable != "semi_trailer_truck_data":
                    cpp_lines.append(f"{variable};")
                else:
                    cpp_lines.append("semi_trailer_truck_data = SemiTrailerTruckData();")
                cpp_lines.append("semi_trailer_truck_data.is = true;")
                for key, value in truck.items():
                    cpp_lines.append(f"semi_trailer_truck_data.{key} = {cpp_value(value)};")
                cpp_lines.append("instance_builder.set_bin_type_semi_trailer_truck_parameters("
                                 "bin_type_id, semi_trailer_truck_data);")
            rectangular_defects(bin_type)(python_lines, cpp_lines)

        if truck is not None:
            python.append(("semi_trailer_truck_parameters", "{\n" + "".join(
                f'            "{key}": {python_value(value)},\n' for key, value in truck.items()) + "        }"))
        add_type(s, "bin", [bin_type["x"], bin_type["y"], bin_type["z"]], python,
                 with_kind(cpp, "bin"), after=after)
    for item_type in instance["item_types"]:
        python, cpp = optional_fields(
                item_type, ["group_id", "weight", "stackability_id", "nesting_height",
                            "maximum_stackability", "maximum_weight_above", "profit", "copies", "copies_min"],
                {"group_id": "set_{kind}_type_group"})
        python_rotations, cpp_rotations = rotations(s, item_type, "Rotation")
        if python_rotations is not None:
            python.insert(0, ("rotations", python_rotations))
        add_type(s, "item", [item_type["x"], item_type["y"], item_type["z"]], python,
                 with_kind(cpp, "item"), after=lambda p, c, r=cpp_rotations: c.extend(r))
    return s.end()


###############################################################################
# onedimensional
###############################################################################

def onedimensional(instance):
    s = Snippet("onedimensional")
    s.objective(instance)
    for bin_type in instance["bin_types"]:
        python, cpp = optional_fields(bin_type, ["cost", "maximum_weight", "copies", "copies_min"])
        if "eligibility_ids" in bin_type:
            python.append(("eligibility_ids", python_value(bin_type["eligibility_ids"])))
            cpp += [("add_{kind}_type_eligibility", value) for value in bin_type["eligibility_ids"]]
        add_type(s, "bin", [bin_type["length"]], python, with_kind(cpp, "bin"))
    for item_type in instance["item_types"]:
        python, cpp = optional_fields(
                item_type, ["weight", "nesting_length", "maximum_stackability", "maximum_weight_after",
                            "eligibility_id", "profit", "copies", "copies_min"],
                {"eligibility_id": "set_{kind}_type_eligibility"})
        add_type(s, "item", [item_type["length"]], python, with_kind(cpp, "item"))
    return s.end()


###############################################################################
# irregular
###############################################################################

def number(value):
    """A coordinate: an integer if it is one."""
    return str(int(value)) if float(value).is_integer() else str(value)


def irregular_shape(json_shape, python_module, cpp_namespace):
    """A shape of the JSON format ('rectangle', 'polygon' or 'circle'), built
    in Python ('python_module' "psi") or in C++ ('cpp_namespace' "shape")."""
    shape_type = json_shape["type"]
    if shape_type == "rectangle":
        x = json_shape.get("x", 0)
        y = json_shape.get("y", 0)
        arguments = ", ".join(number(v) for v in [
            x, x + json_shape["width"], y, y + json_shape["height"]])
        return (f"{python_module}.build_rectangle({arguments})",
                f"{cpp_namespace}::build_rectangle({arguments})")
    if shape_type == "polygon":
        points = [(number(v["x"]), number(v["y"])) for v in json_shape["vertices"]]
        python = "[" + ", ".join(f"({x}, {y})" for x, y in points) + "]"
        cpp = "{" + ", ".join(f"{{{x}, {y}}}" for x, y in points) + "}"
        return (f"{python_module}.build_shape({python})",
                f"{cpp_namespace}::build_shape({cpp})")
    raise ValueError(f"shape type '{shape_type}' isn't supported by the snippets")


def irregular_shape_with_holes(json_shape):
    """Python, C++ ('ShapeWithHoles')."""
    python_shape, cpp_shape = irregular_shape(json_shape, "psi", "shape")
    holes = [irregular_shape(hole, "psi", "shape") for hole in json_shape.get("holes", [])]
    if not holes:
        return python_shape, cpp_shape, False
    python = f"psi.ShapeWithHoles({python_shape}, [{', '.join(h[0] for h in holes)}])"
    cpp = f"shape::ShapeWithHoles{{{cpp_shape}, {{{', '.join(h[1] for h in holes)}}}}}"
    return python, cpp, True


def irregular(instance):
    s = Snippet("irregular")
    s.objective(instance)
    parameters = instance.get("parameters", {})
    for key in ["item_item_minimum_spacing", "open_dimension_xy_aspect_ratio"]:
        if key in parameters:
            s.setter(f"set_{key}", python_value(parameters[key]), cpp_value(parameters[key]))
    if "leftover_mode" in parameters:
        value = camel_case(parameters["leftover_mode"])
        s.setter("set_leftover_mode", f"psi.LeftoverMode.{value}", f"LeftoverMode::{value}")
    for bin_type in instance["bin_types"]:
        python_shape, cpp_shape = irregular_shape(bin_type, "psi", "shape")
        python, cpp = optional_fields(bin_type, ["cost", "item_bin_minimum_spacing", "copies", "copies_min"],
                                      {"item_bin_minimum_spacing": "set_item_bin_minimum_spacing"})

        def after(python_lines, cpp_lines, bin_type=bin_type):
            for defect in bin_type.get("defects", []):
                python_defect, cpp_defect, with_holes = irregular_shape_with_holes(defect)
                if not with_holes:
                    python_defect = f"psi.ShapeWithHoles({python_defect})"
                    cpp_defect = f"shape::ShapeWithHoles{{{cpp_defect}}}"
                defect_type = defect.get("defect_type", -1)
                spacing = defect.get("item_defect_minimum_spacing")
                python_call_ = f"instance_builder.add_defect(bin_type_id, {defect_type}, {python_defect})"
                cpp_call = f"instance_builder.add_defect(bin_type_id, {defect_type}, {cpp_defect});"
                if spacing is None:
                    python_lines.append(python_call_)
                    cpp_lines.append(cpp_call)
                else:
                    python_lines.append(f"defect_id = {python_call_}")
                    python_lines.append(
                            f"instance_builder.set_item_defect_minimum_spacing(bin_type_id, defect_id, {spacing})")
                    cpp_lines.append(f"{s.declare('DefectId defect_id')} = {cpp_call}")
                    cpp_lines.append(
                            f"instance_builder.set_item_defect_minimum_spacing(bin_type_id, defect_id, {spacing});")

        add_type(s, "bin", [python_shape], python, with_kind(cpp, "bin"), [cpp_shape], after=after)
    for item_type in instance["item_types"]:
        json_shapes = item_type.get("shapes", [item_type])
        shapes = [irregular_shape_with_holes(json_shape) for json_shape in json_shapes]
        if len(shapes) == 1:
            python_shapes = shapes[0][0]
        else:
            python_shapes = "[" + ", ".join(f"psi.ItemShape({p if h else f'psi.ShapeWithHoles({p})'})"
                                            for p, _, h in shapes) + "]"
        cpp_shapes = "{" + ", ".join(
            f"ItemShape{{{c if h else f'shape::ShapeWithHoles{{{c}}}'}}}" for _, c, h in shapes) + "}"
        python, cpp = optional_fields(item_type, ["profit", "copies", "copies_min"])
        rotations_ = item_type.get("allowed_rotations")
        cpp_rotations = []
        if rotations_ is not None:
            python.insert(0, ("allowed_rotations", "[" + ", ".join(
                f"({number(r['start'])}, {number(r['end'])}, {r.get('mirror', False)})"
                for r in rotations_) + "]"))
            cpp_rotations = [
                f"instance_builder.add_item_type_allowed_rotation(item_type_id, "
                f"{number(r['start'])}, {number(r['end'])}, {cpp_value(r.get('mirror', False))});"
                for r in rotations_]
        add_type(s, "item", [python_shapes], python, with_kind(cpp, "item"), [cpp_shapes],
                 after=lambda p, c, r=cpp_rotations: c.extend(r))
    return s.end()


SNIPPETS = {
    "rectangleguillotine": rectangleguillotine,
    "rectangle": rectangle,
    "box": box,
    "boxstacks": boxstacks,
    "onedimensional": onedimensional,
    "irregular": irregular,
}


def examples():
    """The examples of the documentation: '(problem type, directory)'."""
    for problem_type in sorted(SNIPPETS):
        list_path = os.path.join(EXAMPLES, problem_type, "examples.json")
        if not os.path.isfile(list_path):
            continue
        with open(list_path) as f:
            for example in json.load(f):
                yield problem_type, os.path.join(EXAMPLES, problem_type, example["name"])


def main():
    for problem_type, directory in examples():
        with open(os.path.join(directory, "instance.json")) as f:
            instance = json.load(f)
        python, cpp = SNIPPETS[problem_type](instance)
        with open(os.path.join(directory, "instance.py"), "w") as f:
            f.write(python)
        with open(os.path.join(directory, "instance.cpp"), "w") as f:
            f.write(cpp)
        print(os.path.relpath(directory, EXAMPLES))


if __name__ == "__main__":
    main()

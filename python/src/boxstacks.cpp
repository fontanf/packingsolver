#include "common.hpp"

#include "packingsolver/boxstacks/instance_builder.hpp"
#include "packingsolver/boxstacks/optimize.hpp"

using namespace packingsolver;
using namespace packingsolver::python;
using namespace packingsolver::boxstacks;

namespace
{

/**
 * Build a 'SemiTrailerTruckData' from a dict of its fields; missing fields
 * keep their default values.
 */
SemiTrailerTruckData semi_trailer_truck_data_from_dict(const nb::dict& parameters)
{
    SemiTrailerTruckData semi_trailer_truck_data;
    semi_trailer_truck_data.is = true;
    for (auto item: parameters) {
        std::string key = nb::cast<std::string>(item.first);
        if (key == "tractor_weight") {
            semi_trailer_truck_data.tractor_weight = nb::cast<Weight>(item.second);
        } else if (key == "front_axle_middle_axle_distance") {
            semi_trailer_truck_data.front_axle_middle_axle_distance = nb::cast<Length>(item.second);
        } else if (key == "front_axle_tractor_gravity_center_distance") {
            semi_trailer_truck_data.front_axle_tractor_gravity_center_distance = nb::cast<Length>(item.second);
        } else if (key == "front_axle_harness_distance") {
            semi_trailer_truck_data.front_axle_harness_distance = nb::cast<Length>(item.second);
        } else if (key == "empty_trailer_weight") {
            semi_trailer_truck_data.empty_trailer_weight = nb::cast<Weight>(item.second);
        } else if (key == "harness_rear_axle_distance") {
            semi_trailer_truck_data.harness_rear_axle_distance = nb::cast<Length>(item.second);
        } else if (key == "trailer_gravity_center_rear_axle_distance") {
            semi_trailer_truck_data.trailer_gravity_center_rear_axle_distance = nb::cast<Length>(item.second);
        } else if (key == "trailer_start_harness_distance") {
            semi_trailer_truck_data.trailer_start_harness_distance = nb::cast<Length>(item.second);
        } else if (key == "rear_axle_maximum_weight") {
            semi_trailer_truck_data.rear_axle_maximum_weight = nb::cast<Weight>(item.second);
        } else if (key == "middle_axle_maximum_weight") {
            semi_trailer_truck_data.middle_axle_maximum_weight = nb::cast<Weight>(item.second);
        } else {
            throw nb::value_error(
                    ("unknown semi-trailer truck parameter '" + key + "'.").c_str());
        }
    }
    return semi_trailer_truck_data;
}

}

void bind_boxstacks(nb::module_& m)
{
    using Handle = OutputHandle<Instance, boxstacks::Output>;

    /*
     * Enums
     */

    // 'Direction' and 'UnloadingConstraint' are the 'rectangle' ones; they
    // are bound in 'bind_rectangle'.
    nb::enum_<Rotation>(m, "Rotation")
        .value("XYZ", Rotation::XYZ)
        .value("YXZ", Rotation::YXZ)
        .value("ZYX", Rotation::ZYX)
        .value("YZX", Rotation::YZX)
        .value("XZY", Rotation::XZY)
        .value("ZXY", Rotation::ZXY);

    /*
     * Instance
     */

    nb::class_<Group>(m, "Group")
        .def_ro("item_types", &Group::item_types)
        .def_ro("number_of_items", &Group::number_of_items);

    nb::class_<ItemType>(m, "ItemType")
        .def_prop_ro("x", [](const ItemType& item_type) { return item_type.box.x; })
        .def_prop_ro("y", [](const ItemType& item_type) { return item_type.box.y; })
        .def_prop_ro("z", [](const ItemType& item_type) { return item_type.box.z; })
        .def_ro("profit", &ItemType::profit)
        .def_ro("copies", &ItemType::copies)
        .def_ro("copies_min", &ItemType::copies_min)
        .def_ro("copies_fixed", &ItemType::copies_fixed)
        .def_ro("group_id", &ItemType::group_id)
        .def_ro("rotations", &ItemType::rotations)
        .def_ro("weight", &ItemType::weight)
        .def_ro("stackability_id", &ItemType::stackability_id)
        .def_ro("nesting_height", &ItemType::nesting_height)
        .def_ro("maximum_stackability", &ItemType::maximum_stackability)
        .def_ro("maximum_weight_above", &ItemType::maximum_weight_above)
        .def("volume", &ItemType::volume)
        .def("area", &ItemType::area)
        .def("can_rotate", &ItemType::can_rotate, nb::arg("rotation"));

    // 'defects' holds 'rectangle.Defect' objects, bound in 'bind_rectangle'.
    nb::class_<BinType>(m, "BinType")
        .def_prop_ro("x", [](const BinType& bin_type) { return bin_type.box.x; })
        .def_prop_ro("y", [](const BinType& bin_type) { return bin_type.box.y; })
        .def_prop_ro("z", [](const BinType& bin_type) { return bin_type.box.z; })
        .def_ro("cost", &BinType::cost)
        .def_ro("copies", &BinType::copies)
        .def_ro("copies_min", &BinType::copies_min)
        .def_ro("defects", &BinType::defects)
        .def_ro("maximum_weight", &BinType::maximum_weight)
        .def_ro("maximum_stack_density", &BinType::maximum_stack_density)
        .def("area", &BinType::area)
        .def("volume", &BinType::volume);

    nb::class_<Instance>(m, "Instance")
        .def("objective", &Instance::objective)
        .def("unloading_constraint", &Instance::unloading_constraint)
        .def("check_weight_constraints",
                [](const Instance& instance, GroupId group_id) {
                    if (group_id < 0)
                        throw nb::index_error("group_id out of range.");
                    return instance.check_weight_constraints(group_id);
                },
                nb::arg("group_id"))
        .def("weight_tolerance", &Instance::weight_tolerance)
        .def("number_of_bin_types", &Instance::number_of_bin_types)
        .def("bin_type",
                [](const Instance& instance, BinTypeId bin_type_id) -> const BinType& {
                    check_index(bin_type_id, instance.number_of_bin_types(), "bin_type_id");
                    return instance.bin_type(bin_type_id);
                },
                nb::arg("bin_type_id"),
                nb::rv_policy::reference_internal)
        .def("number_of_bins", &Instance::number_of_bins)
        .def("bin_type_id",
                [](const Instance& instance, BinPos bin_pos) {
                    check_index(bin_pos, instance.number_of_bins(), "bin_pos");
                    return instance.bin_type_id(bin_pos);
                },
                nb::arg("bin_pos"))
        .def("bin_volume", &Instance::bin_volume)
        .def("bin_area", &Instance::bin_area)
        .def("bin_weight", &Instance::bin_weight)
        .def("number_of_defects", &Instance::number_of_defects)
        .def("previous_bin_volume",
                [](const Instance& instance, BinPos bin_pos) {
                    check_index(bin_pos, instance.number_of_bins(), "bin_pos");
                    return instance.previous_bin_volume(bin_pos);
                },
                nb::arg("bin_pos"))
        .def("number_of_item_types", &Instance::number_of_item_types)
        .def("item_type",
                [](const Instance& instance, ItemTypeId item_type_id) -> const ItemType& {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    return instance.item_type(item_type_id);
                },
                nb::arg("item_type_id"),
                nb::rv_policy::reference_internal)
        .def("number_of_items", &Instance::number_of_items)
        .def("number_of_groups", &Instance::number_of_groups)
        .def("group",
                [](const Instance& instance, GroupId group_id) -> const Group& {
                    check_index(group_id, instance.number_of_groups(), "group_id");
                    return instance.group(group_id);
                },
                nb::arg("group_id"),
                nb::rv_policy::reference_internal)
        .def("item_volume", &Instance::item_volume)
        .def("mean_volume", &Instance::mean_volume)
        .def("item_weight", &Instance::item_weight)
        .def("item_profit", &Instance::item_profit)
        .def("largest_efficiency_item_type_id", &Instance::largest_efficiency_item_type_id)
        .def("unbounded_knapsack", &Instance::unbounded_knapsack)
        .def("fits_some_bin",
                [](const Instance& instance, ItemTypeId item_type_id) {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    return instance.fits_some_bin(item_type_id);
                },
                nb::arg("item_type_id"))
        .def("write", &Instance::write,
                nb::arg("instance_path"),
                nb::arg("format") = InstanceFormat::Csv,
                "Write the instance: a JSON file with 'InstanceFormat.Json', or with "
                "'InstanceFormat.Csv' (default) '<instance_path>_items.csv', "
                "'_bins.csv', '_defects.csv' (if there are defects) and "
                "'_parameters.csv'.")
        .def("write_item_types", &Instance::write_item_types, nb::arg("items_path"))
        .def("write_bin_types", &Instance::write_bin_types, nb::arg("bins_path"))
        .def("write_defects", &Instance::write_defects, nb::arg("defects_path"))
        .def("write_parameters", &Instance::write_parameters, nb::arg("parameters_path"))
        .def("format",
                [](const Instance& instance, int verbosity_level) {
                    std::stringstream ss;
                    instance.format(ss, verbosity_level);
                    return ss.str();
                },
                nb::arg("verbosity_level") = 1);

    nb::class_<InstanceBuilder>(m, "InstanceBuilder")
        .def(nb::init<>())
        .def("set_objective", &InstanceBuilder::set_objective, nb::arg("objective"))
        .def("read", &InstanceBuilder::read, nb::arg("instance_path"),
                "Read a full instance from a JSON file.")
        .def("read_parameters", &InstanceBuilder::read_parameters, nb::arg("parameters_path"))
        .def("read_bin_types", &InstanceBuilder::read_bin_types, nb::arg("bins_path"))
        .def("read_defects", &InstanceBuilder::read_defects, nb::arg("defects_path"))
        .def("read_item_types", &InstanceBuilder::read_item_types, nb::arg("items_path"))
        .def("set_weight_tolerance", &InstanceBuilder::set_weight_tolerance, nb::arg("weight_tolerance"))
        .def("set_group_weight_constraints", &InstanceBuilder::set_group_weight_constraints,
                nb::arg("group_id"), nb::arg("check_weight_constraints"))
        .def("set_unloading_constraint", &InstanceBuilder::set_unloading_constraint, nb::arg("unloading_constraint"))
        // Bin types.
        .def("add_bin_type",
                [](InstanceBuilder& instance_builder,
                    Length x,
                    Length y,
                    Length z,
                    std::optional<Profit> cost,
                    std::optional<Weight> maximum_weight,
                    std::optional<double> maximum_stack_density,
                    const std::optional<nb::dict>& semi_trailer_truck_parameters,
                    std::optional<BinPos> copies,
                    std::optional<BinPos> copies_min) {
                    BinTypeId bin_type_id = instance_builder.add_bin_type(x, y, z);
                    if (cost.has_value())
                        instance_builder.set_bin_type_cost(bin_type_id, *cost);
                    if (maximum_weight.has_value())
                        instance_builder.set_bin_type_maximum_weight(bin_type_id, *maximum_weight);
                    if (maximum_stack_density.has_value())
                        instance_builder.set_bin_type_maximum_stack_density(bin_type_id, *maximum_stack_density);
                    if (semi_trailer_truck_parameters.has_value()) {
                        instance_builder.set_bin_type_semi_trailer_truck_parameters(
                                bin_type_id,
                                semi_trailer_truck_data_from_dict(*semi_trailer_truck_parameters));
                    }
                    if (copies.has_value())
                        instance_builder.set_bin_type_copies(bin_type_id, *copies);
                    if (copies_min.has_value())
                        instance_builder.set_bin_type_copies_min(bin_type_id, *copies_min);
                    return bin_type_id;
                },
                nb::arg("x"), nb::arg("y"), nb::arg("z"),
                nb::kw_only(),
                nb::arg("cost") = nb::none(),
                nb::arg("maximum_weight") = nb::none(),
                nb::arg("maximum_stack_density") = nb::none(),
                nb::arg("semi_trailer_truck_parameters") = nb::none(),
                nb::arg("copies") = nb::none(),
                nb::arg("copies_min") = nb::none(),
                "Add a bin type and return its id.\n"
                "\n"
                "Keyword arguments (omitted ones keep the C++ defaults):\n"
                "- cost: the cost of the bin type.\n"
                "- maximum_weight: the maximum weight of the bin type.\n"
                "- maximum_stack_density: the maximum stack density of the bin type.\n"
                "- semi_trailer_truck_parameters: make the bin type a semi-trailer\n"
                "  truck subject to axle weight constraints; a dict whose keys are\n"
                "  among 'tractor_weight', 'front_axle_middle_axle_distance',\n"
                "  'front_axle_tractor_gravity_center_distance',\n"
                "  'front_axle_harness_distance', 'empty_trailer_weight',\n"
                "  'harness_rear_axle_distance',\n"
                "  'trailer_gravity_center_rear_axle_distance',\n"
                "  'trailer_start_harness_distance', 'rear_axle_maximum_weight' and\n"
                "  'middle_axle_maximum_weight' (missing keys default to 0, and to\n"
                "  infinity for the two axle maximum weights);\n"
                "  'build()' requires strictly positive\n"
                "  'front_axle_middle_axle_distance' and\n"
                "  'harness_rear_axle_distance'.\n"
                "- copies: the number of copies of the bin type.\n"
                "- copies_min: the minimum number of copies of the bin type.")
        .def("add_defect", &InstanceBuilder::add_defect,
                nb::arg("bin_type_id"), nb::arg("x"), nb::arg("y"), nb::arg("w"), nb::arg("h"))
        .def("set_bin_types_infinite_x", &InstanceBuilder::set_bin_types_infinite_x)
        .def("set_bin_types_infinite_y", &InstanceBuilder::set_bin_types_infinite_y)
        .def("set_bin_types_infinite_copies", &InstanceBuilder::set_bin_types_infinite_copies)
        .def("set_bin_types_unweighted", &InstanceBuilder::set_bin_types_unweighted)
        // Item types.
        .def("add_item_type",
                [](InstanceBuilder& instance_builder,
                    Length x,
                    Length y,
                    Length z,
                    const std::optional<std::vector<Rotation>>& rotations,
                    std::optional<GroupId> group_id,
                    std::optional<Weight> weight,
                    std::optional<StackabilityId> stackability_id,
                    std::optional<Length> nesting_height,
                    std::optional<ItemPos> maximum_stackability,
                    std::optional<Weight> maximum_weight_above,
                    std::optional<Profit> profit,
                    std::optional<ItemPos> copies,
                    std::optional<ItemPos> copies_min) {
                    ItemTypeId item_type_id = instance_builder.add_item_type(x, y, z);
                    if (rotations.has_value())
                        for (Rotation rotation: *rotations)
                            instance_builder.add_item_type_rotation(item_type_id, rotation);
                    if (group_id.has_value())
                        instance_builder.set_item_type_group(item_type_id, *group_id);
                    if (weight.has_value())
                        instance_builder.set_item_type_weight(item_type_id, *weight);
                    if (stackability_id.has_value())
                        instance_builder.set_item_type_stackability_id(item_type_id, *stackability_id);
                    if (nesting_height.has_value())
                        instance_builder.set_item_type_nesting_height(item_type_id, *nesting_height);
                    if (maximum_stackability.has_value())
                        instance_builder.set_item_type_maximum_stackability(item_type_id, *maximum_stackability);
                    if (maximum_weight_above.has_value())
                        instance_builder.set_item_type_maximum_weight_above(item_type_id, *maximum_weight_above);
                    if (profit.has_value())
                        instance_builder.set_item_type_profit(item_type_id, *profit);
                    if (copies.has_value())
                        instance_builder.set_item_type_copies(item_type_id, *copies);
                    if (copies_min.has_value())
                        instance_builder.set_item_type_copies_min(item_type_id, *copies_min);
                    return item_type_id;
                },
                nb::arg("x"), nb::arg("y"), nb::arg("z"),
                nb::kw_only(),
                nb::arg("rotations") = nb::none(),
                nb::arg("group_id") = nb::none(),
                nb::arg("weight") = nb::none(),
                nb::arg("stackability_id") = nb::none(),
                nb::arg("nesting_height") = nb::none(),
                nb::arg("maximum_stackability") = nb::none(),
                nb::arg("maximum_weight_above") = nb::none(),
                nb::arg("profit") = nb::none(),
                nb::arg("copies") = nb::none(),
                nb::arg("copies_min") = nb::none(),
                "Add an item type and return its id.\n"
                "\n"
                "Keyword arguments (omitted ones keep the C++ defaults):\n"
                "- rotations: the allowed rotations of the item type; if omitted\n"
                "  or empty, only 'Rotation.XYZ' is allowed.\n"
                "- group_id: the group of the item type.\n"
                "- weight: the weight of the item type.\n"
                "- stackability_id: the stackability id of the item type.\n"
                "- nesting_height: the nesting height of the item type.\n"
                "- maximum_stackability: the maximum stackability of the item type.\n"
                "- maximum_weight_above: the maximum weight above of the item type.\n"
                "- profit: the profit of the item type (default: its volume).\n"
                "- copies: the number of copies of the item type.\n"
                "- copies_min: the minimum number of copies to pack of the item type.")
        .def("set_item_types_profits_auto", &InstanceBuilder::set_item_types_profits_auto)
        .def("set_item_types_infinite_copies", &InstanceBuilder::set_item_types_infinite_copies)
        .def("set_item_types_unweighted", &InstanceBuilder::set_item_types_unweighted)
        .def("set_item_types_oriented", &InstanceBuilder::set_item_types_oriented)
        .def("build",
                [](InstanceBuilder& instance_builder) {
                    std::shared_ptr<Instance> instance = std::make_shared<Instance>(instance_builder.build());
                    // 'build' moves the instance out of the builder.
                    instance_builder = InstanceBuilder();
                    return instance;
                },
                "Build the instance; the builder is reset afterwards.");

    /*
     * Solution
     */

    nb::class_<SolutionItem>(m, "SolutionItem")
        .def_ro("item_type_id", &SolutionItem::item_type_id)
        .def_ro("z_start", &SolutionItem::z_start)
        .def_ro("rotation", &SolutionItem::rotation);

    nb::class_<SolutionStack>(m, "SolutionStack")
        .def_ro("x_start", &SolutionStack::x_start)
        .def_ro("x_end", &SolutionStack::x_end)
        .def_ro("y_start", &SolutionStack::y_start)
        .def_ro("y_end", &SolutionStack::y_end)
        .def_ro("z_end", &SolutionStack::z_end)
        .def_ro("items", &SolutionStack::items)
        .def_ro("weight", &SolutionStack::weight)
        .def_ro("weight_weighted_sum", &SolutionStack::weight_weighted_sum)
        .def_ro("profit", &SolutionStack::profit);

    nb::class_<SolutionBin>(m, "SolutionBin")
        .def_ro("bin_type_id", &SolutionBin::bin_type_id)
        .def_ro("copies", &SolutionBin::copies)
        .def_ro("stacks", &SolutionBin::stacks)
        .def_ro("weight", &SolutionBin::weight)
        .def_ro("weight_weighted_sum", &SolutionBin::weight_weighted_sum)
        .def_ro("profit", &SolutionBin::profit);

    nb::class_<Solution>(m, "Solution")
        .def("feasible", &Solution::feasible)
        .def("callback_feasible", &Solution::callback_feasible)
        .def("total_weight_feasible", &Solution::total_weight_feasible)
        .def("axle_weights_feasible", &Solution::axle_weights_feasible)
        .def("item_copies_feasible", &Solution::item_copies_feasible)
        .def("full", &Solution::full)
        .def("number_of_bins", &Solution::number_of_bins)
        .def("number_of_different_bins", &Solution::number_of_different_bins)
        .def("bin",
                [](const Solution& solution, BinPos bin_pos) -> const SolutionBin& {
                    check_index(bin_pos, solution.number_of_different_bins(), "bin_pos");
                    return solution.bin(bin_pos);
                },
                nb::arg("bin_pos"),
                nb::rv_policy::reference_internal)
        .def("bin_copies",
                [](const Solution& solution, BinTypeId bin_type_id) {
                    check_index(bin_type_id, solution.instance().number_of_bin_types(), "bin_type_id");
                    return solution.bin_copies(bin_type_id);
                },
                nb::arg("bin_type_id"))
        .def("cost", &Solution::cost)
        .def("bin_volume", &Solution::bin_volume)
        .def("bin_area", &Solution::bin_area)
        .def("bin_weight", &Solution::bin_weight)
        .def("number_of_stacks", &Solution::number_of_stacks)
        .def("stack_area", &Solution::stack_area)
        .def("number_of_items", &Solution::number_of_items)
        .def("item_copies",
                [](const Solution& solution, ItemTypeId item_type_id) {
                    check_index(item_type_id, solution.instance().number_of_item_types(), "item_type_id");
                    return solution.item_copies(item_type_id);
                },
                nb::arg("item_type_id"))
        .def("number_of_infeasible_item_copies_min", &Solution::number_of_infeasible_item_copies_min)
        .def("profit", &Solution::profit)
        .def("item_volume", &Solution::item_volume)
        .def("item_weight", &Solution::item_weight)
        .def("x_max", &Solution::x_max)
        .def("y_max", &Solution::y_max)
        .def("volume", &Solution::volume)
        .def("volume_load", &Solution::volume_load)
        .def("area_load", &Solution::area_load)
        .def("weight_load", &Solution::weight_load)
        .def("item_fraction", &Solution::item_fraction)
        .def("volume_fraction", &Solution::volume_fraction)
        .def("weight_fraction", &Solution::weight_fraction)
        .def("waste", &Solution::waste)
        .def("waste_percentage", &Solution::waste_percentage)
        .def("full_waste", &Solution::full_waste)
        .def("full_waste_percentage", &Solution::full_waste_percentage)
        .def("compute_weight_constraints_violation",
                nb::overload_cast<>(&Solution::compute_weight_constraints_violation, nb::const_))
        .def("compute_middle_axle_weight_constraints_violation",
                nb::overload_cast<>(&Solution::compute_middle_axle_weight_constraints_violation, nb::const_))
        .def("compute_rear_axle_weight_constraints_violation",
                nb::overload_cast<>(&Solution::compute_rear_axle_weight_constraints_violation, nb::const_))
        .def("write", &solution_write_path<Solution>, nb::arg("certificate_path"),
                "Write the solution certificate to a file.")
        .def("write", &solution_write_stream<Solution>, nb::arg("stream"),
                "Write the solution certificate to a text stream (e.g. 'io.StringIO').")
        .def("to_json", [](const Solution& solution) { return json_to_python(solution.to_json()); });

    /*
     * Optimize
     */

    nb::class_<ReductionParameters>(m, "ReductionParameters")
        .def(nb::init<>())
        .def_rw("reduce", &ReductionParameters::reduce)
        .def_rw("remove_negative_profit_items", &ReductionParameters::remove_negative_profit_items)
        .def_rw("merge_identical_items", &ReductionParameters::merge_identical_items);

    nb::class_<OptimizeParameters> parameters = bind_parameters_base<boxstacks::Output, OptimizeParameters>(m);
    parameters
        .def_rw("optimization_mode", &OptimizeParameters::optimization_mode)
        .def_rw("memory_limit_megabytes", &OptimizeParameters::memory_limit_megabytes)
        .def_rw("reduction_parameters", &OptimizeParameters::reduction_parameters)
        .def_rw("linear_programming_solver_name", &OptimizeParameters::linear_programming_solver_name)
        .def_rw("use_box_bounds", &OptimizeParameters::use_box_bounds)
        .def_rw("use_sequential_single_knapsack", &OptimizeParameters::use_sequential_single_knapsack)
        .def_rw("use_sequential_value_correction", &OptimizeParameters::use_sequential_value_correction)
        .def_rw("tree_search_guides", &OptimizeParameters::tree_search_guides)
        .def_rw("many_items_in_bins_threshold", &OptimizeParameters::many_items_in_bins_threshold)
        .def_rw("many_items_in_bins_threshold_2", &OptimizeParameters::many_items_in_bins_threshold_2)
        .def_rw("many_item_type_copies_factor", &OptimizeParameters::many_item_type_copies_factor)
        .def_rw("sequential_value_correction_subproblem_tree_search_queue_size", &OptimizeParameters::sequential_value_correction_subproblem_tree_search_queue_size)
        .def_rw("column_generation_subproblem_tree_search_queue_size", &OptimizeParameters::column_generation_subproblem_tree_search_queue_size)
        .def_rw("anytime_tree_search_initial_queue_size", &OptimizeParameters::anytime_tree_search_initial_queue_size)
        .def_rw("anytime_sequential_onedimensional_rectangle_rectangle_initial_queue_size", &OptimizeParameters::anytime_sequential_onedimensional_rectangle_rectangle_initial_queue_size)
        .def_rw("not_anytime_tree_search_queue_size", &OptimizeParameters::not_anytime_tree_search_queue_size)
        .def_rw("not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size", &OptimizeParameters::not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size)
        .def_rw("not_anytime_sequential_single_knapsack_subproblem_rectangle_tree_search_queue_size", &OptimizeParameters::not_anytime_sequential_single_knapsack_subproblem_rectangle_tree_search_queue_size)
        .def_rw("not_anytime_sequential_onedimensional_rectangle_rectangle_tree_search_queue_size", &OptimizeParameters::not_anytime_sequential_onedimensional_rectangle_rectangle_tree_search_queue_size)
        .def_rw("not_anytime_sequential_value_correction_number_of_iterations", &OptimizeParameters::not_anytime_sequential_value_correction_number_of_iterations);

    nb::class_<Handle> output = bind_output<Instance, Solution, boxstacks::Output>(m);
    def_output_field(output, "knapsack_bound", &boxstacks::Output::knapsack_bound);
    def_output_field(output, "bin_packing_bound", &boxstacks::Output::bin_packing_bound);
    def_output_field(output, "variable_sized_bin_packing_bound", &boxstacks::Output::variable_sized_bin_packing_bound);
    def_output_field(output, "is_proven_infeasible", &boxstacks::Output::is_proven_infeasible);
    // Statistics.
    def_output_field(output, "sequential_onedimensional_rectangle_number_of_items", &boxstacks::Output::sequential_onedimensional_rectangle_number_of_items);
    def_output_field(output, "sequential_onedimensional_rectangle_profit", &boxstacks::Output::sequential_onedimensional_rectangle_profit);
    def_output_field(output, "sequential_onedimensional_rectangle_time", &boxstacks::Output::sequential_onedimensional_rectangle_time);
    def_output_field(output, "sequential_onedimensional_rectangle_onedimensional_time", &boxstacks::Output::sequential_onedimensional_rectangle_onedimensional_time);
    def_output_field(output, "sequential_onedimensional_rectangle_rectangle_time", &boxstacks::Output::sequential_onedimensional_rectangle_rectangle_time);
    def_output_field(output, "sequential_onedimensional_rectangle_failed", &boxstacks::Output::sequential_onedimensional_rectangle_failed);
    def_output_field(output, "tree_search_time", &boxstacks::Output::tree_search_time);
    def_output_field(output, "number_of_sequential_onedimensional_rectangle_calls", &boxstacks::Output::number_of_sequential_onedimensional_rectangle_calls);
    def_output_field(output, "number_of_sequential_onedimensional_rectangle_perfect", &boxstacks::Output::number_of_sequential_onedimensional_rectangle_perfect);
    def_output_field(output, "number_of_sequential_onedimensional_rectangle_good", &boxstacks::Output::number_of_sequential_onedimensional_rectangle_good);
    def_output_field(output, "number_of_tree_search_calls", &boxstacks::Output::number_of_tree_search_calls);
    def_output_field(output, "number_of_tree_search_perfect", &boxstacks::Output::number_of_tree_search_perfect);
    def_output_field(output, "number_of_tree_search_better", &boxstacks::Output::number_of_tree_search_better);

    m.def("optimize",
            [](std::shared_ptr<Instance> instance, const OptimizeParameters& parameters) {
                return run_optimize<Instance, boxstacks::Output>(
                        instance,
                        parameters,
                        [](const Instance& instance, const OptimizeParameters& parameters) {
                            return boxstacks::optimize(instance, parameters);
                        });
            },
            nb::arg("instance"),
            nb::arg("parameters") = default_optimize_parameters<OptimizeParameters>());
}

#include "common.hpp"

#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/optimize.hpp"

using namespace packingsolver;
using namespace packingsolver::python;
using namespace packingsolver::box;

void bind_box(nb::module_& m)
{
    using Handle = OutputHandle<Instance, box::Output>;

    /*
     * Enums
     */

    nb::enum_<Direction>(m, "Direction")
        .value("X", Direction::X)
        .value("Y", Direction::Y)
        .value("Z", Direction::Z)
        .value("Any", Direction::Any);

    // Each name gives where the item's (x, y, z) dimensions end up; e.g.
    // 'XYZ' is the identity and 'YXZ' swaps x and y.
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

    nb::class_<ItemType>(m, "ItemType")
        .def_prop_ro("x", [](const ItemType& item_type) { return item_type.box.x; })
        .def_prop_ro("y", [](const ItemType& item_type) { return item_type.box.y; })
        .def_prop_ro("z", [](const ItemType& item_type) { return item_type.box.z; })
        .def_ro("profit", &ItemType::profit)
        .def_ro("copies", &ItemType::copies)
        .def_ro("copies_min", &ItemType::copies_min)
        .def_ro("rotations", &ItemType::rotations)
        .def_ro("weight", &ItemType::weight)
        .def_ro("copies_fixed", &ItemType::copies_fixed)
        .def("volume", &ItemType::volume)
        .def("area", &ItemType::area)
        .def("can_rotate", &ItemType::can_rotate, nb::arg("rotation"));

    nb::class_<BinType>(m, "BinType")
        .def_prop_ro("x", [](const BinType& bin_type) { return bin_type.box.x; })
        .def_prop_ro("y", [](const BinType& bin_type) { return bin_type.box.y; })
        .def_prop_ro("z", [](const BinType& bin_type) { return bin_type.box.z; })
        .def_ro("cost", &BinType::cost)
        .def_ro("copies", &BinType::copies)
        .def_ro("copies_min", &BinType::copies_min)
        .def_ro("maximum_weight", &BinType::maximum_weight)
        .def("area", &BinType::area)
        .def("volume", &BinType::volume)
        .def("number_of_resources", &BinType::number_of_resources);

    nb::class_<Instance>(m, "Instance")
        .def("objective", &Instance::objective)
        .def("weight_tolerance", &Instance::weight_tolerance)
        // Bin types.
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
        .def("largest_bin_cost", &Instance::largest_bin_cost)
        // Item types.
        .def("number_of_item_types", &Instance::number_of_item_types)
        .def("item_type",
                [](const Instance& instance, ItemTypeId item_type_id) -> const ItemType& {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    return instance.item_type(item_type_id);
                },
                nb::arg("item_type_id"),
                nb::rv_policy::reference_internal)
        .def("number_of_items", &Instance::number_of_items)
        .def("item_volume", &Instance::item_volume)
        .def("item_weight", &Instance::item_weight)
        .def("item_profit", &Instance::item_profit)
        .def("largest_item_profit", &Instance::largest_item_profit)
        .def("largest_efficiency_item_type_id", &Instance::largest_efficiency_item_type_id)
        .def("smallest_item_x", &Instance::smallest_item_x)
        .def("smallest_item_y", &Instance::smallest_item_y)
        .def("smallest_item_z", &Instance::smallest_item_z)
        .def("largest_item_copies", &Instance::largest_item_copies)
        .def("unbounded_knapsack", &Instance::unbounded_knapsack)
        .def("fits_some_bin",
                [](const Instance& instance, ItemTypeId item_type_id) {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    return instance.fits_some_bin(item_type_id);
                },
                nb::arg("item_type_id"))
        .def("resources_matter", &Instance::resources_matter)
        // Export.
        .def("write", &Instance::write,
                nb::arg("instance_path"),
                nb::arg("format") = InstanceFormat::Csv)
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
        .def("read", &InstanceBuilder::read, nb::arg("instance_path"))
        .def("read_parameters", &InstanceBuilder::read_parameters, nb::arg("parameters_path"))
        .def("read_bin_types", &InstanceBuilder::read_bin_types, nb::arg("bins_path"))
        .def("read_item_types", &InstanceBuilder::read_item_types, nb::arg("items_path"))
        .def("set_weight_tolerance", &InstanceBuilder::set_weight_tolerance, nb::arg("weight_tolerance"))
        // Bin types.
        .def("add_bin_type",
                [](InstanceBuilder& instance_builder,
                    Length x,
                    Length y,
                    Length z,
                    std::optional<Profit> cost,
                    std::optional<Weight> maximum_weight,
                    std::optional<BinPos> copies,
                    std::optional<BinPos> copies_min)
                {
                    BinTypeId bin_type_id = instance_builder.add_bin_type(x, y, z);
                    if (cost.has_value())
                        instance_builder.set_bin_type_cost(bin_type_id, *cost);
                    if (maximum_weight.has_value())
                        instance_builder.set_bin_type_maximum_weight(bin_type_id, *maximum_weight);
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
                nb::arg("copies") = nb::none(),
                nb::arg("copies_min") = nb::none(),
                "Add a bin type and return its id.\n\n"
                "Keyword arguments (omitted ones keep the C++ default):\n"
                "- cost: the cost of the bin type.\n"
                "- maximum_weight: the maximum weight of the bin type.\n"
                "- copies: the number of copies of the bin type.\n"
                "- copies_min: the minimum number of copies of the bin type.")
        .def("add_bin_type_resource", &InstanceBuilder::add_bin_type_resource,
                nb::arg("bin_type_id"), nb::arg("capacity"), nb::arg("penalize") = false, nb::arg("penalty") = 0.0)
        .def("add_resource_consumption", &InstanceBuilder::add_resource_consumption,
                nb::arg("bin_type_id"), nb::arg("resource_id"), nb::arg("item_type_id"), nb::arg("schedule"))
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
                    std::optional<std::vector<Rotation>> rotations,
                    std::optional<Weight> weight,
                    std::optional<Profit> profit,
                    std::optional<ItemPos> copies,
                    std::optional<ItemPos> copies_min)
                {
                    ItemTypeId item_type_id = instance_builder.add_item_type(x, y, z);
                    if (rotations.has_value())
                        for (Rotation rotation: *rotations)
                            instance_builder.add_item_type_rotation(item_type_id, rotation);
                    if (weight.has_value())
                        instance_builder.set_item_type_weight(item_type_id, *weight);
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
                nb::arg("weight") = nb::none(),
                nb::arg("profit") = nb::none(),
                nb::arg("copies") = nb::none(),
                nb::arg("copies_min") = nb::none(),
                "Add an item type and return its id.\n\n"
                "Keyword arguments (omitted ones keep the C++ default):\n"
                "- rotations: the allowed rotations of the item type (the full\n"
                "  list; when omitted or empty, only 'Rotation.XYZ' is allowed).\n"
                "- weight: the weight of the item type.\n"
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
        .def_prop_ro("x", [](const SolutionItem& item) { return item.bl_corner.x; })
        .def_prop_ro("y", [](const SolutionItem& item) { return item.bl_corner.y; })
        .def_prop_ro("z", [](const SolutionItem& item) { return item.bl_corner.z; })
        .def_ro("rotation", &SolutionItem::rotation);

    nb::class_<SolutionBin>(m, "SolutionBin")
        .def_ro("bin_type_id", &SolutionBin::bin_type_id)
        .def_ro("copies", &SolutionBin::copies)
        .def_ro("items", &SolutionBin::items)
        .def_ro("weight", &SolutionBin::weight)
        .def_ro("profit", &SolutionBin::profit)
        .def_ro("resource_consumption", &SolutionBin::resource_consumption);

    nb::class_<Solution>(m, "Solution")
        // Feasibility.
        .def("callback_feasible", &Solution::callback_feasible)
        .def("feasible", &Solution::feasible)
        .def("resource_feasible", &Solution::resource_feasible)
        // Bins.
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
        // Stacks.
        .def("number_of_stacks", &Solution::number_of_stacks)
        .def("stack_area", &Solution::stack_area)
        // Items.
        .def("number_of_items", &Solution::number_of_items)
        .def("full", &Solution::full)
        .def("item_volume", &Solution::item_volume)
        .def("item_weight", &Solution::item_weight)
        .def("profit", &Solution::profit)
        .def("item_copies",
                [](const Solution& solution, ItemTypeId item_type_id) {
                    check_index(item_type_id, solution.instance().number_of_item_types(), "item_type_id");
                    return solution.item_copies(item_type_id);
                },
                nb::arg("item_type_id"))
        .def("number_of_infeasible_item_copies_min", &Solution::number_of_infeasible_item_copies_min)
        .def("item_copies_feasible", &Solution::item_copies_feasible)
        // Others.
        .def("x_max", &Solution::x_max)
        .def("y_max", &Solution::y_max)
        .def("z_max", &Solution::z_max)
        .def("leftover_value", &Solution::leftover_value)
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
        // Export.
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

    nb::class_<OptimizeParameters> parameters = bind_parameters_base<box::Output, OptimizeParameters>(m);
    parameters
        .def_rw("optimization_mode", &OptimizeParameters::optimization_mode)
        .def_rw("memory_limit_megabytes", &OptimizeParameters::memory_limit_megabytes)
        .def_rw("reduction_parameters", &OptimizeParameters::reduction_parameters)
        .def_rw("linear_programming_solver_name", &OptimizeParameters::linear_programming_solver_name)
        .def_rw("use_dual_feasible_functions", &OptimizeParameters::use_dual_feasible_functions)
        .def_rw("use_tree_search", &OptimizeParameters::use_tree_search)
        .def_rw("use_tree_search_maximal_spaces", &OptimizeParameters::use_tree_search_maximal_spaces)
        .def_rw("use_sequential_single_knapsack", &OptimizeParameters::use_sequential_single_knapsack)
        .def_rw("use_sequential_value_correction", &OptimizeParameters::use_sequential_value_correction)
        .def_rw("use_dichotomic_search", &OptimizeParameters::use_dichotomic_search)
        .def_rw("use_column_generation", &OptimizeParameters::use_column_generation)
        .def_rw("tree_search_guides", &OptimizeParameters::tree_search_guides)
        .def_rw("many_items_in_bins_threshold", &OptimizeParameters::many_items_in_bins_threshold)
        .def_rw("many_items_in_bins_threshold_2", &OptimizeParameters::many_items_in_bins_threshold_2)
        .def_rw("many_item_type_copies_factor", &OptimizeParameters::many_item_type_copies_factor)
        .def_rw("sequential_value_correction_subproblem_tree_search_queue_size", &OptimizeParameters::sequential_value_correction_subproblem_tree_search_queue_size)
        .def_rw("sequential_value_correction_subproblem_tree_search_maximal_spaces_queue_size", &OptimizeParameters::sequential_value_correction_subproblem_tree_search_maximal_spaces_queue_size)
        .def_rw("column_generation_subproblem_tree_search_queue_size", &OptimizeParameters::column_generation_subproblem_tree_search_queue_size)
        .def_rw("column_generation_subproblem_tree_search_maximal_spaces_queue_size", &OptimizeParameters::column_generation_subproblem_tree_search_maximal_spaces_queue_size)
        .def_rw("not_anytime_tree_search_queue_size", &OptimizeParameters::not_anytime_tree_search_queue_size)
        .def_rw("not_anytime_tree_search_maximal_spaces_queue_size", &OptimizeParameters::not_anytime_tree_search_maximal_spaces_queue_size)
        .def_rw("not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size", &OptimizeParameters::not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size)
        .def_rw("not_anytime_sequential_single_knapsack_subproblem_tree_search_maximal_spaces_queue_size", &OptimizeParameters::not_anytime_sequential_single_knapsack_subproblem_tree_search_maximal_spaces_queue_size)
        .def_rw("not_anytime_sequential_value_correction_number_of_iterations", &OptimizeParameters::not_anytime_sequential_value_correction_number_of_iterations)
        .def_rw("not_anytime_dichotomic_search_subproblem_tree_search_queue_size", &OptimizeParameters::not_anytime_dichotomic_search_subproblem_tree_search_queue_size);

    nb::class_<Handle> output = bind_output<Instance, Solution, box::Output>(m);
    def_output_field(output, "knapsack_bound", &box::Output::knapsack_bound);
    def_output_field(output, "bin_packing_bound", &box::Output::bin_packing_bound);
    def_output_field(output, "variable_sized_bin_packing_bound", &box::Output::variable_sized_bin_packing_bound);
    def_output_field(output, "open_dimension_x_bound", &box::Output::open_dimension_x_bound);
    def_output_field(output, "open_dimension_y_bound", &box::Output::open_dimension_y_bound);
    def_output_field(output, "open_dimension_z_bound", &box::Output::open_dimension_z_bound);
    def_output_field(output, "is_proven_infeasible", &box::Output::is_proven_infeasible);

    m.def("optimize",
            [](std::shared_ptr<Instance> instance, const OptimizeParameters& parameters) {
                return run_optimize<Instance, box::Output>(
                        instance,
                        parameters,
                        [](const Instance& instance, const OptimizeParameters& parameters) {
                            return box::optimize(instance, parameters);
                        });
            },
            nb::arg("instance"),
            nb::arg("parameters") = default_optimize_parameters<OptimizeParameters>());
}

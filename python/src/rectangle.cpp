#include "common.hpp"

#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"

using namespace packingsolver;
using namespace packingsolver::python;
using namespace packingsolver::rectangle;

void bind_rectangle(nb::module_& m)
{
    using Handle = OutputHandle<Instance, rectangle::Output>;

    /*
     * Enums
     */

    nb::enum_<Direction>(m, "Direction")
        .value("X", Direction::X)
        .value("Y", Direction::Y)
        .value("Any", Direction::Any);

    nb::enum_<LeftoverMode>(m, "LeftoverMode")
        .value("Area", LeftoverMode::Area)
        .value("X", LeftoverMode::X)
        .value("Y", LeftoverMode::Y);

    // 'None' is a Python keyword.
    nb::enum_<UnloadingConstraint>(m, "UnloadingConstraint")
        .value("None_", UnloadingConstraint::None)
        .value("OnlyXMovements", UnloadingConstraint::OnlyXMovements)
        .value("OnlyYMovements", UnloadingConstraint::OnlyYMovements)
        .value("IncreasingX", UnloadingConstraint::IncreasingX)
        .value("IncreasingY", UnloadingConstraint::IncreasingY);

    /*
     * Instance
     */

    nb::class_<ItemType>(m, "ItemType")
        .def_prop_ro("x", [](const ItemType& item_type) { return item_type.rect.x; })
        .def_prop_ro("y", [](const ItemType& item_type) { return item_type.rect.y; })
        .def_ro("profit", &ItemType::profit)
        .def_ro("copies", &ItemType::copies)
        .def_ro("copies_min", &ItemType::copies_min)
        .def_ro("group_id", &ItemType::group_id)
        .def_ro("oriented", &ItemType::oriented)
        .def_ro("weight", &ItemType::weight)
        .def_ro("eligibility_id", &ItemType::eligibility_id);

    nb::class_<Defect>(m, "Defect")
        .def_ro("bin_type_id", &Defect::bin_type_id)
        .def_prop_ro("x", [](const Defect& defect) { return defect.pos.x; })
        .def_prop_ro("y", [](const Defect& defect) { return defect.pos.y; })
        .def_prop_ro("width", [](const Defect& defect) { return defect.rect.x; })
        .def_prop_ro("height", [](const Defect& defect) { return defect.rect.y; });

    nb::class_<BinType>(m, "BinType")
        .def_prop_ro("x", [](const BinType& bin_type) { return bin_type.rect.x; })
        .def_prop_ro("y", [](const BinType& bin_type) { return bin_type.rect.y; })
        .def_ro("cost", &BinType::cost)
        .def_ro("copies", &BinType::copies)
        .def_ro("copies_min", &BinType::copies_min)
        .def_ro("maximum_weight", &BinType::maximum_weight)
        .def_ro("eligibility_ids", &BinType::eligibility_ids)
        .def_ro("defects", &BinType::defects);

    nb::class_<Instance>(m, "Instance")
        .def("objective", &Instance::objective)
        .def("number_of_item_types", &Instance::number_of_item_types)
        .def("item_type",
                [](const Instance& instance, ItemTypeId item_type_id) -> const ItemType& {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    return instance.item_type(item_type_id);
                },
                nb::arg("item_type_id"),
                nb::rv_policy::reference_internal)
        .def("number_of_items", &Instance::number_of_items)
        .def("number_of_bin_types", &Instance::number_of_bin_types)
        .def("bin_type",
                [](const Instance& instance, BinTypeId bin_type_id) -> const BinType& {
                    check_index(bin_type_id, instance.number_of_bin_types(), "bin_type_id");
                    return instance.bin_type(bin_type_id);
                },
                nb::arg("bin_type_id"),
                nb::rv_policy::reference_internal)
        .def("number_of_bins", &Instance::number_of_bins)
        .def("number_of_defects", &Instance::number_of_defects)
        .def("item_area", &Instance::item_area)
        .def("bin_area", &Instance::bin_area)
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
        .def("read", &instance_builder_read_path<InstanceBuilder>, nb::arg("instance_path"),
                "Read an instance from a JSON file.")
        .def("read", &instance_builder_read_stream<InstanceBuilder>, nb::arg("stream"),
                "Read an instance in the JSON format from a text stream (e.g. 'io.StringIO').")
        .def("read_parameters", &InstanceBuilder::read_parameters, nb::arg("parameters_path"))
        .def("read_bin_types", &InstanceBuilder::read_bin_types, nb::arg("bins_path"))
        .def("read_defects", &InstanceBuilder::read_defects, nb::arg("defects_path"))
        .def("read_item_types", &InstanceBuilder::read_item_types, nb::arg("items_path"))
        .def("set_weight_tolerance", &InstanceBuilder::set_weight_tolerance, nb::arg("weight_tolerance"))
        .def("set_group_weight_constraints", &InstanceBuilder::set_group_weight_constraints,
                nb::arg("group_id"), nb::arg("check_weight_constraints"))
        .def("set_unloading_constraint", &InstanceBuilder::set_unloading_constraint, nb::arg("unloading_constraint"))
        .def("set_leftover_mode", &InstanceBuilder::set_leftover_mode, nb::arg("leftover_mode"))
        // Bin types.
        .def("add_bin_type",
                [](InstanceBuilder& instance_builder,
                    Length x,
                    Length y,
                    std::optional<Profit> cost,
                    std::optional<Weight> maximum_weight,
                    std::optional<std::vector<EligibilityId>> eligibility_ids,
                    std::optional<BinPos> copies,
                    std::optional<BinPos> copies_min)
                {
                    BinTypeId bin_type_id = instance_builder.add_bin_type(x, y);
                    if (cost.has_value())
                        instance_builder.set_bin_type_cost(bin_type_id, *cost);
                    if (maximum_weight.has_value())
                        instance_builder.set_bin_type_maximum_weight(bin_type_id, *maximum_weight);
                    if (eligibility_ids.has_value())
                        for (EligibilityId eligibility_id: *eligibility_ids)
                            instance_builder.add_bin_type_eligibility(bin_type_id, eligibility_id);
                    if (copies.has_value())
                        instance_builder.set_bin_type_copies(bin_type_id, *copies);
                    if (copies_min.has_value())
                        instance_builder.set_bin_type_copies_min(bin_type_id, *copies_min);
                    return bin_type_id;
                },
                nb::arg("x"),
                nb::arg("y"),
                nb::kw_only(),
                nb::arg("cost") = nb::none(),
                nb::arg("maximum_weight") = nb::none(),
                nb::arg("eligibility_ids") = nb::none(),
                nb::arg("copies") = nb::none(),
                nb::arg("copies_min") = nb::none(),
                "Add a bin type of dimensions 'x' x 'y' and return its id.\n"
                "\n"
                "Optional keyword arguments (omitted ones keep the C++ default):\n"
                "- cost: the cost of the bin type (default: its area; -1 also means its area);\n"
                "- maximum_weight: the maximum weight of the bin type;\n"
                "- eligibility_ids: the eligibility ids supported by the bin type (list);\n"
                "- copies: the number of copies of the bin type (-1 for infinite; default: 1);\n"
                "- copies_min: the minimum number of copies of the bin type (default: 0).")
        .def("add_defect", &InstanceBuilder::add_defect,
                nb::arg("bin_type_id"), nb::arg("pos_x"), nb::arg("pos_y"), nb::arg("rect_x"), nb::arg("rect_y"))
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
                    bool oriented,
                    std::optional<GroupId> group_id,
                    std::optional<Weight> weight,
                    std::optional<EligibilityId> eligibility_id,
                    std::optional<Profit> profit,
                    std::optional<ItemPos> copies,
                    std::optional<ItemPos> copies_min)
                {
                    ItemTypeId item_type_id = instance_builder.add_item_type(x, y, oriented);
                    if (group_id.has_value())
                        instance_builder.set_item_type_group(item_type_id, *group_id);
                    if (weight.has_value())
                        instance_builder.set_item_type_weight(item_type_id, *weight);
                    if (eligibility_id.has_value())
                        instance_builder.set_item_type_eligibility(item_type_id, *eligibility_id);
                    if (profit.has_value())
                        instance_builder.set_item_type_profit(item_type_id, *profit);
                    if (copies.has_value())
                        instance_builder.set_item_type_copies(item_type_id, *copies);
                    if (copies_min.has_value())
                        instance_builder.set_item_type_copies_min(item_type_id, *copies_min);
                    return item_type_id;
                },
                nb::arg("x"),
                nb::arg("y"),
                nb::kw_only(),
                nb::arg("oriented") = false,
                nb::arg("group_id") = nb::none(),
                nb::arg("weight") = nb::none(),
                nb::arg("eligibility_id") = nb::none(),
                nb::arg("profit") = nb::none(),
                nb::arg("copies") = nb::none(),
                nb::arg("copies_min") = nb::none(),
                "Add an item type of dimensions 'x' x 'y' and return its id.\n"
                "\n"
                "Optional keyword arguments (omitted ones keep the C++ default):\n"
                "- oriented: if True, the item type can't be rotated (default: False);\n"
                "- group_id: the group of the item type (default: 0);\n"
                "- weight: the weight of the item type;\n"
                "- eligibility_id: the eligibility id of the item type (default: -1, any bin type);\n"
                "- profit: the profit of the item type (default: its area);\n"
                "- copies: the number of copies of the item type (-1 for infinite; default: 1);\n"
                "- copies_min: the minimum number of copies to pack of the item type\n"
                "  (default: resolved in 'build()': 0 for Knapsack, 'copies' otherwise).")
        .def("set_item_types_infinite_copies", &InstanceBuilder::set_item_types_infinite_copies)
        .def("multiply_item_types_copies", &InstanceBuilder::multiply_item_types_copies, nb::arg("factor"))
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
        .def_ro("rotate", &SolutionItem::rotate);

    nb::class_<SolutionBin>(m, "SolutionBin")
        .def_ro("bin_type_id", &SolutionBin::bin_type_id)
        .def_ro("copies", &SolutionBin::copies)
        .def_ro("items", &SolutionBin::items);

    nb::class_<Solution>(m, "Solution")
        .def("feasible", &Solution::feasible)
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
        .def("number_of_items", &Solution::number_of_items)
        .def("item_copies",
                [](const Solution& solution, ItemTypeId item_type_id) {
                    check_index(item_type_id, solution.instance().number_of_item_types(), "item_type_id");
                    return solution.item_copies(item_type_id);
                },
                nb::arg("item_type_id"))
        .def("profit", &Solution::profit)
        .def("item_area", &Solution::item_area)
        .def("item_weight", &Solution::item_weight)
        .def("x_max", &Solution::x_max)
        .def("y_max", &Solution::y_max)
        .def("leftover_value", &Solution::leftover_value)
        .def("waste", &Solution::waste)
        .def("waste_percentage", &Solution::waste_percentage)
        .def("full_waste", &Solution::full_waste)
        .def("full_waste_percentage", &Solution::full_waste_percentage)
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
        .def_rw("enlarge_wide_tall_items", &ReductionParameters::enlarge_wide_tall_items)
        .def_rw("enlarge_both_items", &ReductionParameters::enlarge_both_items)
        .def_rw("reduce_full_bin_items", &ReductionParameters::reduce_full_bin_items)
        .def_rw("reduce_perfect_pairs", &ReductionParameters::reduce_perfect_pairs)
        .def_rw("merge_identical_items", &ReductionParameters::merge_identical_items)
        .def_rw("lift_item_dimensions", &ReductionParameters::lift_item_dimensions)
        .def_rw("remove_negative_profit_items", &ReductionParameters::remove_negative_profit_items)
        .def_rw("remove_dominated_items", &ReductionParameters::remove_dominated_items)
        .def_rw("remove_dominated_bin_types", &ReductionParameters::remove_dominated_bin_types)
        .def_rw("maximum_number_of_rounds", &ReductionParameters::maximum_number_of_rounds)
        .def_rw("subproblem_queue_size", &ReductionParameters::subproblem_queue_size);

    nb::class_<OptimizeParameters> parameters = bind_parameters_base<rectangle::Output, OptimizeParameters>(m);
    parameters
        .def_rw("optimization_mode", &OptimizeParameters::optimization_mode)
        .def_rw("memory_limit_megabytes", &OptimizeParameters::memory_limit_megabytes)
        .def_rw("reduction_parameters", &OptimizeParameters::reduction_parameters)
        .def_rw("linear_programming_solver_name", &OptimizeParameters::linear_programming_solver_name)
        .def_rw("use_dual_feasible_functions", &OptimizeParameters::use_dual_feasible_functions)
        .def_rw("use_conservative_scales", &OptimizeParameters::use_conservative_scales)
        .def_rw("use_bar_relaxation", &OptimizeParameters::use_bar_relaxation)
        .def_rw("use_tree_search", &OptimizeParameters::use_tree_search)
        .def_rw("use_tree_search_maximal_spaces", &OptimizeParameters::use_tree_search_maximal_spaces)
        .def_rw("use_sequential_single_knapsack", &OptimizeParameters::use_sequential_single_knapsack)
        .def_rw("use_sequential_value_correction", &OptimizeParameters::use_sequential_value_correction)
        .def_rw("use_dichotomic_search", &OptimizeParameters::use_dichotomic_search)
        .def_rw("use_column_generation", &OptimizeParameters::use_column_generation)
        .def_rw("use_benders_decomposition", &OptimizeParameters::use_benders_decomposition)
        .def_rw("use_benders_decomposition_contiguity", &OptimizeParameters::use_benders_decomposition_contiguity)
        .def_rw("tree_search_guides", &OptimizeParameters::tree_search_guides)
        .def_rw("tree_search_directions", &OptimizeParameters::tree_search_directions)
        .def_rw("many_items_in_bins_threshold", &OptimizeParameters::many_items_in_bins_threshold)
        .def_rw("many_items_in_section_threshold", &OptimizeParameters::many_items_in_section_threshold)
        .def_rw("many_item_type_copies_factor", &OptimizeParameters::many_item_type_copies_factor)
        .def_rw("sequential_value_correction_subproblem_tree_search_queue_size", &OptimizeParameters::sequential_value_correction_subproblem_tree_search_queue_size)
        .def_rw("sequential_value_correction_subproblem_tree_search_maximal_spaces_queue_size", &OptimizeParameters::sequential_value_correction_subproblem_tree_search_maximal_spaces_queue_size)
        .def_rw("column_generation_subproblem_tree_search_queue_size", &OptimizeParameters::column_generation_subproblem_tree_search_queue_size)
        .def_rw("column_generation_subproblem_tree_search_maximal_spaces_queue_size", &OptimizeParameters::column_generation_subproblem_tree_search_maximal_spaces_queue_size)
        .def_rw("benders_decomposition_subproblem_tree_search_queue_size", &OptimizeParameters::benders_decomposition_subproblem_tree_search_queue_size)
        .def_rw("not_anytime_tree_search_queue_size", &OptimizeParameters::not_anytime_tree_search_queue_size)
        .def_rw("not_anytime_tree_search_maximal_spaces_queue_size", &OptimizeParameters::not_anytime_tree_search_maximal_spaces_queue_size)
        .def_rw("not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size", &OptimizeParameters::not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size)
        .def_rw("not_anytime_sequential_single_knapsack_subproblem_tree_search_maximal_spaces_queue_size", &OptimizeParameters::not_anytime_sequential_single_knapsack_subproblem_tree_search_maximal_spaces_queue_size)
        .def_rw("not_anytime_sequential_value_correction_number_of_iterations", &OptimizeParameters::not_anytime_sequential_value_correction_number_of_iterations)
        .def_rw("not_anytime_benders_decomposition_number_of_iterations", &OptimizeParameters::not_anytime_benders_decomposition_number_of_iterations)
        .def_rw("not_anytime_benders_decomposition_contiguity_number_of_iterations", &OptimizeParameters::not_anytime_benders_decomposition_contiguity_number_of_iterations)
        .def_rw("not_anytime_dichotomic_search_subproblem_tree_search_queue_size", &OptimizeParameters::not_anytime_dichotomic_search_subproblem_tree_search_queue_size);

    nb::class_<Handle> output = bind_output<Instance, Solution, rectangle::Output>(m);
    def_output_field(output, "knapsack_bound", &rectangle::Output::knapsack_bound);
    def_output_field(output, "bin_packing_bound", &rectangle::Output::bin_packing_bound);
    def_output_field(output, "variable_sized_bin_packing_bound", &rectangle::Output::variable_sized_bin_packing_bound);
    def_output_field(output, "open_dimension_x_bound", &rectangle::Output::open_dimension_x_bound);
    def_output_field(output, "open_dimension_y_bound", &rectangle::Output::open_dimension_y_bound);
    def_output_field(output, "is_proven_infeasible", &rectangle::Output::is_proven_infeasible);

    m.def("optimize",
            [](std::shared_ptr<Instance> instance, const OptimizeParameters& parameters) {
                return run_optimize<Instance, rectangle::Output>(
                        instance,
                        parameters,
                        [](const Instance& instance, const OptimizeParameters& parameters) {
                            return rectangle::optimize(instance, parameters);
                        });
            },
            nb::arg("instance"),
            nb::arg("parameters") = default_optimize_parameters<OptimizeParameters>());
}

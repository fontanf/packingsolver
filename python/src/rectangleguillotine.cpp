#include "common.hpp"

#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"

using namespace packingsolver;
using namespace packingsolver::python;
using namespace packingsolver::rectangleguillotine;

void bind_rectangleguillotine(nb::module_& m)
{
    using Handle = OutputHandle<Instance, rectangleguillotine::Output>;

    /*
     * Enums
     */

    nb::enum_<CutType>(m, "CutType")
        .value("Roadef2018", CutType::Roadef2018)
        .value("NonExact", CutType::NonExact)
        .value("Exact", CutType::Exact)
        .value("Homogenous", CutType::Homogenous);

    nb::enum_<CutOrientation>(m, "CutOrientation")
        .value("Horizontal", CutOrientation::Horizontal)
        .value("Vertical", CutOrientation::Vertical)
        .value("Any", CutOrientation::Any);

    nb::enum_<TrimType>(m, "TrimType")
        .value("Soft", TrimType::Soft)
        .value("Hard", TrimType::Hard);

    /*
     * Instance
     */

    nb::class_<rectangleguillotine::CutCost>(m, "CutCost")
        .def_ro("fixed", &rectangleguillotine::CutCost::fixed)
        .def_ro("variable", &rectangleguillotine::CutCost::variable);

    // 'packingsolver::Parameters' is also visible here, hence the explicit
    // namespace.
    nb::class_<rectangleguillotine::Parameters>(m, "Parameters")
        .def_ro("number_of_stages", &rectangleguillotine::Parameters::number_of_stages)
        .def_ro("cut_type", &rectangleguillotine::Parameters::cut_type)
        .def_ro("first_stage_orientation", &rectangleguillotine::Parameters::first_stage_orientation)
        .def_ro("minimum_distance_1_cuts", &rectangleguillotine::Parameters::minimum_distance_1_cuts)
        .def_ro("maximum_distance_1_cuts", &rectangleguillotine::Parameters::maximum_distance_1_cuts)
        .def_ro("minimum_distance_2_cuts", &rectangleguillotine::Parameters::minimum_distance_2_cuts)
        .def_ro("maximum_distance_2_cuts", &rectangleguillotine::Parameters::maximum_distance_2_cuts)
        .def_ro("minimum_waste_length", &rectangleguillotine::Parameters::minimum_waste_length)
        .def_ro("maximum_number_1_cuts", &rectangleguillotine::Parameters::maximum_number_1_cuts)
        .def_ro("maximum_number_2_cuts", &rectangleguillotine::Parameters::maximum_number_2_cuts)
        .def_ro("cut_through_defects", &rectangleguillotine::Parameters::cut_through_defects)
        .def_ro("cut_thickness", &rectangleguillotine::Parameters::cut_thickness)
        .def_ro("cutting_costs", &rectangleguillotine::Parameters::cutting_costs)
        .def_ro("waste_cost", &rectangleguillotine::Parameters::waste_cost);

    nb::class_<ItemType>(m, "ItemType")
        .def_prop_ro("w", [](const ItemType& item_type) { return item_type.rect.w; })
        .def_prop_ro("h", [](const ItemType& item_type) { return item_type.rect.h; })
        .def_ro("profit", &ItemType::profit)
        .def_ro("copies", &ItemType::copies)
        .def_ro("copies_min", &ItemType::copies_min)
        .def_ro("stack_id", &ItemType::stack_id)
        .def_ro("stack_pos", &ItemType::stack_pos)
        .def_ro("oriented", &ItemType::oriented)
        .def_ro("copies_fixed", &ItemType::copies_fixed)
        .def("width", &ItemType::width, nb::arg("rotate"))
        .def("height", &ItemType::height, nb::arg("rotate"))
        .def("area", &ItemType::area);

    nb::class_<Defect>(m, "Defect")
        .def_prop_ro("x", [](const Defect& defect) { return defect.pos.x; })
        .def_prop_ro("y", [](const Defect& defect) { return defect.pos.y; })
        .def_prop_ro("w", [](const Defect& defect) { return defect.rect.w; })
        .def_prop_ro("h", [](const Defect& defect) { return defect.rect.h; })
        .def("left", &Defect::left)
        .def("right", &Defect::right)
        .def("bottom", &Defect::bottom)
        .def("top", &Defect::top);

    nb::class_<BinType>(m, "BinType")
        .def_prop_ro("w", [](const BinType& bin_type) { return bin_type.rect.w; })
        .def_prop_ro("h", [](const BinType& bin_type) { return bin_type.rect.h; })
        .def_ro("cost", &BinType::cost)
        .def_ro("copies", &BinType::copies)
        .def_ro("copies_min", &BinType::copies_min)
        .def_ro("defects", &BinType::defects)
        .def_ro("bottom_trim", &BinType::bottom_trim)
        .def_ro("top_trim", &BinType::top_trim)
        .def_ro("left_trim", &BinType::left_trim)
        .def_ro("right_trim", &BinType::right_trim)
        .def_ro("bottom_trim_type", &BinType::bottom_trim_type)
        .def_ro("top_trim_type", &BinType::top_trim_type)
        .def_ro("left_trim_type", &BinType::left_trim_type)
        .def_ro("right_trim_type", &BinType::right_trim_type)
        .def("area", &BinType::area)
        .def("number_of_resources", &BinType::number_of_resources);

    nb::class_<Instance>(m, "Instance")
        .def("objective", &Instance::objective)
        .def("parameters", &Instance::parameters, nb::rv_policy::reference_internal)
        .def("number_of_stages_unlimited", &Instance::number_of_stages_unlimited)
        .def("number_of_bin_types", &Instance::number_of_bin_types)
        .def("bin_type",
                [](const Instance& instance, BinTypeId bin_type_id) -> const BinType& {
                    check_index(bin_type_id, instance.number_of_bin_types(), "bin_type_id");
                    return instance.bin_type(bin_type_id);
                },
                nb::arg("bin_type_id"),
                nb::rv_policy::reference_internal)
        .def("number_of_bins", &Instance::number_of_bins)
        .def("bin_area", &Instance::bin_area)
        .def("bin_type_id",
                [](const Instance& instance, BinPos bin_pos) {
                    check_index(bin_pos, instance.number_of_bins(), "bin_pos");
                    return instance.bin_type_id(bin_pos);
                },
                nb::arg("bin_pos"))
        .def("number_of_defects", &Instance::number_of_defects)
        .def("previous_bin_area",
                [](const Instance& instance, BinPos bin_pos) {
                    check_index(bin_pos, instance.number_of_bins(), "bin_pos");
                    return instance.previous_bin_area(bin_pos);
                },
                nb::arg("bin_pos"))
        .def("largest_bin_cost", &Instance::largest_bin_cost)
        .def("number_of_item_types", &Instance::number_of_item_types)
        .def("item_type",
                [](const Instance& instance, ItemTypeId item_type_id) -> const ItemType& {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    return instance.item_type(item_type_id);
                },
                nb::arg("item_type_id"),
                nb::rv_policy::reference_internal)
        .def("number_of_items", &Instance::number_of_items)
        .def("number_of_stacks", &Instance::number_of_stacks)
        .def("stack_size",
                [](const Instance& instance, StackId stack_id) {
                    check_index(stack_id, instance.number_of_stacks(), "stack_id");
                    return instance.stack_size(stack_id);
                },
                nb::arg("stack_id"))
        .def("item",
                [](const Instance& instance, StackId stack_id, ItemPos item_pos) {
                    check_index(stack_id, instance.number_of_stacks(), "stack_id");
                    check_index(item_pos, instance.stack_size(stack_id), "item_pos");
                    return instance.item(stack_id, item_pos);
                },
                nb::arg("stack_id"), nb::arg("item_pos"))
        .def("item_area", &Instance::item_area)
        .def("item_profit", &Instance::item_profit)
        .def("largest_item_profit", &Instance::largest_item_profit)
        .def("largest_efficiency_item_type_id", &Instance::largest_efficiency_item_type_id)
        .def("largest_item_copies", &Instance::largest_item_copies)
        .def("unbounded_knapsack", &Instance::unbounded_knapsack)
        .def("all_item_types_oriented", &Instance::all_item_types_oriented)
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
        .def("read_defects", &InstanceBuilder::read_defects, nb::arg("defects_path"))
        .def("read_item_types", &InstanceBuilder::read_item_types, nb::arg("items_path"))
        // Parameters.
        .def("set_number_of_stages", &InstanceBuilder::set_number_of_stages, nb::arg("number_of_stages"))
        .def("set_number_of_stages_unlimited", &InstanceBuilder::set_number_of_stages_unlimited)
        .def("set_cut_type", &InstanceBuilder::set_cut_type, nb::arg("cut_type"))
        .def("set_first_stage_orientation", &InstanceBuilder::set_first_stage_orientation,
                nb::arg("first_stage_orientation"))
        .def("set_minimum_distance_1_cuts", &InstanceBuilder::set_minimum_distance_1_cuts,
                nb::arg("minimum_distance_1_cuts"))
        .def("set_maximum_distance_1_cuts", &InstanceBuilder::set_maximum_distance_1_cuts,
                nb::arg("maximum_distance_1_cuts"))
        .def("set_minimum_distance_2_cuts", &InstanceBuilder::set_minimum_distance_2_cuts,
                nb::arg("minimum_distance_2_cuts"))
        .def("set_maximum_distance_2_cuts", &InstanceBuilder::set_maximum_distance_2_cuts,
                nb::arg("maximum_distance_2_cuts"))
        .def("set_minimum_waste_length", &InstanceBuilder::set_minimum_waste_length,
                nb::arg("minimum_waste_length"))
        .def("set_maximum_number_1_cuts", &InstanceBuilder::set_maximum_number_1_cuts,
                nb::arg("maximum_number_1_cuts"))
        .def("set_maximum_number_2_cuts", &InstanceBuilder::set_maximum_number_2_cuts,
                nb::arg("maximum_number_2_cuts"))
        .def("set_cut_through_defects", &InstanceBuilder::set_cut_through_defects,
                nb::arg("cut_through_defects"))
        .def("set_cut_thickness", &InstanceBuilder::set_cut_thickness, nb::arg("cut_thickness"))
        .def("set_fixed_cutting_cost", &InstanceBuilder::set_fixed_cutting_cost,
                nb::arg("stage_id"), nb::arg("fixed_cost"))
        .def("set_variable_cutting_cost", &InstanceBuilder::set_variable_cutting_cost,
                nb::arg("stage_id"), nb::arg("variable_cost"))
        .def("set_waste_cost", &InstanceBuilder::set_waste_cost, nb::arg("waste_cost"))
        .def("set_predefined", &InstanceBuilder::set_predefined, nb::arg("str"),
                "Set the parameters from a code such as '3NVO' (number of stages, cut type, "
                "first stage orientation, oriented items) or 'roadef2018'; the 'oriented' "
                "part only applies to the item types already added.")
        .def("set_roadef2018", &InstanceBuilder::set_roadef2018)
        // Bin types.
        .def("add_bin_type",
                nb::overload_cast<Length, Length>(&InstanceBuilder::add_bin_type),
                nb::arg("width"), nb::arg("height"))
        .def("set_bin_type_cost", &InstanceBuilder::set_bin_type_cost,
                nb::arg("bin_type_id"), nb::arg("cost"))
        .def("add_trims", &InstanceBuilder::add_trims,
                nb::arg("bin_type_id"),
                nb::arg("left_trim"), nb::arg("left_trim_type"),
                nb::arg("right_trim"), nb::arg("right_trim_type"),
                nb::arg("bottom_trim"), nb::arg("bottom_trim_type"),
                nb::arg("top_trim"), nb::arg("top_trim_type"))
        .def("add_defect", &InstanceBuilder::add_defect,
                nb::arg("bin_type_id"), nb::arg("x"), nb::arg("y"), nb::arg("w"), nb::arg("h"))
        .def("add_bin_type_resource", &InstanceBuilder::add_bin_type_resource,
                nb::arg("bin_type_id"), nb::arg("capacity"), nb::arg("penalize") = false, nb::arg("penalty") = 0.0)
        .def("add_resource_consumption", &InstanceBuilder::add_resource_consumption,
                nb::arg("bin_type_id"), nb::arg("resource_id"), nb::arg("item_type_id"), nb::arg("schedule"))
        .def("set_bin_type_copies", &InstanceBuilder::set_bin_type_copies,
                nb::arg("bin_type_id"), nb::arg("copies"))
        .def("set_bin_type_copies_min", &InstanceBuilder::set_bin_type_copies_min,
                nb::arg("bin_type_id"), nb::arg("copies_min"))
        .def("set_bin_types_infinite_x", &InstanceBuilder::set_bin_types_infinite_x)
        .def("set_bin_types_infinite_y", &InstanceBuilder::set_bin_types_infinite_y)
        .def("set_bin_types_infinite_copies", &InstanceBuilder::set_bin_types_infinite_copies)
        .def("set_bin_types_unweighted", &InstanceBuilder::set_bin_types_unweighted)
        // Item types.
        .def("add_item_type",
                nb::overload_cast<Length, Length, bool, StackId>(&InstanceBuilder::add_item_type),
                nb::arg("width"), nb::arg("height"), nb::arg("oriented") = false, nb::arg("stack_id") = -1)
        .def("set_item_type_profit", &InstanceBuilder::set_item_type_profit,
                nb::arg("item_type_id"), nb::arg("profit"))
        .def("set_item_type_copies", &InstanceBuilder::set_item_type_copies,
                nb::arg("item_type_id"), nb::arg("copies"))
        .def("set_item_type_copies_min", &InstanceBuilder::set_item_type_copies_min,
                nb::arg("item_type_id"), nb::arg("copies_min"))
        .def("set_item_types_infinite_copies", &InstanceBuilder::set_item_types_infinite_copies)
        .def("multiply_item_types_copies", &InstanceBuilder::multiply_item_types_copies, nb::arg("factor"))
        .def("set_item_types_unweighted", &InstanceBuilder::set_item_types_unweighted)
        .def("set_item_types_oriented", &InstanceBuilder::set_item_types_oriented,
                nb::arg("oriented") = true)
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

    nb::class_<SolutionNode>(m, "SolutionNode",
            "Node of the cut tree of a bin. 'f' is the parent node id (-1 for the "
            "bin node), 'd' the depth (0 for the bin, s for an s-cut node, -1 for "
            "a trim). 'item_type_id' is the item type for an item node, -1 for "
            "waste, -2 for an intermediate node, and the bin type id for the bin "
            "node.")
        .def_ro("f", &SolutionNode::f)
        .def_ro("d", &SolutionNode::d)
        .def_ro("l", &SolutionNode::l)
        .def_ro("r", &SolutionNode::r)
        .def_ro("b", &SolutionNode::b)
        .def_ro("t", &SolutionNode::t)
        .def_ro("children", &SolutionNode::children)
        .def_ro("item_type_id", &SolutionNode::item_type_id);

    nb::class_<SolutionBin>(m, "SolutionBin")
        .def_ro("bin_type_id", &SolutionBin::bin_type_id)
        .def_ro("copies", &SolutionBin::copies)
        .def_ro("first_cut_orientation", &SolutionBin::first_cut_orientation)
        .def_ro("nodes", &SolutionBin::nodes)
        .def_ro("number_of_stages", &SolutionBin::number_of_stages)
        .def_ro("number_of_stages_real", &SolutionBin::number_of_stages_real)
        .def_ro("cut_type_is_non_exact", &SolutionBin::cut_type_is_non_exact)
        .def_ro("cut_type_is_roadef2018", &SolutionBin::cut_type_is_roadef2018)
        .def_ro("resource_consumption", &SolutionBin::resource_consumption);

    nb::class_<Solution>(m, "Solution")
        .def("number_of_stages_feasible", &Solution::number_of_stages_feasible)
        .def("minimum_waste_length_feasible", &Solution::minimum_waste_length_feasible)
        .def("minimum_distance_1_cuts_feasible", &Solution::minimum_distance_1_cuts_feasible)
        .def("maximum_distance_1_cuts_feasible", &Solution::maximum_distance_1_cuts_feasible)
        .def("minimum_distance_2_cuts_feasible", &Solution::minimum_distance_2_cuts_feasible)
        .def("maximum_distance_2_cuts_feasible", &Solution::maximum_distance_2_cuts_feasible)
        .def("maximum_number_1_cuts_feasible", &Solution::maximum_number_1_cuts_feasible)
        .def("maximum_number_2_cuts_feasible", &Solution::maximum_number_2_cuts_feasible)
        .def("stacks_feasible", &Solution::stacks_feasible)
        .def("defects_feasible", &Solution::defects_feasible)
        .def("cut_through_defects_feasible", &Solution::cut_through_defects_feasible)
        .def("item_copies_feasible", &Solution::item_copies_feasible)
        .def("resource_feasible", &Solution::resource_feasible)
        .def("callback_feasible", &Solution::callback_feasible)
        .def("feasible", &Solution::feasible)
        .def("full", &Solution::full)
        .def("number_of_stages", &Solution::number_of_stages)
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
        .def("cutting_cost", &Solution::cutting_cost)
        .def("number_of_items", &Solution::number_of_items)
        .def("item_copies",
                [](const Solution& solution, ItemTypeId item_type_id) {
                    check_index(item_type_id, solution.instance().number_of_item_types(), "item_type_id");
                    return solution.item_copies(item_type_id);
                },
                nb::arg("item_type_id"))
        .def("number_of_infeasible_item_copies_min", &Solution::number_of_infeasible_item_copies_min)
        .def("profit", &Solution::profit)
        .def("item_area", &Solution::item_area)
        .def("width", &Solution::width)
        .def("height", &Solution::height)
        .def("area", &Solution::area)
        .def("full_area", &Solution::full_area)
        .def("waste", &Solution::waste)
        .def("waste_percentage", &Solution::waste_percentage)
        .def("full_waste", &Solution::full_waste)
        .def("full_waste_percentage", &Solution::full_waste_percentage)
        .def("second_leftover_value", &Solution::second_leftover_value)
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

    nb::class_<OptimizeParameters> parameters(m, "OptimizeParameters");
    bind_parameters_base<rectangleguillotine::Output>(parameters);
    parameters
        .def_rw("optimization_mode", &OptimizeParameters::optimization_mode)
        .def_rw("memory_limit_megabytes", &OptimizeParameters::memory_limit_megabytes)
        .def_rw("linear_programming_solver_name", &OptimizeParameters::linear_programming_solver_name)
        .def_rw("use_dual_feasible_functions", &OptimizeParameters::use_dual_feasible_functions)
        .def_rw("use_tree_search", &OptimizeParameters::use_tree_search)
        .def_rw("use_column_generation_strips", &OptimizeParameters::use_column_generation_strips)
        .def_rw("use_sequential_strips_onedimensional", &OptimizeParameters::use_sequential_strips_onedimensional)
        .def_rw("use_tree_search_hypergraph_infinite_copies", &OptimizeParameters::use_tree_search_hypergraph_infinite_copies)
        .def_rw("use_tree_search_maximal_spaces", &OptimizeParameters::use_tree_search_maximal_spaces)
        .def_rw("use_tree_search_hypergraph", &OptimizeParameters::use_tree_search_hypergraph)
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
        .def_rw("not_anytime_dichotomic_search_subproblem_tree_search_queue_size", &OptimizeParameters::not_anytime_dichotomic_search_subproblem_tree_search_queue_size)
        .def_rw("reduction_parameters", &OptimizeParameters::reduction_parameters);

    nb::class_<Handle> output = bind_output<Instance, Solution, rectangleguillotine::Output>(m);
    def_output_field(output, "knapsack_bound", &rectangleguillotine::Output::knapsack_bound);
    def_output_field(output, "bin_packing_bound", &rectangleguillotine::Output::bin_packing_bound);
    def_output_field(output, "variable_sized_bin_packing_bound", &rectangleguillotine::Output::variable_sized_bin_packing_bound);
    def_output_field(output, "open_dimension_x_bound", &rectangleguillotine::Output::open_dimension_x_bound);
    def_output_field(output, "open_dimension_y_bound", &rectangleguillotine::Output::open_dimension_y_bound);
    def_output_field(output, "is_proven_infeasible", &rectangleguillotine::Output::is_proven_infeasible);

    m.def("optimize",
            [](std::shared_ptr<Instance> instance, const OptimizeParameters& parameters) {
                return run_optimize<Instance, rectangleguillotine::Output>(
                        instance,
                        parameters,
                        [](const Instance& instance, const OptimizeParameters& parameters) {
                            return rectangleguillotine::optimize(instance, parameters);
                        });
            },
            nb::arg("instance"),
            nb::arg("parameters") = default_optimize_parameters<OptimizeParameters>());
}

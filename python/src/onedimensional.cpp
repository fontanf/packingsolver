#include "common.hpp"

#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

using namespace packingsolver;
using namespace packingsolver::python;
using namespace packingsolver::onedimensional;

void bind_onedimensional(nb::module_& m)
{
    using Handle = OutputHandle<Instance, onedimensional::Output>;

    /*
     * Instance
     */

    nb::class_<Precedence>(m, "Precedence")
        .def_ro("dominated_item_type_id", &Precedence::dominated_item_type_id)
        .def_ro("dominating_item_type_id", &Precedence::dominating_item_type_id);

    nb::class_<ItemType>(m, "ItemType")
        .def_ro("length", &ItemType::length)
        .def_ro("profit", &ItemType::profit)
        .def_ro("copies", &ItemType::copies)
        .def_ro("copies_min", &ItemType::copies_min)
        .def_ro("weight", &ItemType::weight)
        .def_ro("nesting_length", &ItemType::nesting_length)
        .def_ro("maximum_stackability", &ItemType::maximum_stackability)
        .def_ro("maximum_weight_after", &ItemType::maximum_weight_after)
        .def_ro("eligibility_id", &ItemType::eligibility_id)
        .def_ro("dominated_precedence_ids", &ItemType::dominated_precedence_ids)
        .def_ro("dominating_precedence_ids", &ItemType::dominating_precedence_ids);

    nb::class_<FixedItem>(m, "FixedItem")
        .def_ro("item_type_id", &FixedItem::item_type_id)
        .def_ro("start", &FixedItem::start);

    nb::class_<BinType>(m, "BinType")
        .def_ro("length", &BinType::length)
        .def_ro("cost", &BinType::cost)
        .def_ro("copies", &BinType::copies)
        .def_ro("copies_min", &BinType::copies_min)
        .def_ro("maximum_weight", &BinType::maximum_weight)
        .def_ro("eligibility_ids", &BinType::eligibility_ids)
        .def_ro("fixed_items", &BinType::fixed_items)
        .def("number_of_resources", &BinType::number_of_resources);

    nb::class_<Instance>(m, "Instance")
        .def("objective", &Instance::objective)
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
        .def("bin_length", &Instance::bin_length)
        .def("previous_bin_length",
                [](const Instance& instance, BinPos bin_pos) {
                    check_index(bin_pos, instance.number_of_bins(), "bin_pos");
                    return instance.previous_bin_length(bin_pos);
                },
                nb::arg("bin_pos"))
        .def("largest_bin_cost", &Instance::largest_bin_cost)
        .def("weight_tolerance", &Instance::weight_tolerance)
        .def("number_of_item_types", &Instance::number_of_item_types)
        .def("item_type",
                [](const Instance& instance, ItemTypeId item_type_id) -> const ItemType& {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    return instance.item_type(item_type_id);
                },
                nb::arg("item_type_id"),
                nb::rv_policy::reference_internal)
        .def("number_of_items", &Instance::number_of_items)
        .def("item_length", &Instance::item_length)
        .def("item_profit", &Instance::item_profit)
        .def("largest_item_profit", &Instance::largest_item_profit)
        .def("largest_efficiency_item_type_id", &Instance::largest_efficiency_item_type_id)
        .def("largest_item_copies", &Instance::largest_item_copies)
        .def("unbounded_knapsack", &Instance::unbounded_knapsack)
        .def("fits_some_bin",
                [](const Instance& instance, ItemTypeId item_type_id) {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    return instance.fits_some_bin(item_type_id);
                },
                nb::arg("item_type_id"))
        .def("item_type_fits_bin_type",
                [](const Instance& instance, ItemTypeId item_type_id, BinTypeId bin_type_id) {
                    check_index(item_type_id, instance.number_of_item_types(), "item_type_id");
                    check_index(bin_type_id, instance.number_of_bin_types(), "bin_type_id");
                    return instance.item_type_fits_bin_type(item_type_id, bin_type_id);
                },
                nb::arg("item_type_id"), nb::arg("bin_type_id"))
        .def("precedences", &Instance::precedences, nb::rv_policy::reference_internal)
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
                [](
                    InstanceBuilder& instance_builder,
                    Length length,
                    std::optional<Profit> cost,
                    std::optional<Weight> maximum_weight,
                    std::optional<std::vector<EligibilityId>> eligibility_ids,
                    std::optional<BinPos> copies,
                    std::optional<BinPos> copies_min)
                {
                    BinTypeId bin_type_id = instance_builder.add_bin_type(length);
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
                nb::arg("length"),
                nb::kw_only(),
                nb::arg("cost") = nb::none(),
                nb::arg("maximum_weight") = nb::none(),
                nb::arg("eligibility_ids") = nb::none(),
                nb::arg("copies") = nb::none(),
                nb::arg("copies_min") = nb::none(),
                "Add a bin type and return its id.\n\n"
                "Keyword arguments (left to their default when omitted):\n"
                "- cost: the cost of the bin type.\n"
                "- maximum_weight: the maximum weight of the bin type.\n"
                "- eligibility_ids: a list of eligibility ids added to the bin type.\n"
                "- copies: the number of copies of the bin type.\n"
                "- copies_min: the minimum number of copies of the bin type.")
        .def("add_bin_type_resource", &InstanceBuilder::add_bin_type_resource,
                nb::arg("bin_type_id"), nb::arg("capacity"), nb::arg("penalize") = false, nb::arg("penalty") = 0.0)
        .def("add_resource_consumption", &InstanceBuilder::add_resource_consumption,
                nb::arg("bin_type_id"), nb::arg("resource_id"), nb::arg("item_type_id"), nb::arg("schedule"))
        .def("set_bin_types_infinite_copies", &InstanceBuilder::set_bin_types_infinite_copies)
        .def("set_bin_types_unweighted", &InstanceBuilder::set_bin_types_unweighted)
        // Item types.
        .def("add_item_type",
                [](
                    InstanceBuilder& instance_builder,
                    Length length,
                    std::optional<Weight> weight,
                    std::optional<Length> nesting_length,
                    std::optional<ItemPos> maximum_stackability,
                    std::optional<Weight> maximum_weight_after,
                    std::optional<EligibilityId> eligibility_id,
                    std::optional<Profit> profit,
                    std::optional<ItemPos> copies,
                    std::optional<ItemPos> copies_min)
                {
                    ItemTypeId item_type_id = instance_builder.add_item_type(length);
                    if (weight.has_value())
                        instance_builder.set_item_type_weight(item_type_id, *weight);
                    if (nesting_length.has_value())
                        instance_builder.set_item_type_nesting_length(item_type_id, *nesting_length);
                    if (maximum_stackability.has_value())
                        instance_builder.set_item_type_maximum_stackability(item_type_id, *maximum_stackability);
                    if (maximum_weight_after.has_value())
                        instance_builder.set_item_type_maximum_weight_after(item_type_id, *maximum_weight_after);
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
                nb::arg("length"),
                nb::kw_only(),
                nb::arg("weight") = nb::none(),
                nb::arg("nesting_length") = nb::none(),
                nb::arg("maximum_stackability") = nb::none(),
                nb::arg("maximum_weight_after") = nb::none(),
                nb::arg("eligibility_id") = nb::none(),
                nb::arg("profit") = nb::none(),
                nb::arg("copies") = nb::none(),
                nb::arg("copies_min") = nb::none(),
                "Add an item type and return its id.\n\n"
                "Keyword arguments (left to their default when omitted):\n"
                "- weight: the weight of the item type.\n"
                "- nesting_length: the nesting length of the item type.\n"
                "- maximum_stackability: the maximum stackability of the item type.\n"
                "- maximum_weight_after: the maximum weight after of the item type.\n"
                "- eligibility_id: the eligibility id of the item type.\n"
                "- profit: the profit of the item type (defaults to its length).\n"
                "- copies: the number of copies of the item type.\n"
                "- copies_min: the minimum number of copies to pack of the item type.")
        .def("add_item_type_precedence", &InstanceBuilder::add_item_type_precedence,
                nb::arg("dominated_item_type_id"), nb::arg("dominating_item_type_id"))
        .def("set_item_types_infinite_copies", &InstanceBuilder::set_item_types_infinite_copies)
        .def("set_item_types_unweighted", &InstanceBuilder::set_item_types_unweighted)
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
        .def_ro("start", &SolutionItem::start);

    nb::class_<SolutionBin>(m, "SolutionBin")
        .def_ro("bin_type_id", &SolutionBin::bin_type_id)
        .def_ro("copies", &SolutionBin::copies)
        .def_ro("end", &SolutionBin::end)
        .def_ro("weight", &SolutionBin::weight)
        .def_ro("items", &SolutionBin::items)
        .def_ro("maximum_number_of_items", &SolutionBin::maximum_number_of_items)
        .def_ro("remaining_weight", &SolutionBin::remaining_weight)
        .def_ro("resource_consumption", &SolutionBin::resource_consumption);

    nb::class_<Solution>(m, "Solution")
        .def("feasible", &Solution::feasible)
        .def("callback_feasible", &Solution::callback_feasible)
        .def("capacity_feasible", &Solution::capacity_feasible)
        .def("weight_feasible", &Solution::weight_feasible)
        .def("stackability_feasible", &Solution::stackability_feasible)
        .def("maximum_weight_after_feasible", &Solution::maximum_weight_after_feasible)
        .def("item_copies_feasible", &Solution::item_copies_feasible)
        .def("bin_type_order_feasible", &Solution::bin_type_order_feasible)
        .def("resource_feasible", &Solution::resource_feasible)
        .def("eligibility_feasible", &Solution::eligibility_feasible)
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
        .def("number_of_infeasible_item_copies_min", &Solution::number_of_infeasible_item_copies_min)
        .def("profit", &Solution::profit)
        .def("length", &Solution::length)
        .def("item_length", &Solution::item_length)
        .def("bin_length", &Solution::bin_length)
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
        .def_rw("remove_negative_profit_items", &ReductionParameters::remove_negative_profit_items)
        .def_rw("merge_identical_items", &ReductionParameters::merge_identical_items)
        .def_rw("reduce_full_bin_items", &ReductionParameters::reduce_full_bin_items)
        .def_rw("reduce_perfect_pairs", &ReductionParameters::reduce_perfect_pairs)
        .def_rw("reduce_dominant_sets", &ReductionParameters::reduce_dominant_sets)
        .def_rw("lift_item_lengths", &ReductionParameters::lift_item_lengths)
        .def_rw("shrink_bin", &ReductionParameters::shrink_bin)
        .def_rw("remove_dominated_bin_types", &ReductionParameters::remove_dominated_bin_types);

    nb::class_<OptimizeParameters> parameters = bind_parameters_base<onedimensional::Output, OptimizeParameters>(m);
    parameters
        .def_rw("optimization_mode", &OptimizeParameters::optimization_mode)
        .def_rw("memory_limit_megabytes", &OptimizeParameters::memory_limit_megabytes)
        .def_rw("linear_programming_solver_name", &OptimizeParameters::linear_programming_solver_name)
        .def_rw("use_dual_feasible_functions", &OptimizeParameters::use_dual_feasible_functions)
        .def_rw("use_tree_search", &OptimizeParameters::use_tree_search)
        .def_rw("use_sequential_single_knapsack", &OptimizeParameters::use_sequential_single_knapsack)
        .def_rw("use_sequential_value_correction", &OptimizeParameters::use_sequential_value_correction)
        .def_rw("use_dichotomic_search", &OptimizeParameters::use_dichotomic_search)
        .def_rw("use_column_generation", &OptimizeParameters::use_column_generation)
        .def_rw("use_milp_assignment", &OptimizeParameters::use_milp_assignment)
        .def_rw("tree_search_guides", &OptimizeParameters::tree_search_guides)
        .def_rw("many_items_in_bins_threshold", &OptimizeParameters::many_items_in_bins_threshold)
        .def_rw("many_item_type_copies_factor", &OptimizeParameters::many_item_type_copies_factor)
        .def_rw("sequential_value_correction_subproblem_tree_search_queue_size", &OptimizeParameters::sequential_value_correction_subproblem_tree_search_queue_size)
        .def_rw("column_generation_subproblem_tree_search_queue_size", &OptimizeParameters::column_generation_subproblem_tree_search_queue_size)
        .def_rw("not_anytime_tree_search_queue_size", &OptimizeParameters::not_anytime_tree_search_queue_size)
        .def_rw("not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size", &OptimizeParameters::not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size)
        .def_rw("not_anytime_sequential_value_correction_number_of_iterations", &OptimizeParameters::not_anytime_sequential_value_correction_number_of_iterations)
        .def_rw("not_anytime_dichotomic_search_subproblem_tree_search_queue_size", &OptimizeParameters::not_anytime_dichotomic_search_subproblem_tree_search_queue_size)
        .def_rw("reduction_parameters", &OptimizeParameters::reduction_parameters);

    nb::class_<Handle> output = bind_output<Instance, Solution, onedimensional::Output>(m);
    def_output_field(output, "knapsack_bound", &onedimensional::Output::knapsack_bound);
    def_output_field(output, "bin_packing_bound", &onedimensional::Output::bin_packing_bound);
    def_output_field(output, "variable_sized_bin_packing_bound", &onedimensional::Output::variable_sized_bin_packing_bound);
    def_output_field(output, "is_proven_infeasible", &onedimensional::Output::is_proven_infeasible);

    m.def("optimize",
            [](std::shared_ptr<Instance> instance, const OptimizeParameters& parameters) {
                return run_optimize<Instance, onedimensional::Output>(
                        instance,
                        parameters,
                        [](const Instance& instance, const OptimizeParameters& parameters) {
                            return onedimensional::optimize(instance, parameters);
                        });
            },
            nb::arg("instance"),
            nb::arg("parameters") = default_optimize_parameters<OptimizeParameters>());
}

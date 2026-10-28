#include "packingsolver/boxstacks/post_process.hpp"

#include "packingsolver/boxstacks/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

GroupIdenticalBinsOutput packingsolver::boxstacks::group_identical_bins(
        const Solution& solution)
{
    GroupIdenticalBinsOutput output(solution.instance());
    Solution grouped_solution = packingsolver::group_identical_bins(solution);
    output.solution_pool.add(grouped_solution);
    return output;
}

Solution packingsolver::boxstacks::reoptimize_bin(
        const Solution& solution,
        BinPos bin_pos,
        const OptimizeParameters& parameters)
{
    const Instance& instance = solution.instance();

    InstanceBuilder bin_instance_builder;
    bin_instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    bin_instance_builder.set_parameters(instance.parameters());

    // Add bin types.
    const SolutionBin& bin = solution.bin(bin_pos);
    BinTypeId bin_type_id = bin_instance_builder.add_bin_type(instance, bin.bin_type_id);
    bin_instance_builder.set_bin_type_copies(bin_type_id, 1);
    bin_instance_builder.set_bin_type_copies_min(bin_type_id, 0);

    // Add item types.
    std::vector<ItemPos> bin_item_copies(instance.number_of_item_types(), 0);
    for (const SolutionStack& solution_stack: bin.stacks)
        for (const SolutionItem& solution_item: solution_stack.items)
            bin_item_copies[solution_item.item_type_id]++;

    std::vector<ItemTypeId> bin_to_orig;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (bin_item_copies[item_type_id] > 0) {
            ItemTypeId sub_item_type_id = bin_instance_builder.add_item_type(instance, item_type_id);
            bin_instance_builder.set_item_type_copies(sub_item_type_id, bin_item_copies[item_type_id]);
            bin_to_orig.push_back(item_type_id);
        }
    }

    // Build instance.
    Instance bin_instance = bin_instance_builder.build();

    // Solve instance.
    OptimizeParameters bin_parameters = parameters;
    bin_parameters.messages_to_stdout = false;
    bin_parameters.messages_path = "";
    bin_parameters.messages_streams.clear();
    bin_parameters.write_json_output = false;
    bin_parameters.new_solution_callback = [](const packingsolver::boxstacks::Output&) { };
    bin_parameters.verbosity_level = 0;
    // The sub-instance has a single bin: its post-process would re-optimize
    // this same bin again.
    bin_parameters.post_process = false;
    auto bin_output = optimize(bin_instance, bin_parameters);
    const Solution& bin_solution = bin_output.solution_pool.best();
    if (!bin_solution.feasible() || !bin_solution.full())
        return solution;

    // Retrieve solution: the bins of 'solution', the bin at 'bin_pos' being
    // replaced by the re-optimized one.
    Solution new_solution(instance);
    for (BinPos pos = 0;
            pos < solution.number_of_different_bins();
            ++pos) {
        if (pos == bin_pos) {
            new_solution.append_bin(
                    bin_solution,
                    0,
                    bin.copies,
                    {bin.bin_type_id},
                    bin_to_orig);
        } else {
            new_solution.append_bin(solution, pos, solution.bin(pos).copies);
        }
    }
    return new_solution;
}

Solution packingsolver::boxstacks::reoptimize_non_last_bins(
        const Solution& solution,
        const OptimizeParameters& parameters)
{
    Solution new_solution = solution;
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins() - 1;
            ++bin_pos) {
        if (parameters.timer.needs_to_end())
            break;
        Solution bin_solution = reoptimize_bin(
                new_solution,
                bin_pos,
                parameters);
        // Only the bin at 'bin_pos' differs: the comparison is on its leftover
        // value, then on its staircase leftover value.
        if (new_solution < bin_solution)
            new_solution = bin_solution;
    }
    return new_solution;
}

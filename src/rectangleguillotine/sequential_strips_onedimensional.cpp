#include "rectangleguillotine/sequential_strips_onedimensional.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/column_generation_strips.hpp"
#include "rectangleguillotine/solution_builder.hpp"
#include "algorithms/thread_pool.hpp"

#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

#include <algorithm>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

/** Available length along the first-stage axis of 'orientation'. */
Length first_stage_capacity(
        const BinType& bin_type,
        CutOrientation orientation)
{
    return (orientation == CutOrientation::Vertical)?
        bin_type.rect.w - bin_type.left_trim - bin_type.right_trim:
        bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
}

/** Find the (only) depth-1 node of a single-strip solution. */
SolutionNodeId find_first_stage_node_id(const SolutionBin& bin)
{
    for (SolutionNodeId node_id = 0;
            node_id < (SolutionNodeId)bin.nodes.size();
            ++node_id) {
        if (bin.nodes[node_id].d == 1)
            return node_id;
    }
    throw std::logic_error(
            FUNC_SIGNATURE + ": "
            "no depth-1 node found.");
}

/** Get the extent of a first-stage node along the first-stage axis. */
Length first_stage_extent(
        const SolutionNode& node,
        Length offset,
        CutOrientation orientation)
{
    return (orientation == CutOrientation::Vertical)?
        node.r - offset:
        node.t - offset;
}

/**
 * Recursively copy the subtree rooted at 'node_id' of 'src_bin' into
 * 'builder', shifting the coordinates along the first-stage axis by
 * 'offset_delta' (the perpendicular axis is left untouched).
 */
void copy_subtree(
        const SolutionBin& src_bin,
        SolutionNodeId node_id,
        CutOrientation orientation,
        Length offset_delta,
        SolutionBuilder& builder)
{
    const SolutionNode& node = src_bin.nodes[node_id];
    // Which field 'add_node' expects depends on the parity of the depth
    // relative to the orientation (see SolutionBuilder::add_node).
    bool read_r = (orientation == CutOrientation::Vertical && node.d % 2 == 1)
        || (orientation == CutOrientation::Horizontal && node.d % 2 == 0);
    Length cut_position = (read_r)? node.r: node.t;
    // The offset only applies to the first-stage axis, i.e. odd depths,
    // regardless of orientation (depth 1 is always the first-stage cut).
    if (node.d % 2 == 1)
        cut_position += offset_delta;
    builder.add_node(node.d, cut_position);
    if (node.item_type_id >= 0)
        builder.set_last_node_item(node.item_type_id);
    for (SolutionNodeId child_id: node.children)
        copy_subtree(src_bin, child_id, orientation, offset_delta, builder);
}

struct Strip
{
    /** Standalone (single-bin) solution of the strip. */
    Solution solution;

    /** Extent of the strip along the first-stage axis. */
    Length extent;
};

/**
 * Split a single-bin solution packing all items (as produced by phase 1)
 * into its individual first-stage strips.
 */
std::vector<Strip> split_into_strips(
        const Instance& instance,
        const Solution& phase1_solution,
        CutOrientation orientation)
{
    const SolutionBin& phase1_bin = phase1_solution.bin(0);

    // Collect the depth-1 (first-stage) node ids, sorted by their position
    // along the first-stage axis (they are contiguous but not necessarily in
    // vector order).
    std::vector<SolutionNodeId> first_stage_node_ids;
    for (SolutionNodeId node_id = 0;
            node_id < (SolutionNodeId)phase1_bin.nodes.size();
            ++node_id) {
        if (phase1_bin.nodes[node_id].d == 1)
            first_stage_node_ids.push_back(node_id);
    }
    std::sort(
            first_stage_node_ids.begin(),
            first_stage_node_ids.end(),
            [&phase1_bin, &orientation](
                SolutionNodeId node_id_1,
                SolutionNodeId node_id_2)
            {
                const SolutionNode& node_1 = phase1_bin.nodes[node_id_1];
                const SolutionNode& node_2 = phase1_bin.nodes[node_id_2];
                return (orientation == CutOrientation::Vertical)?
                    node_1.l < node_2.l:
                    node_1.b < node_2.b;
            });

    std::vector<Strip> strips;
    for (SolutionNodeId node_id: first_stage_node_ids) {
        const SolutionNode& node = phase1_bin.nodes[node_id];
        // Skip pure waste/residual segments (no item, no further cuts): they
        // are filler for the artificially widened phase-1 bin, not strips.
        if (node.item_type_id < 0 && node.children.empty())
            continue;
        Length strip_offset = (orientation == CutOrientation::Vertical)?
            node.l:
            node.b;
        Length strip_extent = first_stage_extent(node, strip_offset, orientation);
        if (strip_extent <= 0)
            continue;

        SolutionBuilder strip_builder(instance);
        strip_builder.add_bin(0, 1, orientation);
        copy_subtree(phase1_bin, node_id, orientation, -strip_offset, strip_builder);
        strips.push_back({strip_builder.build(), strip_extent});
    }
    return strips;
}

/**
 * Phase 2: solve a onedimensional bin packing problem where the items are
 * the strips generated in phase 1, then reconstruct the final solution by
 * placing, in each bin, the strips assigned to it side by side.
 *
 * Invoked from phase 1's 'new_solution_callback' (rather than once after
 * phase 1 fully returns): phase 1 is an anytime search that only returns
 * once its own share of the timer expires, so waiting for it to return
 * before ever attempting phase 2 would leave no time for phase 2 at all
 * under a tight time limit, even though phase 1 typically finds a first
 * complete strip covering almost immediately.
 */
void run_phase2_and_reconstruct(
        const Instance& instance,
        CutOrientation orientation,
        const Solution& phase1_solution,
        const SequentialStripsOnedimensionalParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        packingsolver::Output<Instance, Solution>* local_output)
{
    if (!phase1_solution.full() || phase1_solution.number_of_bins() == 0)
        return;

    const BinType& bin_type = instance.bin_type(0);
    Length cut_thickness = instance.parameters().cut_thickness;
    Length first_stage_length = first_stage_capacity(bin_type, orientation);

    std::vector<Strip> strips = split_into_strips(instance, phase1_solution, orientation);

    onedimensional::InstanceBuilder phase2_instance_builder;
    phase2_instance_builder.set_objective(instance.objective());
    phase2_instance_builder.add_bin_type(
            first_stage_length + cut_thickness,
            bin_type.cost,
            bin_type.copies,
            bin_type.copies_min);
    for (const Strip& strip: strips) {
        phase2_instance_builder.add_item_type(
                strip.extent + cut_thickness,
                -1,
                1);
    }
    onedimensional::Instance phase2_instance = phase2_instance_builder.build();

    onedimensional::OptimizeParameters od_parameters;
    od_parameters.verbosity_level = 0;
    od_parameters.timer = parameters.timer;
    od_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    od_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    od_parameters.optimization_mode = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
        OptimizationMode::NotAnytimeSequential:
        OptimizationMode::NotAnytimeDeterministic;
    auto phase2_output = onedimensional::optimize(phase2_instance, od_parameters);

    const onedimensional::Solution& phase2_solution = phase2_output.solution_pool.best();
    if (!phase2_solution.full())
        return;

    SolutionBuilder final_solution_builder(instance);
    for (BinPos bin_pos = 0;
            bin_pos < phase2_solution.number_of_different_bins();
            ++bin_pos) {
        const onedimensional::SolutionBin& phase2_bin = phase2_solution.bin(bin_pos);
        final_solution_builder.add_bin(0, phase2_bin.copies, orientation);
        for (const onedimensional::SolutionItem& phase2_item: phase2_bin.items) {
            const Strip& strip = strips[phase2_item.item_type_id];
            const SolutionBin& strip_bin = strip.solution.bin(0);
            SolutionNodeId strip_node_id = find_first_stage_node_id(strip_bin);
            copy_subtree(
                    strip_bin,
                    strip_node_id,
                    orientation,
                    phase2_item.start,
                    final_solution_builder);
        }
    }
    Solution final_solution = final_solution_builder.build();
    if (local_output != nullptr) {
        local_output->solution_pool.add(final_solution, "SSO");
    } else {
        algorithm_formatter.update_solution(final_solution, "SSO");
    }
}

void sequential_strips_onedimensional_oriented(
        const Instance& instance,
        CutOrientation orientation,
        const SequentialStripsOnedimensionalParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        packingsolver::Output<Instance, Solution>* local_output)
{
    const BinType& bin_type = instance.bin_type(0);
    Length first_stage_length = first_stage_capacity(bin_type, orientation);

    // Phase 1: generate strips by solving the strip packing problem (all
    // items packed, minimize the total length used along the first-stage
    // axis). Each individual strip is capped to 'first_stage_length' (via
    // 'maximum_distance_1_cuts') so that it can later be packed as a single
    // item in phase 2.
    InstanceBuilder phase1_instance_builder;
    // The instance's own objective must match the axis the outer formatter
    // reports on, so that comparing (unflipped) solutions with operator<
    // uses the axis actually being minimized: width for Vertical, height for
    // Horizontal (InstanceFlipper converts OpenDimensionY to OpenDimensionX
    // internally for the flipped/Vertical solve either way).
    phase1_instance_builder.set_objective(
            (orientation == CutOrientation::Vertical)?
                Objective::OpenDimensionX:
                Objective::OpenDimensionY);
    rectangleguillotine::Parameters phase1_parameters = instance.parameters();
    phase1_parameters.first_stage_orientation = orientation;
    phase1_parameters.maximum_distance_1_cuts = (phase1_parameters.maximum_distance_1_cuts == -1)?
        first_stage_length:
        std::min(phase1_parameters.maximum_distance_1_cuts, first_stage_length);
    phase1_instance_builder.set_parameters(phase1_parameters);
    phase1_instance_builder.add_bin_type(instance, 0, 1);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        phase1_instance_builder.add_item_type(
                instance,
                item_type_id,
                item_type.profit,
                item_type.copies);
    }
    if (orientation == CutOrientation::Vertical) {
        phase1_instance_builder.set_bin_types_infinite_x();
    } else {
        phase1_instance_builder.set_bin_types_infinite_y();
    }
    Instance phase1_instance = phase1_instance_builder.build();

    ColumnGenerationStripsParameters cgs_parameters;
    cgs_parameters.verbosity_level = 0;
    cgs_parameters.timer = parameters.timer;
    cgs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cgs_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    cgs_parameters.optimization_mode = parameters.optimization_mode;
    cgs_parameters.new_solution_callback = [
        &instance, orientation, &parameters, &algorithm_formatter, local_output](
                const packingsolver::Output<Instance, Solution>& ps_output)
        {
            run_phase2_and_reconstruct(
                    instance,
                    orientation,
                    ps_output.solution_pool.best(),
                    parameters,
                    algorithm_formatter,
                    local_output);
        };
    column_generation_strips(phase1_instance, cgs_parameters);
}

}

const SequentialStripsOnedimensionalOutput packingsolver::rectangleguillotine::sequential_strips_onedimensional(
        const Instance& instance,
        const SequentialStripsOnedimensionalParameters& parameters)
{
    SequentialStripsOnedimensionalOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.parameters().first_stage_orientation == CutOrientation::Vertical) {
        sequential_strips_onedimensional_oriented(
                instance,
                CutOrientation::Vertical,
                parameters,
                algorithm_formatter,
                nullptr);
    } else if (instance.parameters().first_stage_orientation == CutOrientation::Horizontal) {
        sequential_strips_onedimensional_oriented(
                instance,
                CutOrientation::Horizontal,
                parameters,
                algorithm_formatter,
                nullptr);
    } else {
        // 'sequential_strips_onedimensional_oriented' runs in parallel for
        // both orientations; in 'NotAnytimeDeterministic' mode, each writes
        // its solutions to its own local output instead of the shared
        // 'algorithm_formatter', so that they can be replayed into it in a
        // fixed, deterministic order (vertical then horizontal) once both
        // have terminated, instead of the (non-deterministic) order in which
        // they actually finish.
        bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
        packingsolver::Output<Instance, Solution> local_output_vertical(instance);
        packingsolver::Output<Instance, Solution> local_output_horizontal(instance);

        std::vector<std::function<void()>> tasks;
        std::forward_list<std::exception_ptr> exception_ptr_list;
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr_1 = exception_ptr_list.front();
        tasks.push_back([&exception_ptr_1, &instance, &parameters, &algorithm_formatter, &local_output_vertical, deterministic]() {
            wrapper<decltype(&sequential_strips_onedimensional_oriented), sequential_strips_onedimensional_oriented>(
                    exception_ptr_1,
                    instance,
                    CutOrientation::Vertical,
                    parameters,
                    algorithm_formatter,
                    deterministic ? &local_output_vertical : nullptr);
        });
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr_2 = exception_ptr_list.front();
        tasks.push_back([&exception_ptr_2, &instance, &parameters, &algorithm_formatter, &local_output_horizontal, deterministic]() {
            wrapper<decltype(&sequential_strips_onedimensional_oriented), sequential_strips_onedimensional_oriented>(
                    exception_ptr_2,
                    instance,
                    CutOrientation::Horizontal,
                    parameters,
                    algorithm_formatter,
                    deterministic ? &local_output_horizontal : nullptr);
        });
        run(tasks, true);
        for (const std::exception_ptr& exception_ptr: exception_ptr_list)
            if (exception_ptr)
                std::rethrow_exception(exception_ptr);

        if (deterministic) {
            algorithm_formatter.update_solution(
                    local_output_vertical.solution_pool.best(),
                    local_output_vertical.solution_pool.best_label());
            algorithm_formatter.update_solution(
                    local_output_horizontal.solution_pool.best(),
                    local_output_horizontal.solution_pool.best_label());
        }
    }

    algorithm_formatter.end();
    return output;
}

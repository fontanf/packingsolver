#include "rectangleguillotine/column_generation_2.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

#include "columngenerationsolver/algorithms/limited_discrepancy_search.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

using Value = columngenerationsolver::Value;
using Column = columngenerationsolver::Column;
using PricingOutput = columngenerationsolver::PricingSolver::PricingOutput;

class ColumnGenerationPricingSolver: public columngenerationsolver::PricingSolver
{

public:

    ColumnGenerationPricingSolver(
            const Instance& instance):
        instance_(instance),
        filled_demands_(instance.number_of_item_types())
    { }

    virtual std::vector<std::shared_ptr<const Column>> initialize_pricing(
            const std::vector<std::pair<std::shared_ptr<const Column>, Value>>& fixed_columns);

    virtual PricingOutput solve_pricing(
            const std::vector<Value>& duals);

private:

    const Instance& instance_;

    std::vector<double> filled_demands_;

    Length filled_width_ = 0;

};

columngenerationsolver::Model get_model(
        const Instance& instance)
{
    const BinType& bin_type = instance.bin_type(0);

    columngenerationsolver::Model model;

    Profit maximum_bin_type_cost = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        if (maximum_bin_type_cost < instance.bin_type(bin_type_id).cost)
            maximum_bin_type_cost = instance.bin_type(bin_type_id).cost;
    }

    ItemPos maximum_item_type_demand = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (maximum_item_type_demand < instance.item_type(item_type_id).copies)
            maximum_item_type_demand = instance.item_type(item_type_id).copies;
    }

    Profit maximum_item_profit = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (maximum_item_profit < instance.item_type(item_type_id).profit)
            maximum_item_profit = instance.item_type(item_type_id).profit;
    }

    if (instance.objective() == Objective::OpenDimensionX) {
        model.objective_sense = optimizationtools::ObjectiveDirection::Minimize;
    } else if (instance.objective() == Objective::Knapsack) {
        model.objective_sense = optimizationtools::ObjectiveDirection::Maximize;
    }

    // Row bounds.
    if (instance.objective() == Objective::Knapsack) {

        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            columngenerationsolver::Row row;
            row.lower_bound = 0;
            row.upper_bound = instance.item_type(item_type_id).copies;
            row.coefficient_lower_bound = 0;
            row.coefficient_upper_bound = instance.item_type(item_type_id).copies;
            model.rows.push_back(row);
        }

        columngenerationsolver::Row row;
        row.lower_bound = 0;
        row.upper_bound = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
        row.coefficient_lower_bound = 0;
        row.coefficient_upper_bound = 1;
        model.rows.push_back(row);

    } else {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            columngenerationsolver::Row row;
            row.lower_bound = instance.item_type(item_type_id).copies;
            row.upper_bound = instance.item_type(item_type_id).copies;
            row.coefficient_lower_bound = 0;
            row.coefficient_upper_bound = instance.item_type(item_type_id).copies;
            model.rows.push_back(row);
        }
    }

    // Pricing solver.
    model.pricing_solver = std::unique_ptr<columngenerationsolver::PricingSolver>(
            new ColumnGenerationPricingSolver(instance));
    return model;
}

std::vector<std::shared_ptr<const Column>> ColumnGenerationPricingSolver::initialize_pricing(
        const std::vector<std::pair<std::shared_ptr<const Column>, Value>>& fixed_columns)
{
    //std::cout << "initialize_pricing " << fixed_columns.size() << std::endl;
    std::fill(filled_demands_.begin(), filled_demands_.end(), 0);
    filled_width_ = 0;
    for (auto p: fixed_columns) {
        const Column& column = *(p.first);
        Value value = p.second;
        if (value < 0.5)
            continue;
        for (const columngenerationsolver::LinearTerm& element: column.elements) {
            if (element.row < instance_.number_of_item_types()) {
                filled_demands_[element.row] += value * element.coefficient;
            } else {
                filled_width_ += value * element.coefficient;
            }
        }
    }
    //std::cout << "initialize_pricing end" << std::endl;
    return {};
}

PricingOutput ColumnGenerationPricingSolver::solve_pricing(
            const std::vector<Value>& duals)
{
    //std::cout << "solve_pricing" << std::endl;

    const BinType& bin_type = instance_.bin_type(0);

    PricingOutput output;
    Value reduced_cost_bound = 0.0;

    // Generate one-staged exact patterns.
    {
        Length width = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim - filled_width_;
        Length height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
        for (;;) {
            //std::cout << "1E width " << width << std::endl;

            // Build one-dimensional knapsack instance.
            onedimensional::InstanceBuilder kp_instance_builder;
            kp_instance_builder.set_objective(Objective::Knapsack);
            kp_instance_builder.add_bin_type(height);
            std::vector<BinTypeId> kp2orig;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance_.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance_.item_type(item_type_id);

                ItemPos copies = item_type.copies - filled_demands_[item_type_id];
                if (copies == 0)
                    continue;

                Profit profit = 0;
                if (instance_.objective() == Objective::OpenDimensionX) {
                    profit = duals[item_type_id];
                } else if (instance_.objective() == Objective::Knapsack) {
                    profit = instance_.item_type(item_type_id).profit
                        - duals[item_type_id];
                }
                if (profit <= 0)
                    continue;

                if (item_type.rect.w == width
                        && item_type.rect.h <= height) {
                    kp_instance_builder.add_item_type(
                            item_type.rect.h,
                            profit,
                            copies);
                    kp2orig.push_back(item_type_id);
                } else if (!item_type.oriented
                        && item_type.rect.h == width
                        && item_type.rect.w <= height) {
                    kp_instance_builder.add_item_type(
                            item_type.rect.w,
                            profit,
                            copies);
                    kp2orig.push_back(item_type_id);
                }
            }
            onedimensional::Instance kp_instance = kp_instance_builder.build();

            if (kp_instance.number_of_item_types() > 0) {

                // Solve one-dimensional knapsack problem.
                onedimensional::OptimizeParameters kp_parameters;
                kp_parameters.verbosity_level = 0;
                auto kp_output = optimize(kp_instance, kp_parameters);

                // Retrieve column.
                //std::cout << "retrieve column" << std::endl;
                Column column;
                SolutionBuilder extra_solution_builder(instance_);
                extra_solution_builder.add_bin(0, 1, CutOrientation::Vertical);
                extra_solution_builder.add_node(1, bin_type.left_trim + width);
                Length cut_position = bin_type.bottom_trim;
                for (ItemTypeId kp_item_type_id = 0;
                        kp_item_type_id < kp_instance.number_of_item_types();
                        ++kp_item_type_id) {
                    ItemTypeId item_type_id = kp2orig[kp_item_type_id];
                    const ItemType& item_type = instance_.item_type(item_type_id);

                    ItemPos copies = kp_output.solution_pool.best().item_copies(kp_item_type_id);
                    if (copies == 0)
                        continue;

                    if (instance_.objective() == Objective::Knapsack)
                        column.objective_coefficient += copies * item_type.profit;

                    columngenerationsolver::LinearTerm element;
                    element.row = item_type_id;
                    element.coefficient = copies;
                    column.elements.push_back(element);

                    for (ItemPos copy = 0; copy < copies; ++copy) {
                        cut_position += kp_instance.item_type(kp_item_type_id).length;
                        extra_solution_builder.add_node(2, cut_position);
                        extra_solution_builder.set_last_node_item(item_type_id);
                    }
                }
                if (instance_.objective() == Objective::OpenDimensionX) {
                    column.objective_coefficient = width;
                } else {
                    columngenerationsolver::LinearTerm element;
                    element.row = instance_.number_of_item_types();
                    element.coefficient = width;
                    column.elements.push_back(element);
                }
                Solution extra_solution = extra_solution_builder.build();
                column.extra = std::shared_ptr<void>(new Solution(extra_solution));
                output.columns.push_back(std::shared_ptr<const Column>(new Column(column)));

            }

            // Find the largest width strictly smaller than the largest width
            // in the generated column.
            Length width_new = 0;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance_.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance_.item_type(item_type_id);

                ItemPos copies = item_type.copies - filled_demands_[item_type_id];
                if (copies == 0)
                    continue;

                Profit profit = 0;
                if (instance_.objective() == Objective::OpenDimensionX) {
                    profit = duals[item_type_id];
                } else if (instance_.objective() == Objective::Knapsack) {
                    profit = instance_.item_type(item_type_id).profit
                        - duals[item_type_id];
                }
                if (profit <= 0)
                    continue;

                if (item_type.rect.w < width
                        && item_type.rect.h <= height
                        && width_new < item_type.rect.w) {
                    width_new = item_type.rect.w;
                }
                if (!item_type.oriented
                        && item_type.rect.h < width
                        && item_type.rect.w <= height
                        && width_new < item_type.rect.h) {
                    width_new = item_type.rect.h;
                }
            }
            if (width_new == 0)
                break;
            width = width_new;
        }
    }
    if (instance_.parameters().number_of_stages == 2
            && instance_.parameters().cut_type == CutType::Exact) {
        return output;
    }

    // Generate one-staged non-exact patterns.
    {
        Length width = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim - filled_width_;
        Length height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
        for (;;) {
            //std::cout << "1N width " << width << std::endl;

            // Build one-dimensional knapsack instance.
            onedimensional::InstanceBuilder kp_instance_builder;
            kp_instance_builder.set_objective(Objective::Knapsack);
            kp_instance_builder.add_bin_type(height);
            std::vector<std::pair<ItemTypeId, Length>> kp2orig;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance_.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance_.item_type(item_type_id);

                ItemPos copies = item_type.copies - filled_demands_[item_type_id];
                if (copies == 0)
                    continue;

                Profit profit = 0;
                if (instance_.objective() == Objective::OpenDimensionX) {
                    profit = duals[item_type_id];
                } else if (instance_.objective() == Objective::Knapsack) {
                    profit = instance_.item_type(item_type_id).profit
                        - duals[item_type_id];
                }
                if (!strictly_greater(profit, 0.0))
                    continue;

                Length item_width = width + 1;
                Length item_height = height + 1;
                if (item_type.rect.w <= width
                        && item_type.rect.h <= height) {
                    item_width = item_type.rect.w;
                    item_height = item_type.rect.h;
                }
                if (!item_type.oriented
                        && item_type.rect.h <= width
                        && item_type.rect.w <= height
                        && item_type.rect.w < item_height) {
                    item_width = item_type.rect.h;
                    item_height = item_type.rect.w;
                }

                if (item_width <= width) {
                    kp_instance_builder.add_item_type(
                            item_height,
                            profit,
                            copies);
                    kp2orig.push_back({item_type_id, item_width});
                }
            }
            onedimensional::Instance kp_instance = kp_instance_builder.build();
            if (kp_instance.number_of_item_types() == 0)
                break;

            // Solve one-dimensional knapsack problem.
            onedimensional::OptimizeParameters kp_parameters;
            kp_parameters.verbosity_level = 0;
            auto kp_output = optimize(kp_instance, kp_parameters);

            // Retrieve column.
            Column column;
            Length width_max = 0;
            for (ItemTypeId kp_item_type_id = 0;
                    kp_item_type_id < kp_instance.number_of_item_types();
                    ++kp_item_type_id) {
                ItemTypeId item_type_id = kp2orig[kp_item_type_id].first;
                const ItemType& item_type = instance_.item_type(item_type_id);

                ItemPos copies = kp_output.solution_pool.best().item_copies(kp_item_type_id);
                if (copies == 0)
                    continue;

                if (width_max < kp2orig[kp_item_type_id].second)
                    width_max = kp2orig[kp_item_type_id].second;

                if (instance_.objective() == Objective::Knapsack)
                    column.objective_coefficient += copies * item_type.profit;

                columngenerationsolver::LinearTerm element;
                element.row = item_type_id;
                element.coefficient = copies;
                column.elements.push_back(element);
            }
            if (instance_.objective() == Objective::OpenDimensionX) {
                column.objective_coefficient = width_max;
            } else {
                columngenerationsolver::LinearTerm element;
                element.row = instance_.number_of_item_types();
                element.coefficient = width_max;
                column.elements.push_back(element);
            }

            // Build extra solution.
            //std::cout << "build extra solution width_max " << width_max << std::endl;
            SolutionBuilder extra_solution_builder(instance_);
            extra_solution_builder.add_bin(0, 1, CutOrientation::Vertical);
            extra_solution_builder.add_node(1, bin_type.left_trim + width_max);
            Length cut_position = bin_type.bottom_trim;
            for (ItemTypeId kp_item_type_id = 0;
                    kp_item_type_id < kp_instance.number_of_item_types();
                    ++kp_item_type_id) {
                ItemTypeId item_type_id = kp2orig[kp_item_type_id].first;
                const ItemType& item_type = instance_.item_type(item_type_id);
                Length width_cur = kp2orig[kp_item_type_id].second;

                ItemPos copies = kp_output.solution_pool.best().item_copies(kp_item_type_id);
                if (copies == 0)
                    continue;

                for (ItemPos copy = 0; copy < copies; ++copy) {
                    cut_position += kp_instance.item_type(kp_item_type_id).length;
                    extra_solution_builder.add_node(2, cut_position);
                    if (width_cur < width_max)
                        extra_solution_builder.add_node(3, bin_type.left_trim + width_cur);
                    extra_solution_builder.set_last_node_item(item_type_id);
                }
            }
            Solution extra_solution = extra_solution_builder.build();
            column.extra = std::shared_ptr<void>(new Solution(extra_solution));

            output.columns.push_back(std::shared_ptr<const Column>(new Column(column)));

            // Find the largest width strictly smaller than the largest width
            // in the generated column.
            Length width_new = 0;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance_.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance_.item_type(item_type_id);

                ItemPos copies = item_type.copies - filled_demands_[item_type_id];
                if (copies == 0)
                    continue;

                Profit profit = 0;
                if (instance_.objective() == Objective::OpenDimensionX) {
                    profit = duals[item_type_id];
                } else if (instance_.objective() == Objective::Knapsack) {
                    profit = instance_.item_type(item_type_id).profit
                        - duals[item_type_id];
                }
                if (!strictly_greater(profit, 0.0))
                    continue;

                if (item_type.rect.w < width
                        && item_type.rect.h <= height
                        && width_new < item_type.rect.w) {
                    width_new = item_type.rect.w;
                }
                if (!item_type.oriented
                        && item_type.rect.h < width
                        && item_type.rect.w <= height
                        && width_new < item_type.rect.h) {
                    width_new = item_type.rect.h;
                }
            }
            if (width_new == 0)
                break;
            width = width_new;
        }
    }
    if (instance_.parameters().number_of_stages == 2
            && instance_.parameters().cut_type == CutType::NonExact) {
        return output;
    }

    // Generate two-staged homogenous patterns.
    {
        Length width = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim - filled_width_;
        Length height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
        // TODO
    }
    if (instance_.parameters().number_of_stages == 3
            && instance_.parameters().cut_type == CutType::Homogenous)
        return output;

    // Generate other patterns.
    {
        Length width = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim - filled_width_;
        Length height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
        // TODO
    }

    //std::cout << "solve_pricing end" << std::endl;
    return output;
}

void column_generation_2_vertical(
        const Instance& instance,
        const ColumnGeneration2Parameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    Length item_largest_length = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_largest_length < item_type.rect.w)
            item_largest_length = item_type.rect.w;
        if (!item_type.oriented
                && item_largest_length < item_type.rect.h) {
            item_largest_length = item_type.rect.h;
        }
    }

    columngenerationsolver::Model cgs_model = get_model(instance);
    columngenerationsolver::LimitedDiscrepancySearchParameters cgslds_parameters;
    cgslds_parameters.verbosity_level = 0;
    cgslds_parameters.timer = parameters.timer;
    cgslds_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cgslds_parameters.internal_diving = 1;
    if (instance.objective() == Objective::OpenDimensionX) {
        cgslds_parameters.dummy_column_objective_coefficient = 2 * item_largest_length;
    } else {
        cgslds_parameters.dummy_column_objective_coefficient = 1;
    }
    cgslds_parameters.automatic_stop = parameters.automatic_stop;
    cgslds_parameters.new_solution_callback = [&instance, &algorithm_formatter](
            const columngenerationsolver::Output& cgs_output)
    {
        const BinType& bin_type = instance.bin_type(0);
        const columngenerationsolver::LimitedDiscrepancySearchOutput& cgslds_output
            = static_cast<const columngenerationsolver::LimitedDiscrepancySearchOutput&>(cgs_output);
        if (cgslds_output.solution.feasible()) {
            //std::cout << "callback" << std::endl;
            SolutionBuilder solution_builder(instance);
            solution_builder.add_bin(0, 1, CutOrientation::Vertical);
            Length offset = 0;
            for (const auto& pair: cgslds_output.solution.columns()) {
                const Column& column = *(pair.first);
                BinPos value = std::round(pair.second);
                for (BinPos v = 0; v < value; ++v) {
                    const SolutionBin& bin = std::static_pointer_cast<Solution>(column.extra)->bin(0);
                    bool first = true;
                    Length offset_new = offset;
                    for (const SolutionNode& node: bin.nodes) {
                        if (node.d <= 0)
                            continue;
                        if (node.d == 1) {
                            if (!first)
                                break;
                            first = false;
                            offset_new += (node.r - node.l);
                        }
                        if (node.d % 2 == 1) {
                            solution_builder.add_node(node.d, offset + node.r);
                        } else {
                            solution_builder.add_node(node.d, node.t);
                        }
                        if (node.item_type_id >= 0)
                            solution_builder.set_last_node_item(node.item_type_id);
                    }
                    offset = offset_new;
                }
            }
            Solution solution = solution_builder.build();
            std::stringstream ss;
            ss << "CG n " << cgslds_output.number_of_nodes;
            algorithm_formatter.update_solution(solution, ss.str());
        }
    };
    cgslds_parameters.column_generation_parameters.solver_name
        = parameters.solver_name;
    columngenerationsolver::limited_discrepancy_search(cgs_model, cgslds_parameters);
}

void column_generation_2_horizontal(
        const Instance& instance,
        const ColumnGeneration2Parameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    // Build flipped instance.
    InstanceBuilder flipped_instance_builder;
    if (instance.objective() == Objective::OpenDimensionY) {
        flipped_instance_builder.set_objective(Objective::OpenDimensionX);
    } else if (instance.objective() == Objective::Knapsack) {
        flipped_instance_builder.set_objective(Objective::Knapsack);
    } else {
        throw std::logic_error("");
    }
    rectangleguillotine::Parameters flipped_instance_parameters = instance.parameters();
    flipped_instance_parameters.first_stage_orientation = CutOrientation::Vertical;
    flipped_instance_builder.set_parameters(flipped_instance_parameters);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinType bin_type_new = bin_type;
        bin_type_new.rect.w = bin_type.rect.h;
        bin_type_new.rect.h = bin_type.rect.w;
        bin_type_new.left_trim = bin_type.bottom_trim;
        bin_type_new.left_trim_type = bin_type.bottom_trim_type;
        bin_type_new.right_trim = bin_type.top_trim;
        bin_type_new.right_trim_type = bin_type.top_trim_type;
        bin_type_new.bottom_trim = bin_type.left_trim;
        bin_type_new.bottom_trim_type = bin_type.left_trim_type;
        bin_type_new.top_trim = bin_type.right_trim;
        bin_type_new.top_trim_type = bin_type.right_trim_type;
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            bin_type_new.defects[defect_id].pos.x = bin_type.defects[defect_id].pos.y;
            bin_type_new.defects[defect_id].pos.y = bin_type.defects[defect_id].pos.x;
            bin_type_new.defects[defect_id].rect.w = bin_type.defects[defect_id].rect.h;
            bin_type_new.defects[defect_id].rect.h = bin_type.defects[defect_id].rect.w;
        }
        flipped_instance_builder.add_bin_type(
                bin_type_new,
                bin_type.copies,
                bin_type.copies_min);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemType item_type_new = item_type;
        item_type_new.rect.w = item_type.rect.h;
        item_type_new.rect.h = item_type.rect.w;
        flipped_instance_builder.add_item_type(
                item_type_new,
                item_type.profit,
                item_type.copies);
    }
    Instance flipped_instance = flipped_instance_builder.build();

    ColumnGeneration2Parameters flipped_parameters = parameters;
    flipped_parameters.new_solution_callback = [
        &instance, &algorithm_formatter](
                const packingsolver::Output<Instance, Solution>& ps_output)
        {
            const ColumnGeneration2Output& flipped_output
                = static_cast<const ColumnGeneration2Output&>(ps_output);
            std::stringstream ss;
            ss << "CG n ";
            //std::cout << "callback flipped" << std::endl;
            SolutionBuilder solution_builder(instance);
            const Solution& flipped_solution = flipped_output.solution_pool.best();
            for (BinPos bin_pos = 0;
                    bin_pos < flipped_solution.number_of_different_bins();
                    ++bin_pos) {
                const SolutionBin& flipped_bin = flipped_solution.bin(bin_pos);
                solution_builder.add_bin(
                        flipped_bin.bin_type_id,
                        flipped_bin.copies,
                        CutOrientation::Horizontal);
                for (const SolutionNode& flipped_node: flipped_bin.nodes) {
                    if (flipped_node.d <= 0)
                        continue;
                    if (flipped_node.d % 2 == 1) {
                        solution_builder.add_node(flipped_node.d, flipped_node.r);
                    } else {
                        solution_builder.add_node(flipped_node.d, flipped_node.t);
                    }
                    if (flipped_node.item_type_id >= 0)
                        solution_builder.set_last_node_item(flipped_node.item_type_id);
                }
            }
            Solution solution = solution_builder.build();
            algorithm_formatter.update_solution(
                    solution,
                    ss.str());
        };
    column_generation_2(
            flipped_instance,
            flipped_parameters);
}

}

const ColumnGeneration2Output packingsolver::rectangleguillotine::column_generation_2(
        const Instance& instance,
        const ColumnGeneration2Parameters& parameters)
{
    ColumnGeneration2Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Reduction.
    if (instance.parameters().first_stage_orientation == CutOrientation::Vertical) {
        column_generation_2_vertical(
                instance,
                parameters,
                algorithm_formatter);
    } else if (instance.parameters().first_stage_orientation == CutOrientation::Horizontal) {
        column_generation_2_horizontal(
                instance,
                parameters,
                algorithm_formatter);
    } else {
        std::vector<std::thread> threads;
        std::forward_list<std::exception_ptr> exception_ptr_list;
        exception_ptr_list.push_front(std::exception_ptr());
        threads.push_back(std::thread(
                    wrapper<decltype(&column_generation_2_vertical), column_generation_2_vertical>,
                    std::ref(exception_ptr_list.front()),
                    std::ref(instance),
                    std::ref(parameters),
                    std::ref(algorithm_formatter)));
        threads.push_back(std::thread(
                    wrapper<decltype(&column_generation_2_horizontal), column_generation_2_horizontal>,
                    std::ref(exception_ptr_list.front()),
                    std::ref(instance),
                    std::ref(parameters),
                    std::ref(algorithm_formatter)));
        for (Counter i = 0; i < (Counter)threads.size(); ++i)
            threads[i].join();
        for (std::exception_ptr exception_ptr: exception_ptr_list)
            if (exception_ptr)
                std::rethrow_exception(exception_ptr);
    }

    algorithm_formatter.end();
    return output;
}

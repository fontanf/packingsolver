#include "rectangleguillotine/column_generation_strips.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "rectangleguillotine/instance_flipper.hpp"
#include "rectangleguillotine/solution_builder.hpp"
#include "algorithms/thread_pool.hpp"

#include "packingsolver/rectangleguillotine/instance_builder.hpp"

#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

#include "columngenerationsolver/algorithms/limited_discrepancy_search.hpp"


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
            const Instance& instance,
            const ColumnGenerationStripsParameters& parameters,
            AlgorithmFormatter& algorithm_formatter,
            packingsolver::Output<Instance, Solution>* local_output):
        instance_(instance),
        parameters_(parameters),
        algorithm_formatter_(algorithm_formatter),
        local_output_(local_output),
        filled_demands_(instance.number_of_item_types())
    { }

    virtual std::vector<std::shared_ptr<const Column>> initialize_pricing(
            const std::vector<std::pair<std::shared_ptr<const Column>, Value>>& fixed_columns);

    virtual PricingOutput solve_pricing(
            const std::vector<Value>& duals);

    void set_all_columns_2e_patterns_generated() { all_columns_2e_patterns_generated_ = true; }

    void set_all_columns_2n_patterns_generated() { all_columns_2n_patterns_generated_ = true; }

    void set_all_columns_2r_patterns_generated() { all_columns_2r_patterns_generated_ = true; }

    void set_all_columns_3h_patterns_generated() { all_columns_3h_patterns_generated_ = true; }

private:

    void generate_1e_patterns(
            const std::vector<Value>& duals,
            PricingOutput& output,
            Value& reduced_cost_bound);

    void generate_1n_patterns(
            const std::vector<Value>& duals,
            PricingOutput& output,
            Value& reduced_cost_bound);

    void generate_1ro_patterns(
            const std::vector<Value>& duals,
            PricingOutput& output,
            Value& reduced_cost_bound);

    void generate_2ho_patterns(
            const std::vector<Value>& duals,
            PricingOutput& output,
            Value& reduced_cost_bound);

    void generate_lower_stage_patterns(
            const std::vector<Value>& duals,
            PricingOutput& output,
            Value& reduced_cost_bound);

    /**
     * Generate columns with all duals set to zero, i.e. using real profit
     * instead of reduced profit. This guarantees that the best real-profit
     * column for the current branch (in particular a single-strip solution,
     * since every generated strip is also considered as a candidate
     * solution) is found regardless of where the master problem's dual
     * values happen to be, since a real-profit-optimal column does not
     * always become reduced-cost-optimal at the dual values actually
     * visited by the search.
     */
    void generate_duals_zero(
            PricingOutput& output);

    const Instance& instance_;

    const ColumnGenerationStripsParameters& parameters_;

    AlgorithmFormatter& algorithm_formatter_;

    packingsolver::Output<Instance, Solution>* local_output_;

    std::vector<ItemPos> filled_demands_;

    Length filled_width_ = 0;

    bool all_columns_2e_patterns_generated_ = false;

    bool all_columns_2n_patterns_generated_ = false;

    bool all_columns_2r_patterns_generated_ = false;

    bool all_columns_3h_patterns_generated_ = false;

    /**
     * True from 'initialize_pricing' until the first 'solve_pricing' call has
     * generated columns using zero duals (i.e. real profit instead of
     * reduced profit). This guarantees that, for every diving branch, the
     * best real-profit column (in particular a single-strip solution, since
     * every generated strip is also considered as a candidate solution) is
     * always found regardless of where the master problem's dual values
     * happen to be, since real-profit-optimal columns do not always become
     * reduced-cost-optimal at the dual values actually visited by the
     * search.
     */
    bool first_pricing_ = true;

};

/**
 * Maximum width available for a single first-stage (1-cut) segment: the bin's
 * own remaining width (after 'filled_width' already used by other, fixed,
 * segments), further capped by 'maximum_distance_1_cuts' if set.
 *
 * 'maximum_distance_1_cuts' bounds the width of a single segment, not the
 * bin's total width budget, so it must be applied *after* subtracting
 * 'filled_width', not folded into a value that 'filled_width' is later
 * subtracted from (that would incorrectly shrink as more segments get
 * fixed, eventually going negative).
 */
Length first_stage_available_width(
        const Instance& instance,
        const BinType& bin_type,
        Length filled_width = 0)
{
    Length width = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim - filled_width;
    Length maximum_distance_1_cuts = instance.parameters().maximum_distance_1_cuts;
    if (maximum_distance_1_cuts != -1 && maximum_distance_1_cuts < width)
        width = maximum_distance_1_cuts;
    return width;
}

/**
 * Width of a first-stage (1-cut) segment whose content only requires
 * 'width', padded with waste up to 'minimum_distance_1_cuts' if set (the
 * segment's own boundary is free to sit further out than its content, same
 * as how tree_search pushes 1-cut positions outward to satisfy the
 * constraint instead of rejecting narrower placements).
 */
Length first_stage_padded_width(
        const Instance& instance,
        Length width)
{
    return (std::max)(width, instance.parameters().minimum_distance_1_cuts);
}

/**
 * Check that a solution satisfies the constraints supported by
 * 'column_generation_strips': minimum/maximum distance between 1-cuts and
 * 2-cuts, number of stages and item copies. Minimum waste length, maximum
 * number of 2-cuts, stacks and defects are not supported (the auto algorithm
 * selection in optimize() only enables this algorithm when they are
 * trivial), so they are not checked here.
 */
void check_feasibility(
        const Solution& solution,
        const std::string& func_signature)
{
    if (!solution.number_of_stages_feasible()) {
        throw std::logic_error(
                func_signature + ": solution doesn't satisfy number of stages.");
    }
    if (!solution.minimum_distance_1_cuts_feasible()) {
        throw std::logic_error(
                func_signature + ": solution doesn't satisfy minimum distance between 1-cuts.");
    }
    if (!solution.maximum_distance_1_cuts_feasible()) {
        throw std::logic_error(
                func_signature + ": solution doesn't satisfy maximum distance between 1-cuts.");
    }
    if (!solution.minimum_distance_2_cuts_feasible()) {
        throw std::logic_error(
                func_signature + ": solution doesn't satisfy minimum distance between 2-cuts.");
    }
    if (!solution.maximum_distance_2_cuts_feasible()) {
        throw std::logic_error(
                func_signature + ": solution doesn't satisfy maximum distance between 2-cuts.");
    }
    if (!solution.item_copies_feasible()) {
        throw std::logic_error(
                func_signature + ": solution doesn't satisfy item copies.");
    }
}

Column solution_to_column(
        const Solution& solution)
{
    //solution.format(std::cout, 3);
    const Instance& instance = solution.instance();
    const BinType& bin_type = instance.bin_type(0);
    double multiplier_length = largest_power_of_two_lesser_or_equal(bin_type.rect.w);
    double multiplier_profit = largest_power_of_two_lesser_or_equal(instance.largest_item_profit());
    Length cut_thickness = instance.parameters().cut_thickness;

    Column column;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);

        ItemPos copies = solution.item_copies(item_type_id);
        if (copies == 0)
            continue;

        if (instance.objective() == Objective::Knapsack)
            column.objective_coefficient += copies * item_type.profit / multiplier_profit;

        columngenerationsolver::LinearTerm element;
        element.row = item_type_id;
        element.coefficient = copies;
        column.elements.push_back(element);
    }
    Length width = solution.width() - bin_type.left_trim;
    if (instance.objective() == Objective::OpenDimensionX) {
        column.objective_coefficient = (double)(width + cut_thickness) / multiplier_length;
    } else {
        columngenerationsolver::LinearTerm element;
        element.row = instance.number_of_item_types();
        element.coefficient = (double)(width + cut_thickness) / multiplier_length;
        column.elements.push_back(element);
    }
    column.extra = std::shared_ptr<void>(new Solution(solution));

    //std::cout << column << std::endl;
    return column;
}

/**
 * Generate all columns for 1-staged roadef2018 patterns.
 *
 * When all items are oriented, 2-staged roadef2018 patterns can be generated by
 * solving a 0-1 knapsack problem. However, when some items might be rotated,
 * that doesn't hold anymore. In this case, we solve the Dantzig-Wolfe model of
 * the problem instead. Since the number of possible columns is small, we
 * generate them all at the beginning instead of using a column generation
 * scheme.
 */
std::vector<std::shared_ptr<const Column>> generate_all_columns_1r_patterns(
        const Instance& instance)
{
    const BinType& bin_type = instance.bin_type(0);
    Length available_width = first_stage_available_width(instance, bin_type);

    std::vector<std::shared_ptr<const Column>> columns;

    // Sort item types by width.
    std::vector<std::pair<ItemTypeId, bool>> sorted_item_type_ids;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        sorted_item_type_ids.push_back({item_type_id, false});
        if (!item_type.oriented)
            sorted_item_type_ids.push_back({item_type_id, true});
    }
    std::sort(
            sorted_item_type_ids.begin(),
            sorted_item_type_ids.end(),
            [&instance](
                const std::pair<ItemTypeId, bool>& p1,
                const std::pair<ItemTypeId, bool>& p2)
            {
                Length width_1 = (!p1.second)?
                    instance.item_type(p1.first).rect.w:
                    instance.item_type(p1.first).rect.h;
                Length width_2 = (!p2.second)?
                    instance.item_type(p2.first).rect.w:
                    instance.item_type(p2.first).rect.h;
                return width_1 < width_2;
            });
    for (ItemPos pos_1 = 0;
            pos_1 < (ItemPos)sorted_item_type_ids.size();
            ++pos_1) {
        ItemTypeId item_type_id_1 = sorted_item_type_ids[pos_1].first;
        const ItemType& item_type_1 = instance.item_type(item_type_id_1);
        Length width_1 = (!sorted_item_type_ids[pos_1].second)?
            item_type_1.rect.w:
            item_type_1.rect.h;
        Length height_1 = (!sorted_item_type_ids[pos_1].second)?
            item_type_1.rect.h:
            item_type_1.rect.w;

        if (height_1 > bin_type.rect.h)
            continue;
        if (width_1 > available_width)
            continue;
        // No depth-3 narrowing cut is used here (the item is expected to
        // span the segment exactly), so a segment narrower than
        // 'minimum_distance_1_cuts' can't be padded: skip it entirely.
        if (width_1 < instance.parameters().minimum_distance_1_cuts)
            continue;

        // Retrieve strip.
        SolutionBuilder extra_solution_builder(instance);
        extra_solution_builder.add_bin(0, 1, CutOrientation::Vertical);
        extra_solution_builder.add_node(1, bin_type.left_trim + width_1);
        extra_solution_builder.add_node(2, bin_type.bottom_trim + height_1);
        extra_solution_builder.set_last_node_item(item_type_id_1);
        Solution extra_solution = extra_solution_builder.build();

        // Build column.
        Column column = solution_to_column(extra_solution);
        columns.push_back(std::shared_ptr<const Column>(new Column(column)));

        for (ItemPos pos_2 = pos_1;
                pos_2 < (ItemPos)sorted_item_type_ids.size();
                ++pos_2) {
            ItemTypeId item_type_id_2 = sorted_item_type_ids[pos_2].first;
            const ItemType& item_type_2 = instance.item_type(item_type_id_2);
            Length width_2 = (!sorted_item_type_ids[pos_2].second)?
                item_type_2.rect.w:
                item_type_2.rect.h;
            Length height_2 = (!sorted_item_type_ids[pos_2].second)?
                item_type_2.rect.h:
                item_type_2.rect.w;

            if (width_1 != width_2)
                break;
            if (item_type_id_1 == item_type_id_2
                    && item_type_1.copies == 1) {
                continue;
            }
            if (bin_type.bottom_trim
                    + height_1
                    + instance.parameters().cut_thickness
                    + height_2
                    + bin_type.top_trim
                    != bin_type.rect.h)
                continue;

            // Retrieve strip.
            SolutionBuilder extra_solution_builder(instance);
            extra_solution_builder.add_bin(0, 1, CutOrientation::Vertical);
            extra_solution_builder.add_node(1, bin_type.left_trim + width_1);
            extra_solution_builder.add_node(2, bin_type.bottom_trim + height_1);
            extra_solution_builder.set_last_node_item(item_type_id_1);
            extra_solution_builder.add_node(2, bin_type.bottom_trim + height_1 + instance.parameters().cut_thickness + height_2);
            extra_solution_builder.set_last_node_item(item_type_id_2);
            Solution extra_solution = extra_solution_builder.build();

            // Build column.
            Column column = solution_to_column(extra_solution);
            columns.push_back(std::shared_ptr<const Column>(new Column(column)));
        }
    }

    return columns;
}

/**
 * Generate all columns for 2-staged homogenous patterns.
 *
 * When all items are oriented, 3-staged homogenous patterns can be generated by
 * solving a 0-1 knapsack problem. However, when some items might be rotated,
 * that doesn't hold anymore. In this case, we solve the Dantzig-Wolfe model of
 * the problem instead. Since the number of possible columns is small, we
 * generate them all at the beginning instead of using a column generation
 * scheme.
 */
std::vector<std::shared_ptr<const Column>> generate_all_columns_2h_patterns(
        const Instance& instance)
{
    //std::cout << "generate_2hr_patterns..." << std::endl;
    const BinType& bin_type = instance.bin_type(0);
    Length available_width = first_stage_available_width(instance, bin_type);

    std::vector<std::shared_ptr<const Column>> columns;

    // Sort item types by width.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        //std::cout << "item_type_id " << item_type_id
        //    << " h " << item_type.rect.h << std::endl;

        ItemPos copies_max = (bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim + instance.parameters().cut_thickness)
            / (item_type.rect.h + instance.parameters().cut_thickness);
        copies_max = std::min(copies_max, item_type.copies);
        //std::cout
        //    << "bin_type.h " << bin_type.rect.h
        //    << " copies_max " << copies_max
        //    << std::endl;
        // No depth-3 narrowing cut is used here (each item is expected to
        // span the segment exactly), so a segment narrower than
        // 'minimum_distance_1_cuts' can't be padded: skip it entirely.
        if (item_type.rect.w <= available_width
                && item_type.rect.w >= instance.parameters().minimum_distance_1_cuts) {
            for (ItemPos copies = 1; copies <= copies_max; ++copies) {

                // Build strip.
                //std::cout << "build strip no rotation..." << std::endl;
                //std::cout << "copies " << copies << std::endl;
                SolutionBuilder extra_solution_builder(instance);
                extra_solution_builder.add_bin(0, 1, CutOrientation::Vertical);
                //std::cout << "add_node 1 " << bin_type.left_trim + item_type.rect.w << std::endl;
                extra_solution_builder.add_node(1, bin_type.left_trim + item_type.rect.w);
                Length cut_position = bin_type.bottom_trim;
                for (ItemPos copy = 0; copy < copies; ++copy) {
                    cut_position += item_type.rect.h;
                    //std::cout << "add_node 2 " << cut_position << std::endl;
                    extra_solution_builder.add_node(2, cut_position);
                    extra_solution_builder.set_last_node_item(item_type_id);
                    cut_position += instance.parameters().cut_thickness;
                }
                Solution extra_solution = extra_solution_builder.build();

                // Build column.
                Column column = solution_to_column(extra_solution);
                columns.push_back(std::shared_ptr<const Column>(new Column(column)));
            }
        }

        if (!item_type.oriented
                && item_type.rect.h <= available_width
                && item_type.rect.h >= instance.parameters().minimum_distance_1_cuts) {
            ItemPos copies_max = (bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim + instance.parameters().cut_thickness)
                / (item_type.rect.w + instance.parameters().cut_thickness);
            copies_max = std::min(copies_max, item_type.copies);
            for (ItemPos copies = 1; copies <= copies_max; ++copies) {

                // Build strip.
                //std::cout << "build strip rotation..." << std::endl;
                SolutionBuilder extra_solution_builder(instance);
                extra_solution_builder.add_bin(0, 1, CutOrientation::Vertical);
                extra_solution_builder.add_node(1, bin_type.left_trim + item_type.rect.h);
                Length cut_position = bin_type.bottom_trim;
                for (ItemPos copy = 0; copy < copies; ++copy) {
                    cut_position += item_type.rect.w;
                    extra_solution_builder.add_node(2, cut_position);
                    extra_solution_builder.set_last_node_item(item_type_id);
                    cut_position += instance.parameters().cut_thickness;
                }
                Solution extra_solution = extra_solution_builder.build();

                // Build column.
                Column column = solution_to_column(extra_solution);
                columns.push_back(std::shared_ptr<const Column>(new Column(column)));
            }
        }
    }

    //std::cout << "generate_2hr_patterns end" << std::endl;
    return columns;
}

/**
 * Generate all columns for 2-staged exact patterns.
 *
 * Stop if too many columns are generated.
 */
std::vector<std::shared_ptr<const Column>> generate_all_columns_2e_patterns(
        const Instance& instance)
{
    // TODO
    return {};
}

/**
 * Generate all columns for 2-staged non-exact patterns.
 *
 * Stop if too many columns are generated.
 */
std::vector<std::shared_ptr<const Column>> generate_all_columns_2n_patterns(
        const Instance& instance)
{
    // TODO
    return {};
}

/**
 * Generate all columns for 2-staged roadef2018 patterns.
 *
 * Stop if too many columns are generated.
 */
std::vector<std::shared_ptr<const Column>> generate_all_columns_2r_patterns(
        const Instance& instance)
{
    // TODO
    return {};
}

/**
 * Generate all columns for 3-staged homogenous patterns.
 *
 * Stop if too many patterns are generated.
 */
std::vector<std::shared_ptr<const Column>> generate_all_columns_3h_patterns(
        const Instance& instance)
{
    // TODO
    return {};
}

struct GetModelOutput
{
    columngenerationsolver::Model model;
    std::vector<std::shared_ptr<const Column>> column_pool;
};

GetModelOutput get_model(
        const Instance& instance,
        const ColumnGenerationStripsParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        packingsolver::Output<Instance, Solution>* local_output)
{
    const BinType& bin_type = instance.bin_type(0);
    double multiplier_length = largest_power_of_two_lesser_or_equal(bin_type.rect.w);

    GetModelOutput output;
    columngenerationsolver::Model& model = output.model;
    std::vector<std::shared_ptr<const Column>>& column_pool = output.column_pool;

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
        row.upper_bound = (double)(bin_type.rect.w
            - bin_type.left_trim
            - bin_type.right_trim
            + instance.parameters().cut_thickness)
            / multiplier_length;
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

    if (instance.parameters().number_of_stages == 1
            && instance.parameters().cut_type == CutType::Roadef2018) {
        for (const auto& column: generate_all_columns_1r_patterns(instance))
            column_pool.push_back(column);
    }
    if (instance.parameters().number_of_stages == 2
            && instance.parameters().cut_type == CutType::Homogenous) {
        for (const auto& column: generate_all_columns_2h_patterns(instance))
            column_pool.push_back(column);
    }

    // Pricing solver.
    model.pricing_solver = std::unique_ptr<columngenerationsolver::PricingSolver>(
            new ColumnGenerationPricingSolver(instance, parameters, algorithm_formatter, local_output));

    // Try to generate all columns.

    if ((instance.parameters().number_of_stages == 2
                && instance.parameters().cut_type == CutType::Exact)
            || (instance.parameters().number_of_stages == 2
                && instance.parameters().cut_type == CutType::NonExact)
            || (instance.parameters().number_of_stages == 2
                && instance.parameters().cut_type == CutType::Roadef2018)
            || instance.parameters().number_of_stages >= 3) {
        auto columns = generate_all_columns_2e_patterns(instance);
        if (!columns.empty()) {
            for (const auto& column: columns)
                column_pool.push_back(column);
            static_cast<ColumnGenerationPricingSolver&>(*model.pricing_solver).set_all_columns_2e_patterns_generated();
        }
    }

    if ((instance.parameters().number_of_stages == 2
                && instance.parameters().cut_type == CutType::NonExact)
            || (instance.parameters().number_of_stages == 2
                && instance.parameters().cut_type == CutType::Roadef2018)
            || instance.parameters().number_of_stages >= 3) {
        auto columns = generate_all_columns_2n_patterns(instance);
        if (!columns.empty()) {
            for (const auto& column: columns)
                column_pool.push_back(column);
            static_cast<ColumnGenerationPricingSolver&>(*model.pricing_solver).set_all_columns_2n_patterns_generated();
        }
    }

    if (instance.parameters().number_of_stages == 2
            && instance.parameters().cut_type == CutType::Roadef2018
            && instance.all_item_types_oriented()) {
        auto columns = generate_all_columns_2r_patterns(instance);
        if (!columns.empty()) {
            for (const auto& column: columns)
                column_pool.push_back(column);
            static_cast<ColumnGenerationPricingSolver&>(*model.pricing_solver).set_all_columns_2r_patterns_generated();
        }
    }

    if (instance.parameters().number_of_stages >= 3) {
        auto columns = generate_all_columns_3h_patterns(instance);
        if (!columns.empty()) {
            for (const auto& column: columns)
                column_pool.push_back(column);
            static_cast<ColumnGenerationPricingSolver&>(*model.pricing_solver).set_all_columns_3h_patterns_generated();
        }
    }

    // Every generated strip is, on its own, already a complete single-bin
    // solution (the rest of the bin auto-fills as waste): consider each one
    // directly as a candidate solution, so the algorithm finds the optimum
    // when it consists of a single strip.
    if (instance.objective() == Objective::Knapsack) {
        for (const std::shared_ptr<const Column>& column: column_pool) {
            const Solution& solution = *std::static_pointer_cast<Solution>(column->extra);
            check_feasibility(solution, FUNC_SIGNATURE);
            if (local_output != nullptr) {
                local_output->solution_pool.add(solution, "V strip");
            } else {
                algorithm_formatter.update_solution(solution, "V strip");
            }
        }
    }

    return output;
}

std::vector<std::shared_ptr<const Column>> ColumnGenerationPricingSolver::initialize_pricing(
        const std::vector<std::pair<std::shared_ptr<const Column>, Value>>& fixed_columns)
{
    const BinType& bin_type = instance_.bin_type(0);
    double multiplier_length = largest_power_of_two_lesser_or_equal(bin_type.rect.w);
    std::fill(filled_demands_.begin(), filled_demands_.end(), 0);
    filled_width_ = 0;
    first_pricing_ = true;
    for (auto p: fixed_columns) {
        const Column& column = *(p.first);
        Value value = p.second;
        for (const columngenerationsolver::LinearTerm& element: column.elements) {
            if (element.row < instance_.number_of_item_types()) {
                ItemTypeId item_type_id = element.row;
                ItemPos copies = instance_.item_type(item_type_id).copies;
                filled_demands_[item_type_id] += std::round(value) * std::round(element.coefficient);
                if (filled_demands_[item_type_id] > copies) {
                    throw std::logic_error(
                            FUNC_SIGNATURE + "; "
                            "item_type_id: " + std::to_string(item_type_id) + "; "
                            "copies: " + std::to_string(copies) + "; "
                            "filled_demands: " + std::to_string(filled_demands_[item_type_id]) + ".");
                }
            } else {
                filled_width_ += std::round(value) * multiplier_length * element.coefficient;
            }
        }
    }
    //std::cout << "initialize_pricing end" << std::endl;
    return {};
}

void ColumnGenerationPricingSolver::generate_1e_patterns(
        const std::vector<Value>& duals,
        PricingOutput& output,
        Value& reduced_cost_bound)
{
    //std::cout << "generate_1e_patterns..." << std::endl;
    const BinType& bin_type = instance_.bin_type(0);
    double multiplier_profit = largest_power_of_two_lesser_or_equal(instance_.largest_item_profit());
    Length cut_thickness = instance_.parameters().cut_thickness;
    Length width = first_stage_available_width(instance_, bin_type, filled_width_);
    Length height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    for (;;) {
        // Any pattern found below this point would be padded back up to
        // 'minimum_distance_1_cuts' anyway, so it's not worth searching.
        if (width < instance_.parameters().minimum_distance_1_cuts)
            break;
        //std::cout << "1E width " << width << std::endl;

        // Build one-dimensional knapsack instance.
        onedimensional::InstanceBuilder kp_instance_builder;
        kp_instance_builder.set_objective(Objective::Knapsack);
        kp_instance_builder.add_bin_type(height + cut_thickness);
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
                profit = item_type.profit - duals[item_type_id] * multiplier_profit;
            }
            if (profit <= 0)
                continue;

            // A row's height (the distance between the 2-cuts bounding it)
            // is the item's own height in that orientation and can't be
            // shrunk, so an item taller than 'maximum_distance_2_cuts' must
            // be excluded outright.
            if (item_type.rect.w == width
                    && item_type.rect.h <= height
                    && (instance_.parameters().maximum_distance_2_cuts == -1
                        || item_type.rect.h <= instance_.parameters().maximum_distance_2_cuts)) {
                ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(
                        item_type.rect.h + cut_thickness);
                kp_instance_builder.set_item_type_profit(kp_item_type_id, profit);
                kp_instance_builder.set_item_type_copies(kp_item_type_id, copies);
                kp2orig.push_back(item_type_id);
            } else if (!item_type.oriented
                    && item_type.rect.h == width
                    && item_type.rect.w <= height
                    && (instance_.parameters().maximum_distance_2_cuts == -1
                        || item_type.rect.w <= instance_.parameters().maximum_distance_2_cuts)) {
                ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(
                        item_type.rect.w + cut_thickness);
                kp_instance_builder.set_item_type_profit(kp_item_type_id, profit);
                kp_instance_builder.set_item_type_copies(kp_item_type_id, copies);
                kp2orig.push_back(item_type_id);
            }
        }
        onedimensional::Instance kp_instance = kp_instance_builder.build();

        if (kp_instance.number_of_item_types() > 0) {

            // Solve one-dimensional knapsack problem.
            onedimensional::OptimizeParameters kp_parameters;
            kp_parameters.verbosity_level = 0;
            auto kp_output = optimize(kp_instance, kp_parameters);
            if (parameters_.timer.needs_to_end())
                break;

            // Retrieve solution.
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

                for (ItemPos copy = 0; copy < copies; ++copy) {
                    cut_position += (item_type.rect.w == width)?
                        item_type.rect.h:
                        item_type.rect.w;
                    extra_solution_builder.add_node(2, cut_position);
                    extra_solution_builder.set_last_node_item(item_type_id);
                    cut_position += cut_thickness;
                }
            }
            Solution extra_solution = extra_solution_builder.build();

            // Retrieve column.
            Column column = solution_to_column(extra_solution);
            output.columns.push_back(std::shared_ptr<const Column>(new Column(column)));
            if (instance_.parameters().number_of_stages == 3
                    && instance_.parameters().cut_type == CutType::Homogenous) {
                // An improving column has rc > 0 for a Maximize model
                // (Knapsack) but rc < 0 for a Minimize model
                // (OpenDimensionX); accumulate 'reduced_cost_bound' via max
                // for the former, min for the latter, so it always tracks
                // the most improving reduced cost found so far.
                Value rc = columngenerationsolver::compute_reduced_cost(column, duals);
                if (instance_.objective() == Objective::Knapsack) {
                    reduced_cost_bound = (std::max)(reduced_cost_bound, rc);
                } else if (instance_.objective() == Objective::OpenDimensionX) {
                    reduced_cost_bound = (std::min)(reduced_cost_bound, rc);
                }
            }

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
                profit = item_type.profit - duals[item_type_id] * multiplier_profit;
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
    //std::cout << "generate_1e_patterns end" << std::endl;
}

void ColumnGenerationPricingSolver::generate_1n_patterns(
        const std::vector<Value>& duals,
        PricingOutput& output,
        Value& reduced_cost_bound)
{
    //std::cout << "generate_1n_patterns..." << std::endl;
    const BinType& bin_type = instance_.bin_type(0);
    double multiplier_profit = largest_power_of_two_lesser_or_equal(instance_.largest_item_profit());
    Length cut_thickness = instance_.parameters().cut_thickness;
    Length width = first_stage_available_width(instance_, bin_type, filled_width_);
    Length height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    for (;;) {
        // Any pattern found below this point would be padded back up to
        // 'minimum_distance_1_cuts' anyway, so it's not worth searching.
        if (width < instance_.parameters().minimum_distance_1_cuts)
            break;
        //std::cout << "1N width " << width << std::endl;

        // Build one-dimensional knapsack instance.
        onedimensional::InstanceBuilder kp_instance_builder;
        kp_instance_builder.set_objective(Objective::Knapsack);
        kp_instance_builder.add_bin_type(height + cut_thickness);
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
                profit = item_type.profit - duals[item_type_id] * multiplier_profit;
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

            // A row's height (the distance between the 2-cuts bounding it)
            // is 'item_height' in the chosen orientation and can't be
            // shrunk, so an item taller than 'maximum_distance_2_cuts' must
            // be excluded outright.
            if (item_width <= width
                    && (instance_.parameters().maximum_distance_2_cuts == -1
                        || item_height <= instance_.parameters().maximum_distance_2_cuts)) {
                ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(
                        item_height + cut_thickness);
                kp_instance_builder.set_item_type_profit(kp_item_type_id, profit);
                kp_instance_builder.set_item_type_copies(kp_item_type_id, copies);
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
        if (parameters_.timer.needs_to_end())
            break;

        // Retrieve width_max.
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
        }
        // Pad up to 'minimum_distance_1_cuts' *before* it is used below to
        // decide which items need a depth-3 narrowing cut: otherwise an item
        // exactly as wide as the original (unpadded) width_max would wrongly
        // be considered to span the (now wider) segment exactly.
        width_max = (std::max)(width_max, instance_.parameters().minimum_distance_1_cuts);

        // Retrieve solution.
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
                cut_position += kp_instance.item_type(kp_item_type_id).length
                    - cut_thickness;
                extra_solution_builder.add_node(2, cut_position);
                if (width_cur < width_max)
                    extra_solution_builder.add_node(3, bin_type.left_trim + width_cur);
                extra_solution_builder.set_last_node_item(item_type_id);
                cut_position += cut_thickness;
            }
        }
        Solution extra_solution = extra_solution_builder.build();

        // Retrieve column.
        Column column = solution_to_column(extra_solution);
        output.columns.push_back(std::shared_ptr<const Column>(new Column(column)));
        if (instance_.parameters().number_of_stages == 2
                && instance_.parameters().cut_type == CutType::Exact) {
            // An improving column has rc > 0 for a Maximize model
            // (Knapsack) but rc < 0 for a Minimize model (OpenDimensionX);
            // accumulate 'reduced_cost_bound' via max for the former, min
            // for the latter, so it always tracks the most improving
            // reduced cost found so far.
            Value rc = columngenerationsolver::compute_reduced_cost(column, duals);
            if (instance_.objective() == Objective::Knapsack) {
                reduced_cost_bound = (std::max)(reduced_cost_bound, rc);
            } else if (instance_.objective() == Objective::OpenDimensionX) {
                reduced_cost_bound = (std::min)(reduced_cost_bound, rc);
            }
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
                profit = item_type.profit - duals[item_type_id] * multiplier_profit;
            }
            if (!strictly_greater(profit, 0.0))
                continue;

            if (item_type.rect.w < width_max
                    && item_type.rect.h <= height
                    && width_new < item_type.rect.w) {
                width_new = item_type.rect.w;
            }
            if (!item_type.oriented
                    && item_type.rect.h < width_max
                    && item_type.rect.w <= height
                    && width_new < item_type.rect.h) {
                width_new = item_type.rect.h;
            }
        }
        if (width_new == 0)
            break;
        width = width_new;
    }
    //std::cout << "generate_1n_patterns end" << std::endl;
}

void ColumnGenerationPricingSolver::generate_1ro_patterns(
        const std::vector<Value>& duals,
        PricingOutput& output,
        Value& reduced_cost_bound)
{
    //std::cout << "generate_1ro_patterns..." << std::endl;
    const BinType& bin_type = instance_.bin_type(0);
    double multiplier_profit = largest_power_of_two_lesser_or_equal(instance_.largest_item_profit());
    Length cut_thickness = instance_.parameters().cut_thickness;
    Length width = first_stage_available_width(instance_, bin_type, filled_width_);
    Length height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;

    // Sort items by height and then by profit.
    std::vector<std::pair<ItemTypeId, double>> sorted_item_type_ids;
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
            profit = item_type.profit - duals[item_type_id] * multiplier_profit;
        }
        if (!strictly_greater(profit, 0.0))
            continue;

        sorted_item_type_ids.push_back({item_type_id, profit});
    }
    std::sort(
            sorted_item_type_ids.begin(),
            sorted_item_type_ids.end(),
            [this](
                const std::pair<ItemTypeId, bool>& p1,
                const std::pair<ItemTypeId, bool>& p2)
            {
                const ItemType& item_type_1 = instance_.item_type(p1.first);
                const ItemType& item_type_2 = instance_.item_type(p2.first);
                if (item_type_1.rect.h != item_type_2.rect.h)
                    return item_type_1.rect.h < item_type_2.rect.h;
                return p1.second > p2.second;
            });
    //for (const auto& p: sorted_item_type_ids)
    //    std::cout << "item_type_id " << p.first << " profit " << p.second << std::endl;

    for (;;) {
        // Any pattern found below this point would be padded back up to
        // 'minimum_distance_1_cuts' anyway, so it's not worth searching.
        if (width < instance_.parameters().minimum_distance_1_cuts)
            break;
        //std::cout << "2RO width " << width << std::endl;

        std::vector<ItemPos> remaining_copies(instance_.number_of_item_types(), 0);
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance_.item_type(item_type_id);
            ItemPos copies = item_type.copies - filled_demands_[item_type_id];
            if (copies == 0)
                continue;
            remaining_copies[item_type_id] = copies;
        }

        // Build one-dimensional knapsack instance.
        onedimensional::InstanceBuilder kp_instance_builder;
        kp_instance_builder.set_objective(Objective::Knapsack);
        kp_instance_builder.add_bin_type(height + cut_thickness);
        std::vector<std::pair<ItemTypeId, ItemTypeId>> kp2orig;
        for (ItemTypeId item_type_pos_1 = 0;
                item_type_pos_1 < (ItemTypeId)sorted_item_type_ids.size();
                ++item_type_pos_1) {
            ItemTypeId item_type_id_1 = sorted_item_type_ids[item_type_pos_1].first;
            Profit profit_1 = sorted_item_type_ids[item_type_pos_1].second;
            const ItemType& item_type_1 = instance_.item_type(item_type_id_1);
            if (item_type_1.rect.h > height)
                continue;
            if (item_type_1.rect.w > width)
                continue;

            for (ItemTypeId item_type_pos_2 = item_type_pos_1;
                    item_type_pos_2 < (ItemTypeId)sorted_item_type_ids.size();
                    ++item_type_pos_2) {
                ItemTypeId item_type_id_2 = sorted_item_type_ids[item_type_pos_2].first;
                const ItemType& item_type_2 = instance_.item_type(item_type_id_2);
                Profit profit_2 = sorted_item_type_ids[item_type_pos_2].second;

                // Items must be of the same height.
                if (item_type_2.rect.h != item_type_1.rect.h)
                    break;

                // The sum of their widths must match the strip width.
                if (item_type_1.rect.w
                        + cut_thickness
                        + item_type_2.rect.w
                        != width) {
                    continue;
                }

                if (item_type_id_1 == item_type_id_2
                        && remaining_copies[item_type_id_1] == 1)
                    continue;

                ItemPos copies = (item_type_id_1 == item_type_id_2)?
                    remaining_copies[item_type_id_1] / 2:
                    std::min(remaining_copies[item_type_id_1], remaining_copies[item_type_id_2]);
                if (copies == 0)
                    continue;

                //std::cout << "add_item_type " << item_type_id_1
                //    << " " << item_type_id_2
                //    << " profit " << profit_1 + profit_2
                //    << std::endl;
                ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(
                        item_type_1.rect.h + cut_thickness);
                kp_instance_builder.set_item_type_profit(kp_item_type_id, profit_1 + profit_2);
                kp_instance_builder.set_item_type_copies(kp_item_type_id, copies);
                kp2orig.push_back({item_type_id_1, item_type_id_2});

                remaining_copies[item_type_id_1] -= copies;
                remaining_copies[item_type_id_2] -= copies;
            }
            if (remaining_copies[item_type_id_1] > 0) {
                //std::cout << "add_item_type " << item_type_id_1 << std::endl;
                ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(
                        item_type_1.rect.h);
                kp_instance_builder.set_item_type_profit(kp_item_type_id, profit_1);
                kp_instance_builder.set_item_type_copies(kp_item_type_id, remaining_copies[item_type_id_1]);
                kp2orig.push_back({item_type_id_1, -1});
            }
        }
        onedimensional::Instance kp_instance = kp_instance_builder.build();
        if (kp_instance.number_of_item_types() == 0)
            break;

        // Solve one-dimensional knapsack problem.
        onedimensional::OptimizeParameters kp_parameters;
        kp_parameters.verbosity_level = 0;
        auto kp_output = optimize(kp_instance, kp_parameters);
        if (parameters_.timer.needs_to_end())
            break;

        // Retrieve width_max.
        Length width_max = 0;
        for (ItemTypeId kp_item_type_id = 0;
                kp_item_type_id < kp_instance.number_of_item_types();
                ++kp_item_type_id) {
            ItemTypeId item_type_id_1 = kp2orig[kp_item_type_id].first;
            ItemTypeId item_type_id_2 = kp2orig[kp_item_type_id].second;

            ItemPos kp_copies = kp_output.solution_pool.best().item_copies(kp_item_type_id);
            if (kp_copies == 0)
                continue;

            const ItemType& item_type_1 = instance_.item_type(item_type_id_1);
            Length width_cur = item_type_1.rect.w;
            //std::cout << "kp_item_type_id " << kp_item_type_id
            //    << " kp_copies " << kp_copies
            //    << " item_type_id_1 " << item_type_id_1
            //    << " w " << item_type_1.rect.w
            //    << " item_type_id_2 " << item_type_id_2
            //    << std::endl;
            if (item_type_id_2 != -1) {
                const ItemType& item_type_2 = instance_.item_type(item_type_id_2);
                width_cur += cut_thickness + item_type_2.rect.w;
            }
            if (width_max < width_cur)
                width_max = width_cur;
        }
        // Pad up to 'minimum_distance_1_cuts' *before* it is used below to
        // decide which items need a depth-3 narrowing cut: otherwise an item
        // exactly as wide as the original (unpadded) width_max would wrongly
        // be considered to span the (now wider) segment exactly.
        width_max = (std::max)(width_max, instance_.parameters().minimum_distance_1_cuts);

        // Build extra solution.
        //std::cout << "build extra solution width_max " << width_max << std::endl;
        SolutionBuilder extra_solution_builder(instance_);
        extra_solution_builder.add_bin(0, 1, CutOrientation::Vertical);
        extra_solution_builder.add_node(1, bin_type.left_trim + width_max);
        Length cut_position = bin_type.bottom_trim;
        for (ItemTypeId kp_item_type_id = 0;
                kp_item_type_id < kp_instance.number_of_item_types();
                ++kp_item_type_id) {
            ItemTypeId item_type_id_1 = kp2orig[kp_item_type_id].first;
            const ItemType& item_type_1 = instance_.item_type(item_type_id_1);

            ItemPos kp_copies = kp_output.solution_pool.best().item_copies(kp_item_type_id);
            if (kp_copies == 0)
                continue;

            for (ItemPos kp_copy = 0; kp_copy < kp_copies; ++kp_copy) {
                cut_position += item_type_1.rect.h;
                extra_solution_builder.add_node(2, cut_position);

                if (item_type_1.rect.w < width_max) {
                    extra_solution_builder.add_node(
                            3,
                            bin_type.left_trim
                            + item_type_1.rect.w);
                }
                extra_solution_builder.set_last_node_item(item_type_id_1);

                ItemTypeId item_type_id_2 = kp2orig[kp_item_type_id].second;
                if (item_type_id_2 != -1) {
                    const ItemType& item_type_2 = instance_.item_type(item_type_id_2);
                    extra_solution_builder.add_node(
                            3,
                            bin_type.left_trim
                            + item_type_1.rect.w
                            + cut_thickness
                            + item_type_2.rect.w);
                    extra_solution_builder.set_last_node_item(item_type_id_2);
                }
                cut_position += cut_thickness;
            }
        }
        Solution extra_solution = extra_solution_builder.build();

        // Retrieve column.
        Column column = solution_to_column(extra_solution);
        output.columns.push_back(std::shared_ptr<const Column>(new Column(column)));
        if (instance_.parameters().number_of_stages == 3
                && instance_.parameters().cut_type == CutType::Homogenous
                && instance_.all_item_types_oriented()) {
            // An improving column has rc > 0 for a Maximize model
            // (Knapsack) but rc < 0 for a Minimize model (OpenDimensionX);
            // accumulate 'reduced_cost_bound' via max for the former, min
            // for the latter, so it always tracks the most improving
            // reduced cost found so far.
            Value rc = columngenerationsolver::compute_reduced_cost(column, duals);
            if (instance_.objective() == Objective::Knapsack) {
                reduced_cost_bound = (std::max)(reduced_cost_bound, rc);
            } else if (instance_.objective() == Objective::OpenDimensionX) {
                reduced_cost_bound = (std::min)(reduced_cost_bound, rc);
            }
        }

        // Find the largest width strictly smaller than the largest width
        // in the generated column.
        Length width_new = 0;
        // If 'width_max < width', we may still needs to try with a new width of
        // 'width_max' since item pairs valid with width 'width_max' were not
        // valid with width 'width'.
        if (width_max < width)
            width_max++;
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
                profit = item_type.profit - duals[item_type_id] * multiplier_profit;
            }
            if (!strictly_greater(profit, 0.0))
                continue;

            ItemPos number_of_copies_in_full_strip = (width_max - 1) / item_type.rect.w;
            if (number_of_copies_in_full_strip == 0)
                continue;
            if (item_type.rect.h > height)
                continue;
            ItemPos number_of_full_strips = copies / number_of_copies_in_full_strip;
            ItemPos number_of_copies_in_last_strip = copies % number_of_copies_in_full_strip;
            Length width_cur = (number_of_full_strips > 0)?
                number_of_copies_in_full_strip * item_type.rect.w:
                number_of_copies_in_last_strip * item_type.rect.w;
            //std::cout << "wj " << item_type.rect.w
            //    << " copies " << copies
            //    << " number_of_copies_in_full_strip " << number_of_full_strips
            //    << " number_of_full_strips " << number_of_full_strips
            //    << " number_of_copies_in_last_strip " << number_of_copies_in_last_strip
            //    << " wcur " << width_cur
            //    << std::endl;

            if (width_cur < width_max
                    && width_new < width_cur) {
                width_new = width_cur;
            }
        }
        if (width_new == 0)
            break;
        width = width_new;
    }
    //std::cout << "generate_1ro_patterns end" << std::endl;
}

void ColumnGenerationPricingSolver::generate_2ho_patterns(
        const std::vector<Value>& duals,
        PricingOutput& output,
        Value& reduced_cost_bound)
{
    //std::cout << "generate_2ho_patterns..." << std::endl;
    const BinType& bin_type = instance_.bin_type(0);
    double multiplier_profit = largest_power_of_two_lesser_or_equal(instance_.largest_item_profit());
    Length cut_thickness = instance_.parameters().cut_thickness;
    Length width = first_stage_available_width(instance_, bin_type, filled_width_);
    Length height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    for (;;) {
        // Any pattern found below this point would be padded back up to
        // 'minimum_distance_1_cuts' anyway, so it's not worth searching.
        if (width < instance_.parameters().minimum_distance_1_cuts)
            break;
        //std::cout << "2HO width " << width << std::endl;

        // Build one-dimensional knapsack instance.
        onedimensional::InstanceBuilder kp_instance_builder;
        kp_instance_builder.set_objective(Objective::Knapsack);
        kp_instance_builder.add_bin_type(height + cut_thickness);
        std::vector<std::pair<ItemTypeId, ItemPos>> kp2orig;
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
                profit = item_type.profit - duals[item_type_id] * multiplier_profit;
            }
            if (!strictly_greater(profit, 0.0))
                continue;

            ItemPos number_of_copies_in_full_strip = (width + cut_thickness)
                / (item_type.rect.w + cut_thickness);
            if (number_of_copies_in_full_strip == 0)
                continue;
            if (item_type.rect.h > height)
                continue;
            // Each row is homogeneous in a single item type, so its height
            // (the distance between the 2-cuts bounding it) can't be shrunk
            // below the item's own height: an item taller than
            // 'maximum_distance_2_cuts' can never appear in a 2ho pattern.
            if (instance_.parameters().maximum_distance_2_cuts != -1
                    && item_type.rect.h > instance_.parameters().maximum_distance_2_cuts)
                continue;
            ItemPos number_of_full_strips = copies / number_of_copies_in_full_strip;
            ItemPos number_of_copies_in_last_strip = copies % number_of_copies_in_full_strip;
            Length width_cur = (number_of_full_strips > 0)?
                number_of_copies_in_full_strip * item_type.rect.w
                + (number_of_copies_in_full_strip - 1) * cut_thickness:
                number_of_copies_in_last_strip * item_type.rect.w
                + (number_of_copies_in_last_strip - 1) * cut_thickness;
            //std::cout << "wj " << item_type.rect.w
            //    << " copies " << copies
            //    << " number_of_copies_in_full_strip " << number_of_copies_in_full_strip
            //    << " number_of_full_strips " << number_of_full_strips
            //    << " number_of_copies_in_last_strip " << number_of_copies_in_last_strip
            //    << " wcur " << width_cur
            //    << std::endl;

            if (number_of_full_strips > 0) {
                ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(
                        item_type.rect.h + cut_thickness);
                kp_instance_builder.set_item_type_profit(kp_item_type_id, number_of_copies_in_full_strip * profit);
                kp_instance_builder.set_item_type_copies(kp_item_type_id, number_of_full_strips);
                kp2orig.push_back({item_type_id, number_of_copies_in_full_strip});
            }
            if (number_of_copies_in_last_strip > 0) {
                ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(
                        item_type.rect.h + cut_thickness);
                kp_instance_builder.set_item_type_profit(kp_item_type_id, number_of_copies_in_last_strip * profit);
                kp_instance_builder.set_item_type_copies(kp_item_type_id, 1);
                kp2orig.push_back({item_type_id, number_of_copies_in_last_strip});
            }
        }
        onedimensional::Instance kp_instance = kp_instance_builder.build();
        if (kp_instance.number_of_item_types() == 0)
            break;

        // Solve one-dimensional knapsack problem.
        onedimensional::OptimizeParameters kp_parameters;
        kp_parameters.verbosity_level = 0;
        auto kp_output = optimize(kp_instance, kp_parameters);
        if (parameters_.timer.needs_to_end())
            break;

        // Retrieve width_max.
        Length width_max = 0;
        for (ItemTypeId kp_item_type_id = 0;
                kp_item_type_id < kp_instance.number_of_item_types();
                ++kp_item_type_id) {
            ItemTypeId item_type_id = kp2orig[kp_item_type_id].first;
            const ItemType& item_type = instance_.item_type(item_type_id);

            ItemPos kp_copies = kp_output.solution_pool.best().item_copies(kp_item_type_id);
            if (kp_copies == 0)
                continue;

            ItemPos copies = kp2orig[kp_item_type_id].second;
            Length width = item_type.rect.w * copies
                + cut_thickness * (copies - 1);
            if (width_max < width)
                width_max = width;
        }
        // Pad up to 'minimum_distance_1_cuts' *before* it is used below to
        // decide which items need a depth-3 narrowing cut: otherwise an item
        // exactly as wide as the original (unpadded) width_max would wrongly
        // be considered to span the (now wider) segment exactly.
        width_max = (std::max)(width_max, instance_.parameters().minimum_distance_1_cuts);

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

            ItemPos kp_copies = kp_output.solution_pool.best().item_copies(kp_item_type_id);
            if (kp_copies == 0)
                continue;

            ItemPos copies = kp2orig[kp_item_type_id].second;

            for (ItemPos kp_copy = 0; kp_copy < kp_copies; ++kp_copy) {
                cut_position += kp_instance.item_type(kp_item_type_id).length
                    - cut_thickness;
                extra_solution_builder.add_node(2, cut_position);

                Length width_cur = 0;
                //std::cout << "copies " << copies << std::endl;
                if (copies == 1) {
                    width_cur += item_type.rect.w;
                    if (width_cur < width_max) {
                        //std::cout << "add_node depth 3 cut_position " << bin_type.left_trim + width_cur << std::endl;
                        extra_solution_builder.add_node(3, bin_type.left_trim + width_cur);
                    }
                    extra_solution_builder.set_last_node_item(item_type_id);
                } else {
                    for (ItemPos copy = 0; copy < copies; ++copy) {
                        width_cur += item_type.rect.w;
                        //std::cout << "add_node depth 3 cut_position " << bin_type.left_trim + width_cur << std::endl;
                        extra_solution_builder.add_node(3, bin_type.left_trim + width_cur);
                        extra_solution_builder.set_last_node_item(item_type_id);
                        width_cur += cut_thickness;
                    }
                }
                cut_position += cut_thickness;
            }
        }
        Solution extra_solution = extra_solution_builder.build();

        // Retrieve column.
        Column column = solution_to_column(extra_solution);
        output.columns.push_back(std::shared_ptr<const Column>(new Column(column)));
        if (instance_.parameters().number_of_stages == 3
                && instance_.parameters().cut_type == CutType::Homogenous
                && instance_.all_item_types_oriented()) {
            // An improving column has rc > 0 for a Maximize model
            // (Knapsack) but rc < 0 for a Minimize model (OpenDimensionX);
            // accumulate 'reduced_cost_bound' via max for the former, min
            // for the latter, so it always tracks the most improving
            // reduced cost found so far.
            Value rc = columngenerationsolver::compute_reduced_cost(column, duals);
            if (instance_.objective() == Objective::Knapsack) {
                reduced_cost_bound = (std::max)(reduced_cost_bound, rc);
            } else if (instance_.objective() == Objective::OpenDimensionX) {
                reduced_cost_bound = (std::min)(reduced_cost_bound, rc);
            }
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
                profit = item_type.profit - duals[item_type_id] * multiplier_profit;
            }
            if (!strictly_greater(profit, 0.0))
                continue;

            ItemPos number_of_copies_in_full_strip = (width_max - 1 + cut_thickness)
                / (item_type.rect.w + cut_thickness);
            if (number_of_copies_in_full_strip == 0)
                continue;
            if (item_type.rect.h > height)
                continue;
            if (instance_.parameters().maximum_distance_2_cuts != -1
                    && item_type.rect.h > instance_.parameters().maximum_distance_2_cuts)
                continue;
            ItemPos number_of_full_strips = copies / number_of_copies_in_full_strip;
            ItemPos number_of_copies_in_last_strip = copies % number_of_copies_in_full_strip;
            Length width_cur = (number_of_full_strips > 0)?
                number_of_copies_in_full_strip * item_type.rect.w
                + (number_of_copies_in_full_strip - 1) * cut_thickness:
                number_of_copies_in_last_strip * item_type.rect.w
                + (number_of_copies_in_last_strip - 1) * cut_thickness;
            //std::cout << "wj " << item_type.rect.w
            //    << " copies " << copies
            //    << " number_of_copies_in_full_strip " << number_of_copies_in_full_strip
            //    << " number_of_full_strips " << number_of_full_strips
            //    << " number_of_copies_in_last_strip " << number_of_copies_in_last_strip
            //    << " wcur " << width_cur
            //    << std::endl;

            if (width_new < width_cur)
                width_new = width_cur;
        }
        if (width_new == 0)
            break;
        width = width_new;
    }
    //std::cout << "generate_2ho_patterns end" << std::endl;
}

void ColumnGenerationPricingSolver::generate_lower_stage_patterns(
        const std::vector<Value>& duals,
        PricingOutput& output,
        Value& reduced_cost_bound)
{
    const BinType& bin_type = instance_.bin_type(0);
    double multiplier_length = largest_power_of_two_lesser_or_equal(bin_type.rect.w);
    double multiplier_profit = largest_power_of_two_lesser_or_equal(instance_.largest_item_profit());
    Length cut_thickness = instance_.parameters().cut_thickness;
    Length available_width = first_stage_available_width(instance_, bin_type, filled_width_);
    Length height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;

    // Hoist the width-constraint dual: constant across all iterations.
    //
    // For Knapsack, this is the dual of the shared width-capacity row.
    // OpenDimensionX has no such row (width is minimized directly as the
    // objective, not shared out via a row/dual), but the pre-filter below
    // only needs "the cost of one unit of width", which for OpenDimensionX
    // is a fixed, unscaled 1 (see the objective_coefficient assignment
    // further down, which uses '(width + cut_thickness) / multiplier_length'
    // directly, with no dual scaling).
    Value width_dual = 0.0;
    if (instance_.objective() == Objective::Knapsack) {
        width_dual = duals[instance_.number_of_item_types()];
    } else if (instance_.objective() == Objective::OpenDimensionX) {
        width_dual = 1.0;
    }

    Length width = available_width;
    for (;;) {
        // Any pattern found below this point would be padded back up to
        // 'minimum_distance_1_cuts' anyway, so it's not worth searching.
        if (width < instance_.parameters().minimum_distance_1_cuts)
            break;
        // Phase 1 (cheap): identify eligible items and accumulate an upper bound
        // on the achievable sum of reduced profits for a strip of this width.
        // This avoids building the sub-instance and solving when the bound is too low.
        std::vector<ItemTypeId> sub2orig;
        Value max_profit_sum = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance_.item_type(item_type_id);

            ItemPos copies = item_type.copies - filled_demands_[item_type_id];
            if (copies <= 0)
                continue;

            Profit profit = 0;
            if (instance_.objective() == Objective::OpenDimensionX) {
                profit = duals[item_type_id];
            } else if (instance_.objective() == Objective::Knapsack) {
                profit = item_type.profit - duals[item_type_id] * multiplier_profit;
            }
            if (!strictly_greater(profit, 0.0))
                continue;

            bool fits = (item_type.rect.w <= width && item_type.rect.h <= height)
                || (!item_type.oriented && item_type.rect.h <= width && item_type.rect.w <= height);
            if (!fits)
                continue;

            // For Knapsack, 'profit' above was already scaled by
            // multiplier_profit (it's a reduced profit: item_type.profit -
            // duals * multiplier_profit), so dividing by multiplier_profit
            // here keeps it in the same units as the width term compared
            // against it below. For OpenDimensionX, 'profit' is the raw dual
            // (unscaled), so no such division applies.
            if (instance_.objective() == Objective::Knapsack) {
                max_profit_sum += (Value)copies * profit / multiplier_profit;
            } else if (instance_.objective() == Objective::OpenDimensionX) {
                max_profit_sum += (Value)copies * profit;
            }
            sub2orig.push_back(item_type_id);
        }

        if (sub2orig.empty())
            break;

        // Pre-filter: if the upper bound on the sum of reduced profits does not
        // exceed the width cost threshold, no packing in this strip can yield a
        // positive reduced cost.  Jump directly to the highest width where positive
        // reduced cost is still theoretically possible.
        if (strictly_greater(width_dual, 0.0)) {
            Value threshold = (Value)(width + cut_thickness) / multiplier_length * width_dual;
            if (!strictly_greater(max_profit_sum, threshold)) {
                double bound = max_profit_sum * multiplier_length / width_dual
                    - cut_thickness
                    - 1e-9;
                if (bound < 1)
                    break;
                width = (Length)bound;
                continue;
            }
        }

        // Phase 2: build the sub-instance and solve.
        // Sub-instance: strip of 'width × height' with 'number_of_stages - 1' stages.
        // The outer first stage is vertical, so the sub-problem's first stage is horizontal.
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::Knapsack);
        rectangleguillotine::Parameters sub_parameters = instance_.parameters();
        sub_parameters.number_of_stages = instance_.parameters().number_of_stages - 1;
        sub_parameters.first_stage_orientation = CutOrientation::Horizontal;
        // The sub-instance's own 1-cuts are the outer instance's 2-cuts (row
        // divisions), so they inherit the outer's 2-cuts distance constraint;
        // its own 2-cuts are the outer's 3-cuts, which have no distance
        // constraint at the outer level, so that constraint is not inherited.
        sub_parameters.minimum_distance_1_cuts = instance_.parameters().minimum_distance_2_cuts;
        sub_parameters.maximum_distance_1_cuts = instance_.parameters().maximum_distance_2_cuts;
        sub_parameters.minimum_distance_2_cuts = 0;
        sub_parameters.maximum_distance_2_cuts = -1;
        // Similarly, 'maximum_number_2_cuts' bounds the number of the outer
        // instance's 2-cuts (rows) per column; the sub-instance's own 2-cuts
        // are the outer's 3-cuts, which have no such count constraint at the
        // outer level, so it is not inherited either.
        sub_parameters.maximum_number_2_cuts = -1;
        sub_instance_builder.set_parameters(sub_parameters);
        sub_instance_builder.add_bin_type(width, height);

        for (ItemTypeId item_type_id: sub2orig) {
            const ItemType& item_type = instance_.item_type(item_type_id);
            ItemPos copies = item_type.copies - filled_demands_[item_type_id];
            Profit profit = 0;
            if (instance_.objective() == Objective::OpenDimensionX) {
                profit = duals[item_type_id];
            } else if (instance_.objective() == Objective::Knapsack) {
                profit = item_type.profit - duals[item_type_id] * multiplier_profit;
            }
            ItemTypeId sub_item_type_id = sub_instance_builder.add_item_type(instance_, item_type_id);
            sub_instance_builder.set_item_type_profit(sub_item_type_id, profit);
            sub_instance_builder.set_item_type_copies(sub_item_type_id, copies);
        }

        Instance sub_instance = sub_instance_builder.build();

        ColumnGenerationStripsParameters sub_params;
        sub_params.verbosity_level = 0;
        sub_params.optimization_mode = OptimizationMode::NotAnytimeSequential;
        sub_params.timer = parameters_.timer;
        auto sub_output = column_generation_strips(sub_instance, sub_params);
        if (parameters_.timer.needs_to_end())
            break;

        const Solution& sub_solution = sub_output.solution_pool.best();
        if (sub_solution.number_of_items() == 0)
            break;

        const SolutionBin& sub_bin = sub_solution.bin(0);

        // Compute the actual used width: max node.r over depth-2 non-waste nodes.
        // Depth-2 is the first vertical-cut level in the horizontal-first-stage
        // sub-solution, so node.r is the right edge of each vertical strip.
        // Waste nodes (item_type_id == -1) are excluded because auto-created waste
        // always spans to the parent's right edge (= sub-bin width), which would
        // give actual_used_width = width regardless of actual item placement.
        Length actual_used_width = 0;
        for (const SolutionNode& sub_node: sub_bin.nodes) {
            if (sub_node.d == 2 && sub_node.item_type_id != -1)
                actual_used_width = std::max(actual_used_width, sub_node.r);
        }
        if (actual_used_width == 0)
            break;
        // Pad up to 'minimum_distance_1_cuts' so that the LP-facing width
        // cost below matches the depth-1 node built for reconstruction;
        // otherwise the LP underestimates the strip's true width and the
        // merge callback can concatenate strips past the outer bin's edge.
        actual_used_width = first_stage_padded_width(instance_, actual_used_width);
        //std::cout << "sub_width " << sub_solution.width() << std::endl;
        //sub_solution.write("solution_rectangleguillotine_" + std::to_string(sub_solution.width())
        //        + "_" + std::to_string(width) + ".csv");

        // Build column for the outer problem.
        Column column;

        // Item type elements.
        for (ItemTypeId sub_item_type_id = 0;
                sub_item_type_id < sub_instance.number_of_item_types();
                ++sub_item_type_id) {
            ItemPos copies = sub_solution.item_copies(sub_item_type_id);
            if (copies == 0)
                continue;

            ItemTypeId item_type_id = sub2orig[sub_item_type_id];
            const ItemType& item_type = instance_.item_type(item_type_id);

            if (instance_.objective() == Objective::Knapsack) {
                column.objective_coefficient += copies * item_type.profit / multiplier_profit;
            }

            columngenerationsolver::LinearTerm element;
            element.row = item_type_id;
            element.coefficient = copies;
            column.elements.push_back(element);
        }

        // Width element: the strip claims 'actual_used_width' units of the outer bin.
        if (instance_.objective() == Objective::OpenDimensionX) {
            column.objective_coefficient =
                (double)(actual_used_width + cut_thickness) / multiplier_length;
        } else {
            columngenerationsolver::LinearTerm element;
            element.row = instance_.number_of_item_types();
            element.coefficient =
                (double)(actual_used_width + cut_thickness) / multiplier_length;
            column.elements.push_back(element);
        }

        // Build extra solution for callback reconstruction.
        //
        // The outer first stage is vertical (depth-1 nodes are vertical cuts).
        // The sub-solution has horizontal first stage, so:
        //   - Sub odd depths (horizontal): add with depth+1 (outer even) using bottom_trim + t
        //   - Sub even depths (vertical): add with depth+1 (outer odd) using left_trim + r
        // The outer depth-1 node uses actual_used_width, not the full 'width'.
        SolutionBuilder extra_solution_builder(instance_);
        extra_solution_builder.add_bin(0, 1, CutOrientation::Vertical);
        extra_solution_builder.add_node(1, bin_type.left_trim + actual_used_width);
        for (const SolutionNode& sub_node: sub_bin.nodes) {
            if (sub_node.d <= 0)
                continue;
            if (sub_node.d % 2 == 1) {
                // Horizontal cut in sub (odd depth) → outer even depth; t coordinate.
                extra_solution_builder.add_node(
                        sub_node.d + 1,
                        bin_type.bottom_trim + sub_node.t);
            } else {
                // Vertical cut in sub (even depth) → outer odd depth; r coordinate.
                // Cap at actual_used_width: waste nodes that auto-fill to the sub-bin
                // width must be resized to fit the narrower outer strip.  Skip entirely
                // when the node's l already meets or exceeds actual_used_width (capping
                // would yield zero or negative width).
                if (sub_node.l >= actual_used_width)
                    continue;
                extra_solution_builder.add_node(
                        sub_node.d + 1,
                        bin_type.left_trim + std::min(sub_node.r, actual_used_width));
            }
            if (sub_node.item_type_id >= 0) {
                extra_solution_builder.set_last_node_item(sub2orig[sub_node.item_type_id]);
            }
        }
        Solution extra_solution = extra_solution_builder.build();
        column.extra = std::shared_ptr<void>(new Solution(extra_solution));
        output.columns.push_back(std::shared_ptr<const Column>(new Column(column)));
        // As above: rc > 0 is improving for Knapsack (Maximize), rc < 0 is
        // improving for OpenDimensionX (Minimize). Unlike the other sites,
        // this bound is derived from 'sub_output.knapsack_bound' rather than
        // 'compute_reduced_cost' directly: for Knapsack, the item profits
        // fed into the sub-instance were pre-scaled by multiplier_profit
        // (see 'profit = item_type.profit - duals[...] * multiplier_profit'
        // above), so the bound needs the matching '/ multiplier_profit' to
        // stay in the same units as the width term; for OpenDimensionX, the
        // sub-instance's item profits are the raw duals, unscaled, so
        // 'sub_output.knapsack_bound' is already in the same (unscaled)
        // units as the width term below.
        if (instance_.objective() == Objective::Knapsack) {
            Value rc_bound = sub_output.knapsack_bound / multiplier_profit
                - width_dual * actual_used_width / multiplier_length;
            reduced_cost_bound = (std::max)(reduced_cost_bound, rc_bound);
        } else if (instance_.objective() == Objective::OpenDimensionX) {
            Value rc_bound = (double)(actual_used_width + cut_thickness) / multiplier_length
                - sub_output.knapsack_bound;
            reduced_cost_bound = (std::min)(reduced_cost_bound, rc_bound);
        }

        // Compute the next width to try.
        //
        // Any strip of width w' ≤ actual_used_width can pack at most the same
        // sum of reduced profits as the current column.  For a column at w' to
        // have positive reduced cost we therefore need:
        //
        //   sum_reduced_profit > (w' + cut_t) / multiplier_length × width_dual
        //   ⟺  w' < actual_used_width + rc × multiplier_length / width_dual
        //
        // When rc ≤ 0 this gives a tighter upper bound than actual_used_width - 1
        // and lets us skip widths that provably cannot yield a positive reduced cost.
        //
        // Restricted to Knapsack: 'compute_reduced_cost' returns a
        // maximization-style value (positive = improving), and this
        // refinement's polarity ('rc > 0' is "still promising") and bound
        // formula are both built around that convention. Reusing it as-is
        // for OpenDimensionX (rc < 0 is improving there) would apply the
        // wrong direction, so it's left at the safe 'actual_used_width - 1'
        // fallback for that objective instead of re-deriving it here too.
        Length width_next = actual_used_width - 1;
        if (instance_.objective() == Objective::Knapsack
                && strictly_greater(width_dual, 0.0)) {
            Value rc = columngenerationsolver::compute_reduced_cost(column, duals);
            if (!strictly_greater(rc, 0.0)) {
                double bound = actual_used_width
                    + rc * multiplier_length / width_dual
                    - 1e-9;
                if (bound < 1)
                    break;
                width_next = std::min(width_next, (Length)bound);
            }
        }
        width = width_next;
    }
    //std::cout << "generate_lower_stage_patterns end" << std::endl;
}

void ColumnGenerationPricingSolver::generate_duals_zero(
        PricingOutput& output)
{
    std::vector<Value> duals(
            instance_.objective() == Objective::Knapsack?
            instance_.number_of_item_types() + 1:
            instance_.number_of_item_types(),
            0.0);
    Value reduced_cost_bound = 0.0;

    if (instance_.parameters().number_of_stages == 2
            && instance_.parameters().cut_type == CutType::Exact) {
        generate_1e_patterns(duals, output, reduced_cost_bound);
    } else if (instance_.parameters().number_of_stages == 2
            && instance_.parameters().cut_type == CutType::NonExact) {
        generate_1n_patterns(duals, output, reduced_cost_bound);
    } else if (instance_.parameters().number_of_stages == 2
            && instance_.parameters().cut_type == CutType::Roadef2018
            && instance_.all_item_types_oriented()) {
        generate_1ro_patterns(duals, output, reduced_cost_bound);
    } else if (instance_.parameters().cut_type == CutType::Homogenous) {
        generate_2ho_patterns(duals, output, reduced_cost_bound);
    } else {
        generate_lower_stage_patterns(duals, output, reduced_cost_bound);
    }
}

PricingOutput ColumnGenerationPricingSolver::solve_pricing(
            const std::vector<Value>& duals)
{
    //std::cout << "solve_pricing" << std::endl;

    const BinType& bin_type = instance_.bin_type(0);

    PricingOutput output;
    Value reduced_cost_bound = 0.0;

    // If all columns have already been generated, stop.

    if (instance_.parameters().number_of_stages == 1
            && instance_.parameters().cut_type == CutType::Roadef2018) {
        return output;
    }

    if (instance_.parameters().number_of_stages == 2
            && instance_.parameters().cut_type == CutType::Homogenous) {
        return output;
    }

    if (all_columns_2e_patterns_generated_
            && instance_.parameters().number_of_stages == 2
            && instance_.parameters().cut_type == CutType::Exact) {
        return output;
    }

    if (all_columns_2n_patterns_generated_
            && instance_.parameters().number_of_stages == 2
            && instance_.parameters().cut_type == CutType::NonExact) {
        return output;
    }

    if (all_columns_2r_patterns_generated_
            && instance_.parameters().number_of_stages == 2
            && instance_.parameters().cut_type == CutType::Roadef2018) {
        return output;
    }

    if (all_columns_3h_patterns_generated_
            && instance_.parameters().number_of_stages == 3
            && instance_.parameters().cut_type == CutType::Homogenous) {
        return output;
    }

    if (first_pricing_) {
        generate_duals_zero(output);
        first_pricing_ = false;
    }

    // Otherwise, solve the pricing sub-problems.

    // Base cases.

    // Generate one-staged exact patterns.
    if (!all_columns_2e_patterns_generated_) {
        if ((instance_.parameters().number_of_stages == 2
                    && instance_.parameters().cut_type == CutType::Exact)
                || (instance_.parameters().number_of_stages == 2
                    && instance_.parameters().cut_type == CutType::NonExact)
                || (instance_.parameters().number_of_stages == 2
                    && instance_.parameters().cut_type == CutType::Roadef2018)
                || instance_.parameters().number_of_stages >= 3) {
            generate_1e_patterns(duals, output, reduced_cost_bound);
        }
    }

    // Generate one-staged non-exact patterns.
    if (!all_columns_2n_patterns_generated_) {
        if ((instance_.parameters().number_of_stages == 2
                    && instance_.parameters().cut_type == CutType::NonExact)
                || (instance_.parameters().number_of_stages == 2
                    && instance_.parameters().cut_type == CutType::Roadef2018)
                || instance_.parameters().number_of_stages >= 3) {
            generate_1n_patterns(duals, output, reduced_cost_bound);
        }
    }

    // Generate one-staged roadef2018 oriented patterns.
    if (!all_columns_2r_patterns_generated_) {
        if (instance_.parameters().number_of_stages == 2
                && instance_.parameters().cut_type == CutType::Roadef2018
                && instance_.all_item_types_oriented()) {
            generate_1ro_patterns(duals, output, reduced_cost_bound);
        }
    }

    // Generate two-staged homogenous patterns without rotations.
    if (!all_columns_3h_patterns_generated_) {
        if (instance_.parameters().number_of_stages >= 3) {
            generate_2ho_patterns(duals, output, reduced_cost_bound);
        }
    }

    // An improving column has rc > 0 for a Maximize model (Knapsack) but
    // rc < 0 for a Minimize model (OpenDimensionX), matching how
    // 'columngenerationsolver::column_generation' itself decides whether to
    // accept a generated column.
    bool has_good_column = false;
    for (const auto& column: output.columns) {
        Value rc = columngenerationsolver::compute_reduced_cost(*column, duals);
        if (instance_.objective() == Objective::Knapsack) {
            if (strictly_greater(rc, 0)) {
                has_good_column = true;
                break;
            }
        } else if (instance_.objective() == Objective::OpenDimensionX) {
            if (strictly_lesser(rc, 0)) {
                has_good_column = true;
                break;
            }
        }
    }

    // Recursion.

    // Generate other patterns.
    if (!has_good_column) {
        if ((instance_.parameters().number_of_stages == 2
                    && instance_.parameters().cut_type == CutType::Roadef2018
                    && !instance_.all_item_types_oriented())
                || (instance_.parameters().number_of_stages == 3
                    && instance_.parameters().cut_type == CutType::Homogenous
                    && !instance_.all_item_types_oriented())
                || (instance_.parameters().number_of_stages == 3
                    && instance_.parameters().cut_type == CutType::Exact)
                || (instance_.parameters().number_of_stages == 3
                    && instance_.parameters().cut_type == CutType::NonExact)
                || (instance_.parameters().number_of_stages == 3
                    && instance_.parameters().cut_type == CutType::Roadef2018)
                || (instance_.parameters().number_of_stages >= 4)) {
            generate_lower_stage_patterns(duals, output, reduced_cost_bound);
        }
    } else {
        reduced_cost_bound = std::numeric_limits<Value>::infinity();
    }

    if (instance_.objective() == Objective::Knapsack) {
        for (const std::shared_ptr<const Column>& column: output.columns) {
            const Solution& solution = *std::static_pointer_cast<Solution>(column->extra);
            check_feasibility(solution, FUNC_SIGNATURE);
            if (local_output_ != nullptr) {
                local_output_->solution_pool.add(solution, "V strip");
            } else {
                algorithm_formatter_.update_solution(solution, "V strip");
            }
        }
    }

    output.overcost = instance_.number_of_items() * reduced_cost_bound;
    //for (const auto& column: output.columns)
    //    std::cout << *column << std::endl;
    return output;
}

void column_generation_strips_vertical(
        const Instance& instance,
        const ColumnGenerationStripsParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        packingsolver::Output<Instance, Solution>* local_output)
{
    GetModelOutput cgs_model = get_model(instance, parameters, algorithm_formatter, local_output);
    columngenerationsolver::LimitedDiscrepancySearchParameters cgslds_parameters;
    for (const auto& column: cgs_model.column_pool)
        cgslds_parameters.column_pool.push_back(column);
    cgslds_parameters.verbosity_level = 0;
    cgslds_parameters.timer = parameters.timer;
    cgslds_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    cgslds_parameters.internal_diving = 0;
    cgslds_parameters.dummy_column_objective_coefficient = 2;
    cgslds_parameters.automatic_stop = (parameters.optimization_mode != OptimizationMode::Anytime);
    cgslds_parameters.new_solution_callback = [&instance, &algorithm_formatter, local_output](
            const columngenerationsolver::Output& cgs_output)
    {
        const BinType& bin_type = instance.bin_type(0);
        const columngenerationsolver::LimitedDiscrepancySearchOutput& cgslds_output
            = static_cast<const columngenerationsolver::LimitedDiscrepancySearchOutput&>(cgs_output);
        if (cgslds_output.solution.feasible()) {
            //std::cout << "callback..." << std::endl;
            SolutionBuilder solution_builder(instance);
            solution_builder.add_bin(0, 1, CutOrientation::Vertical);
            Length offset = 0;
            for (const auto& pair: cgslds_output.solution.columns()) {
                //std::cout << "offset " << offset << std::endl;
                const Column& column = *(pair.first);
                BinPos value = std::round(pair.second);
                for (BinPos v = 0; v < value; ++v) {
                    const SolutionBin& bin = std::static_pointer_cast<Solution>(column.extra)->bin(0);
                    bool first = true;
                    Length w_max = 0;
                    for (const SolutionNode& node: bin.nodes) {
                        //std::cout << "node " << node << std::endl;
                        if (node.d <= 0)
                            continue;
                        if (node.d == 1) {
                            if (!first)
                                break;
                            first = false;
                            w_max = std::max(w_max, node.r - node.l);
                        }
                        if (node.d % 2 == 1) {
                            //std::cout << "add_node depth " << node.d << " cut_position " << offset + node.r << std::endl;
                            solution_builder.add_node(node.d, offset + node.r);
                        } else {
                            //std::cout << "add_node depth " << node.d << " cut_position " << node.t << std::endl;
                            solution_builder.add_node(node.d, node.t);
                        }
                        if (node.item_type_id >= 0)
                            solution_builder.set_last_node_item(node.item_type_id);
                    }
                    offset += w_max + instance.parameters().cut_thickness;
                }
            }
            Solution solution = solution_builder.build();
            check_feasibility(solution, FUNC_SIGNATURE);
            std::stringstream ss;
            ss << "V n " << cgslds_output.number_of_nodes;
            if (local_output != nullptr) {
                local_output->solution_pool.add(solution, ss.str());
            } else {
                algorithm_formatter.update_solution(solution, ss.str());
            }
            //std::cout << "callback end" << std::endl;
        }
    };
    cgslds_parameters.new_bound_callback = [&instance, &algorithm_formatter](
            const columngenerationsolver::Output& cgs_output)
    {
        const columngenerationsolver::LimitedDiscrepancySearchOutput& cgslds_output
            = static_cast<const columngenerationsolver::LimitedDiscrepancySearchOutput&>(cgs_output);
        // 'cgslds_output.bound' is scaled differently depending on the
        // objective: by multiplier_profit for Knapsack (a profit upper
        // bound, matching the objective_coefficient scaling in
        // 'solution_to_column'), by multiplier_length for OpenDimensionX (a
        // width lower bound, matching its own objective_coefficient
        // scaling). Note that as seen by this function, a flipped
        // OpenDimensionY instance (see 'column_generation_strips_horizontal'
        // below) has objective OpenDimensionX too.
        if (instance.objective() == Objective::Knapsack) {
            double multiplier_profit = largest_power_of_two_lesser_or_equal(instance.largest_item_profit());
            algorithm_formatter.update_knapsack_bound(cgslds_output.bound * multiplier_profit);
        } else if (instance.objective() == Objective::OpenDimensionX) {
            const BinType& bin_type = instance.bin_type(0);
            double multiplier_length = largest_power_of_two_lesser_or_equal(bin_type.rect.w);
            algorithm_formatter.update_open_dimension_x_bound(
                    (Length)(cgslds_output.bound * multiplier_length));
        }
    };
    cgslds_parameters.column_generation_parameters.solver_name
        = parameters.linear_programming_solver_name;
    columngenerationsolver::limited_discrepancy_search(cgs_model.model, cgslds_parameters);
}

void column_generation_strips_horizontal(
        const Instance& instance,
        const ColumnGenerationStripsParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        packingsolver::Output<Instance, Solution>* local_output)
{
    // Build flipped instance.
    InstanceFlipper instance_flippper(instance);
    const Instance& flipped_instance = instance_flippper.flipped_instance();

    ColumnGenerationStripsParameters flipped_parameters = parameters;
    flipped_parameters.new_solution_callback = [
        &instance, &algorithm_formatter, local_output, &instance_flippper](
                const packingsolver::Output<Instance, Solution>& ps_output)
        {
            const ColumnGenerationStripsOutput& flipped_output
                = static_cast<const ColumnGenerationStripsOutput&>(ps_output);
            std::string label = flipped_output.solution_pool.best_label();
            if (!label.empty() && label[0] == 'V')
                label[0] = 'H';
            Solution solution = instance_flippper.unflip_solution(
                    flipped_output.solution_pool.best());
            check_feasibility(solution, FUNC_SIGNATURE);
            if (local_output != nullptr) {
                local_output->solution_pool.add(solution, label);
            } else {
                algorithm_formatter.update_solution(solution, label);
            }
            // 'InstanceFlipper' turns OpenDimensionY into OpenDimensionX
            // (and leaves every other objective untouched), so
            // 'flipped_output''s bound must be routed back according to the
            // true (unflipped) 'instance' objective, not the flipped one.
            if (instance.objective() == Objective::Knapsack) {
                algorithm_formatter.update_knapsack_bound(
                        flipped_output.knapsack_bound);
            } else if (instance.objective() == Objective::OpenDimensionX) {
                algorithm_formatter.update_open_dimension_x_bound(
                        flipped_output.open_dimension_x_bound);
            } else if (instance.objective() == Objective::OpenDimensionY) {
                algorithm_formatter.update_open_dimension_y_bound(
                        flipped_output.open_dimension_x_bound);
            }
        };
    column_generation_strips(
            flipped_instance,
            flipped_parameters);
}

}

const ColumnGenerationStripsOutput packingsolver::rectangleguillotine::column_generation_strips(
        const Instance& instance,
        const ColumnGenerationStripsParameters& parameters)
{
    ColumnGenerationStripsOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Reduction.
    if (instance.parameters().first_stage_orientation == CutOrientation::Vertical) {
        column_generation_strips_vertical(
                instance,
                parameters,
                algorithm_formatter,
                nullptr);
    } else if (instance.parameters().first_stage_orientation == CutOrientation::Horizontal) {
        column_generation_strips_horizontal(
                instance,
                parameters,
                algorithm_formatter,
                nullptr);
    } else {
        // 'column_generation_strips_vertical' and
        // 'column_generation_strips_horizontal' run in parallel; in
        // 'NotAnytimeDeterministic' mode, each writes its solutions to its
        // own local output instead of the shared 'algorithm_formatter', so
        // that they can be replayed into it in a fixed, deterministic order
        // (vertical then horizontal) once both have terminated, instead of
        // the (non-deterministic) order in which they actually finish.
        bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
        packingsolver::Output<Instance, Solution> local_output_vertical(instance);
        packingsolver::Output<Instance, Solution> local_output_horizontal(instance);

        std::vector<std::function<void()>> tasks;
        std::forward_list<std::exception_ptr> exception_ptr_list;
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr_1 = exception_ptr_list.front();
        tasks.push_back([&exception_ptr_1, &instance, &parameters, &algorithm_formatter, &local_output_vertical, deterministic]() {
            wrapper<decltype(&column_generation_strips_vertical), column_generation_strips_vertical>(
                    exception_ptr_1,
                    instance,
                    parameters,
                    algorithm_formatter,
                    deterministic ? &local_output_vertical : nullptr);
        });
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr_2 = exception_ptr_list.front();
        tasks.push_back([&exception_ptr_2, &instance, &parameters, &algorithm_formatter, &local_output_horizontal, deterministic]() {
            wrapper<decltype(&column_generation_strips_horizontal), column_generation_strips_horizontal>(
                    exception_ptr_2,
                    instance,
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

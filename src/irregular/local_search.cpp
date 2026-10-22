#include "irregular/local_search.hpp"
#include "irregular/sequential_feasibility.hpp"
#include "irregular/linear_programming/separator.hpp"
#include "irregular/solution_builder.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "shape/boolean_operations.hpp"

#include "localsearchsolver/population.hpp"
//#include "shape/writer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * Population-based iterated local search, for objective Feasibility, on top of
 * a separator (direct trigonometry or linear programming engine) already
 * loaded with the initial layout.
 *
 * The initial layout is separated; then, while the separations fail, their
 * layouts are added to a population, and the next separation starts from a
 * disruption of a layout drawn from the population. It is given up early if
 * its loss doesn't get below the loss of this layout.
 */
template <typename SeparatorType>
void run_iterated_local_search(
        SeparatorType& separator,
        std::mt19937_64& generator,
        const LocalSearchParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    // Layouts of the failed separations.
    struct FailedLayout
    {
        typename SeparatorType::Layout layout;

        double loss;
    };
    typename localsearchsolver::Population<FailedLayout, double>::Parameters population_parameters;
    population_parameters.minimum_size = parameters.population_minimum_size;
    population_parameters.maximum_size = parameters.population_maximum_size;
    population_parameters.number_of_elite_solutions = parameters.population_number_of_elite_solutions;
    population_parameters.number_of_closest_neighbors = parameters.population_number_of_closest_neighbors;
    localsearchsolver::Population<FailedLayout, double> population(
            [](const FailedLayout& failed_layout)
            {
                return failed_layout.loss;
            },
            [&separator](
                const FailedLayout& failed_layout_1,
                const FailedLayout& failed_layout_2)
            {
                return separator.distance(
                        failed_layout_1.layout,
                        failed_layout_2.layout);
            },
            population_parameters);

    // If the last separation, of iteration 'iteration', succeeded, return true
    // and report the solution. Otherwise, add its layout to the population.
    auto process_separation = [&separator, &algorithm_formatter, &population, &generator](
            bool separated,
            Counter iteration)
    {
        if (separated) {
            std::stringstream ss;
            ss << "it " << iteration;
            algorithm_formatter.update_solution(separator.solution(), ss.str());
            return true;
        }
        FailedLayout failed_layout = {separator.layout(), separator.total_loss()};
        population.add(failed_layout, generator);
        return false;
    };

    // Separation of the initial layout, with more effort.
    if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
        return;
    bool separated = separator.separate_initial(parameters.timer);
    if (process_separation(separated, 0))
        return;

    for (Counter number_of_failed_separations = 1;
            parameters.maximum_number_of_failed_separations == -1
            || number_of_failed_separations < parameters.maximum_number_of_failed_separations;
            ++number_of_failed_separations) {
        if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
            return;

        // Draw a failed layout from the population (binary tournament) and
        // disrupt it.
        FailedLayout selected_layout = population.binary_tournament_single(generator);
        separator.set_layout(selected_layout.layout);
        separator.disrupt();

        // Separate it; the separation is given up early if its loss doesn't
        // get below the loss of the drawn layout.
        separated = separator.separate(parameters.timer, selected_layout.loss);
        if (process_separation(separated, number_of_failed_separations))
            return;
    }
}

/** Local search with the direct trigonometry engine, for objective Feasibility. */
void local_search_direct_trigonometry(
        const Instance& instance,
        std::mt19937_64& generator,
        const Solution* initial_solution,
        const LocalSearchParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    Separator separator(
            instance,
            parameters.separator_parameters,
            generator,
            parameters.separator_item_types);
    separator.set_initial_stopping_criteria(
            parameters.initial_separation_maximum_number_of_iterations_without_improvement,
            parameters.initial_separation_maximum_number_of_strikes);
    if (initial_solution != nullptr) {
        separator.set_solution(
                convert_initial_solution(instance, *initial_solution),
                parameters.timer);
    } else {
        separator.set_solution(Solution(instance), parameters.timer);
    }
    run_iterated_local_search(separator, generator, parameters, algorithm_formatter);
}

/** Local search with the linear programming engine, for objective Feasibility. */
void local_search_linear_programming(
        const Instance& instance,
        std::mt19937_64& generator,
        const Solution* initial_solution,
        const LocalSearchParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    LinearProgrammingSeparator separator(
            instance,
            parameters.linear_programming_separator_parameters,
            generator);
    if (initial_solution != nullptr) {
        separator.set_solution(
                convert_initial_solution(instance, *initial_solution),
                parameters.timer);
    } else {
        separator.set_solution(Solution(instance), parameters.timer);
    }
    run_iterated_local_search(separator, generator, parameters, algorithm_formatter);
}

}  // namespace

Solution packingsolver::irregular::convert_initial_solution(
        const Instance& instance,
        const Solution& solution)
{
    std::vector<BinPos> bin_types_remaining_copies(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        bin_types_remaining_copies[bin_type_id]
            = instance.bin_type(bin_type_id).copies;
    }

    // Bin type of each copy of each bin of 'solution' (-1 if dropped).
    std::vector<std::vector<BinTypeId>> bins_bin_type_ids(solution.number_of_different_bins());
    // First, the bins of the same bin type.
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& bin = solution.bin(bin_pos);
        bins_bin_type_ids[bin_pos].resize(bin.copies, -1);
        if (bin.bin_type_id >= instance.number_of_bin_types())
            continue;
        for (BinPos copy = 0; copy < bin.copies; ++copy) {
            if (bin_types_remaining_copies[bin.bin_type_id] == 0)
                break;
            bins_bin_type_ids[bin_pos][copy] = bin.bin_type_id;
            bin_types_remaining_copies[bin.bin_type_id]--;
        }
    }
    // Then, the remaining bins.
    BinTypeId bin_type_id_cur = 0;
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        for (BinTypeId& bin_type_id: bins_bin_type_ids[bin_pos]) {
            if (bin_type_id != -1)
                continue;
            while (bin_type_id_cur < instance.number_of_bin_types()
                    && bin_types_remaining_copies[bin_type_id_cur] == 0) {
                bin_type_id_cur++;
            }
            if (bin_type_id_cur == instance.number_of_bin_types())
                break;
            bin_type_id = bin_type_id_cur;
            bin_types_remaining_copies[bin_type_id_cur]--;
        }
    }

    SolutionBuilder solution_builder(instance);
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& bin = solution.bin(bin_pos);
        for (BinTypeId bin_type_id: bins_bin_type_ids[bin_pos]) {
            if (bin_type_id == -1)
                continue;
            BinPos bin_pos_new = solution_builder.add_bin(bin_type_id, 1);
            for (const SolutionItem& item: bin.items) {
                solution_builder.add_item(
                        bin_pos_new,
                        item.item_type_id,
                        item.bl_corner,
                        item.angle,
                        item.mirror,
                        item.is_fixed);
            }
        }
    }
    return solution_builder.build();
}

LocalSearchOutput packingsolver::irregular::local_search(
        const Instance& instance,
        std::mt19937_64& generator,
        const Solution* initial_solution,
        const LocalSearchParameters& parameters)
{
    if (instance.objective() != Objective::Feasibility) {
        LocalSearchOutput output(instance);
        AlgorithmFormatter algorithm_formatter(instance, parameters, output);
        algorithm_formatter.start();
        algorithm_formatter.print_header();

        // The item types of the separator are the same for all the
        // sub-problems.
        std::shared_ptr<const Separator::ItemTypes> separator_item_types
            = parameters.separator_item_types;
        if (parameters.engine == LocalSearchEngine::DirectTrigonometry
                && separator_item_types == nullptr) {
            separator_item_types = Separator::build_item_types(
                    instance,
                    parameters.separator_parameters);
        }

        SequentialFeasibilitySolver solver = [&parameters, &algorithm_formatter, &generator, initial_solution, separator_item_types](
                const Instance& sub_instance,
                const Solution* previous_solution)
        {
            LocalSearchParameters inner_parameters;
            inner_parameters.verbosity_level = 0;
            inner_parameters.timer = parameters.timer;
            inner_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            inner_parameters.engine = parameters.engine;
            inner_parameters.shrink_ratio = parameters.shrink_ratio;
            inner_parameters.separator_parameters = parameters.separator_parameters;
            inner_parameters.linear_programming_separator_parameters
                = parameters.linear_programming_separator_parameters;
            inner_parameters.separator_item_types = separator_item_types;
            inner_parameters.initial_separation_maximum_number_of_iterations_without_improvement
                = parameters.initial_separation_maximum_number_of_iterations_without_improvement;
            inner_parameters.initial_separation_maximum_number_of_strikes
                = parameters.initial_separation_maximum_number_of_strikes;
            inner_parameters.maximum_number_of_failed_separations
                = parameters.maximum_number_of_failed_separations;
            inner_parameters.population_minimum_size = parameters.population_minimum_size;
            inner_parameters.population_maximum_size = parameters.population_maximum_size;
            inner_parameters.population_number_of_elite_solutions
                = parameters.population_number_of_elite_solutions;
            inner_parameters.population_number_of_closest_neighbors
                = parameters.population_number_of_closest_neighbors;
            // The first sub-problem has no previous solution: warm start it
            // from the initial solution.
            return local_search(
                    sub_instance,
                    generator,
                    (previous_solution != nullptr)? previous_solution: initial_solution,
                    inner_parameters).solution_pool;
        };

        SequentialFeasibilityParameters sf_parameters;
        sf_parameters.verbosity_level = 0;
        sf_parameters.shrink_ratio = parameters.shrink_ratio;
        sf_parameters.timer = parameters.timer;
        sf_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        sf_parameters.new_solution_callback = [&algorithm_formatter](
                const packingsolver::Output<Instance, Solution>& sf_output)
        {
            algorithm_formatter.update_solution(
                    sf_output.solution_pool.best(),
                    sf_output.solution_pool.best_label());
        };

        sequential_feasibility(instance, solver, sf_parameters);

        algorithm_formatter.end();
        return output;
    }

    LocalSearchOutput output(instance);

    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (parameters.engine == LocalSearchEngine::LinearProgramming) {
        local_search_linear_programming(instance, generator, initial_solution, parameters, algorithm_formatter);
    } else {
        local_search_direct_trigonometry(instance, generator, initial_solution, parameters, algorithm_formatter);
    }

    algorithm_formatter.end();
    return output;
}

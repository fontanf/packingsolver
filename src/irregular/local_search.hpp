/**
 * Local search
 *
 * The goal of the local search algorithm is to improve a given solution by
 * iteratively applying local moves.
 */

#pragma once

#include "irregular/direct_trigonometry/separator.hpp"
#include "irregular/linear_programming/separator.hpp"

#include "packingsolver/irregular/optimize.hpp"

#include <random>

namespace packingsolver
{
namespace irregular
{

/** Engine of the local search for objective Feasibility. */
enum class LocalSearchEngine
{
    /**
     * Direct trigonometry: the items may overlap, and the separator moves them
     * to minimize the overlap, computed directly from their geometry (see
     * 'separator.hpp').
     */
    DirectTrigonometry,

    /**
     * Shrinkage minimization: the items are packed one by one; the items of a
     * bin are shrunk to remove the overlap, and a linear program minimizes the
     * shrinkage.
     */
    LinearProgramming,
};

struct LocalSearchOutput: Output
{
    /** Constructor. */
    LocalSearchOutput(const Instance& instance):
        Output(instance) { }
};

struct LocalSearchParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** Engine. */
    LocalSearchEngine engine = LocalSearchEngine::DirectTrigonometry;

    /*
     * Parameters of the sequential feasibility algorithm, for the other
     * objectives than Feasibility.
     */

    /** Shrink ratio (see 'SequentialFeasibilityParameters'). */
    double shrink_ratio = 0.001;

    /*
     * Parameters of the direct trigonometry engine.
     */

    /** Parameters of the separator. */
    SeparatorParameters separator_parameters;

    /**
     * Item types of the separator, if already built (see
     * 'Separator::build_item_types'); built from the instance otherwise.
     *
     * For the other objectives than Feasibility, they are built once from the
     * instance and shared by the sub-problems of the sequential feasibility
     * algorithm.
     */
    std::shared_ptr<const Separator::ItemTypes> separator_item_types;

    /**
     * Maximum number of iterations without improvement of the separation of
     * the initial layout.
     *
     * -1 means the one of 'separator_parameters'.
     */
    Counter initial_separation_maximum_number_of_iterations_without_improvement = -1;

    /**
     * Maximum number of strikes of the separation of the initial layout.
     *
     * The initial layout, built from the initial solution, is separated with
     * more effort than the disruptions of the failed layouts.
     *
     * -1 means the one of 'separator_parameters'.
     */
    Counter initial_separation_maximum_number_of_strikes = 10;

    /**
     * Maximum number of failed separations, after which the instance is
     * considered infeasible.
     *
     * After a failed separation, the next one starts from a disruption of a
     * failed layout, selected by binary tournament in a population. It is
     * given up early if its loss doesn't get below the loss of this failed
     * layout (see
     * 'SeparatorParameters::maximum_number_of_strikes_above_reference').
     *
     * -1 means no limit: the search only stops with the timer.
     */
    Counter maximum_number_of_failed_separations = 10;

    /** Minimum size of the population. */
    Counter population_minimum_size = 4;

    /**
     * Maximum size of the population, above which layouts are removed until
     * it reaches its minimum size.
     */
    Counter population_maximum_size = 8;

    /**
     * Number of elite layouts: the number of layouts kept for their loss
     * regardless of their contribution to the diversity.
     */
    Counter population_number_of_elite_solutions = 2;

    /**
     * Number of closest layouts used to compute the contribution of a layout
     * to the diversity.
     */
    Counter population_number_of_closest_neighbors = 2;

    /*
     * Parameters of the linear programming engine.
     */

    /** Parameters of the separator. */
    LinearProgrammingSeparatorParameters linear_programming_separator_parameters;
};

/**
 * Run the local search algorithm.
 *
 * For objective Feasibility, the local search is run directly. For the other
 * objectives, it is run as the inner solver of the sequential feasibility
 * algorithm.
 *
 * If 'initial_solution' is not 'nullptr', it is the warm start of the local
 * search. It is a solution of the original instance: 'instance' itself, or,
 * when 'instance' is a sub-instance built by the sequential feasibility
 * algorithm, the instance it has been built from. That is, an instance with
 * the same item types and with the same bin type ids, but whose bins may be
 * restricted and have fewer copies. Therefore, it might not be a feasible
 * solution of 'instance': it may use more bins than available, and some items
 * may stick out of their bin. The local search starts by repairing it.
 *
 * For the other objectives than Feasibility, it is the warm start of the first
 * sub-problem of the sequential feasibility algorithm; the next sub-problems
 * are warm-started from the previous solution found. The linear programming
 * engine doesn't use it.
 */
LocalSearchOutput local_search(
        const Instance& instance,
        std::mt19937_64& generator,
        const Solution* initial_solution = nullptr,
        const LocalSearchParameters& parameters = {});

/**
 * Convert a solution of the original instance (see 'local_search') to a
 * solution of 'instance'.
 *
 * This is the first step of the repair of the initial solution. Each bin of
 * 'solution' is mapped, in order, to a bin of 'instance' of the same bin type,
 * as long as copies of it remain. The bins that can't be mapped this way are
 * then mapped, in order, to the remaining bins of 'instance' (for objective
 * BinPackingWithLeftovers, the last bin of a sub-instance of the sequential
 * feasibility algorithm, restricted, may be of a bin type of its own). The
 * bins that still can't be mapped are dropped together with their items.
 * Items keep their positions, even if they don't fit in their bin anymore.
 */
Solution convert_initial_solution(
        const Instance& instance,
        const Solution& solution);

}
}

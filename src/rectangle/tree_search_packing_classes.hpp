/**
 * Tree search on packing classes
 *
 * Exact algorithm for the 'Feasibility' objective with a single bin, based
 * on the characterization of the packings by packing classes:
 * - "A Combinatorial Characterization of Higher-Dimensional Orthogonal
 *   Packing" (Fekete and Schepers, 2004)
 * - "An Exact Algorithm for Higher-Dimensional Orthogonal Packing" (Fekete,
 *   Schepers and van der Veen, 2007)
 *
 * For each axis, the packing class of a packing is the graph of the pairs of
 * items whose projections on the axis overlap. The items fit in the bin iff
 * there are two graphs such that every pair of items is disjoint along at
 * least one axis, and, along each axis, the graph of the disjoint pairs is a
 * comparability graph whose cliques fit in the length of the bin.
 *
 * The search decides, for each axis and each pair of items, whether their
 * projections overlap or are disjoint, but not which item comes first: a
 * packing class represents many packings, and the positions are only built
 * once every pair is disjoint along some axis, from a transitive orientation
 * of the graphs of the disjoint pairs. Its running time doesn't depend on the
 * size of the bin.
 *
 * The rotations of the items are fixed first, the largest items first.
 */

#pragma once

#include "packingsolver/rectangle/optimize.hpp"

namespace packingsolver
{
namespace rectangle
{

struct TreeSearchPackingClassesOutput: Output
{
    /** Constructor. */
    TreeSearchPackingClassesOutput(const Instance& instance):
        Output(instance) { }


    /** Number of nodes. */
    Counter number_of_nodes = 0;
};

struct TreeSearchPackingClassesParameters: packingsolver::Parameters<Instance, Solution, Output>
{
};

/**
 * Solve an instance with the 'Feasibility' objective and a single bin.
 *
 * The bin must have no defects and no fixed items, and the instance no
 * unloading constraints, weight constraints and resources.
 */
TreeSearchPackingClassesOutput tree_search_packing_classes(
        const Instance& instance,
        const TreeSearchPackingClassesParameters& parameters = {});

}
}

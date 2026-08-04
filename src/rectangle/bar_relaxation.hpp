/**
 * Bar relaxation
 *
 * Column-generation bound for (multiple) Knapsack problems with rectangular
 * items, based on the "bar-relaxation" of Scheithauer (1999), specialized
 * here to two dimensions (the original paper is stated for three-dimensional
 * boxes), and generalized to several (possibly different) bin types the way
 * Scheithauer's own multi-container extension (his Section 4) generalizes
 * from one container to several identical ones.
 *
 * Also used (for a single bin) by Côté, Haouari & Iori (2021), Section 7.3,
 * under the name '2D-UKP', as a fast relaxation of the 2D-KP inside their
 * combinatorial-Benders cut-lifting procedure.
 *
 * The idea: rather than requiring a true non-overlapping 2D placement, slice
 * every bin into unit-thickness "bars" - horizontal rows (one-dimensional
 * bins of capacity equal to the bin width, filled by item widths) and
 * vertical columns (one-dimensional bins of capacity equal to the bin
 * height, filled by item heights) - and only require that a selected item
 * appears in enough row-bars and column-bars to match its footprint (its
 * height, resp. its width, worth of bars). This is a genuine relaxation: a
 * set of items that is "bar-feasible" is not necessarily 2D-feasible (bar
 * positions are not tracked, only their count), so this only ever yields a
 * bound, never an actual packing.
 *
 * For every bin type t (width W_t, height H_t, cost c_t, copies bounded by
 * [copies_min_t, copies_t]), and every item type j (possibly split into two
 * "oriented variants" if it can rotate: (width_j, height_j) and (height_j,
 * width_j)), the model reads:
 *
 * Variables:
 * - k_t >= 0: (relaxed) number of bins of type t used.
 * - n_{i,t} >= 0: number of copies of oriented item variant i placed in bins
 *   of type t.
 * - y_q^t >= 0: number of times row-pattern q (a multiset of oriented item
 *   variants whose widths sum to at most W_t) is used, for bin type t.
 * - x_p^t >= 0: number of times column-pattern p (a multiset of oriented
 *   item variants whose heights sum to at most H_t) is used, for bin type t.
 *
 * Program:
 *
 * max sum_i profit_i sum_t n_{i,t}       (Knapsack: maximize profit)
 * min sum_t cost_t k_t                   (BinPacking/VariableSizedBinPacking:
 *                                          minimize the bin cost; 'profit_i'
 *                                          below is then 0)
 * min 0                                  (Feasibility: no objective at all,
 *                                          only constraints to satisfy;
 *                                          'profit_i' and 'cost_t' below are
 *                                          then both 0)
 *
 * sum_q a_iq y_q^t = height_i n_{i,t}    for all bin types t, variants i
 *                                     (row-bars used matches n_{i,t}'s height)
 * sum_p b_ip x_p^t = width_i n_{i,t}     for all bin types t, variants i
 *                                     (column-bars used matches n_{i,t}'s width)
 * sum_q y_q^t <= H_t k_t                 for all bin types t
 *                          (no more row-bars than the bin is tall, times k_t)
 * sum_p x_p^t <= W_t k_t                 for all bin types t
 *                          (no more column-bars than the bin is wide, times k_t)
 * copies_min_t <= k_t <= copies_t        for all bin types t
 * sum_t (n_{i,t} for i a variant of j) <= copies_j    for all item types j
 *                                          (Knapsack: at most 'copies_j';
 *                                           every other objective: '=' -
 *                                           every copy of every item must be
 *                                           packed)
 *
 * (A row-bar is a horizontal, unit-height, full-width strip of the bin - a
 * bin of height H_t holds H_t of them, and a placed item spans as many of
 * them as its own height; symmetrically for column-bars, which are vertical,
 * unit-width, full-height strips, of which a bin of width W_t holds W_t.)
 *
 * The pricing problem for a row-pattern (resp. column-pattern) of bin type t
 * is a bounded 0-1 knapsack over the oriented item variants that fit bin
 * type t, of capacity W_t (resp. H_t), maximizing the sum of the (signed)
 * duals of the corresponding linking constraints - solved with
 * 'onedimensional::optimize' rather than a hand-written knapsack routine,
 * the same way other bound computations in this codebase reduce a
 * sub-problem to a one-dimensional Knapsack instance (see
 * 'conservative_scales.cpp'). For Feasibility (and, in principle,
 * BinPacking/VariableSizedBinPacking, though a finite bound is the
 * meaningful outcome there), the reported 'bound' reaching +infinity - the
 * extended-reals optimal value of an infeasible minimization problem - is a
 * sound (if not always conclusive - see below) proof of infeasibility.
 *
 * For 'BinPacking' specifically, since the model already tracks a per-bin-
 * type relaxed count 'k_t', the reported cost bound is converted to a bin-
 * *count* bound by dividing by the single bin type's cost - which is why
 * 'BinPacking' additionally requires exactly one bin type (unlike
 * 'VariableSizedBinPacking', which reports the cost bound directly and so
 * has no such restriction).
 *
 * Neither defects nor fixed items are modeled - dropping either constraint
 * can only ever relax the feasible region (a fixed item, though
 * unconditionally placed in the true problem, is not forced by dropping it
 * either: nothing stops the relaxed model from choosing to place it anyway,
 * so every true-feasible solution remains representable in the relaxed
 * model with at least as good an objective value), so for Knapsack
 * (maximization) the bound stays valid, just looser on instances where a
 * defect or fixed item actually matters, and for every other (minimization
 * or feasibility) objective, "infeasible/no cheaper even without them"
 * still soundly implies the same "with them" (while the converse says
 * nothing either way).
 */

#pragma once

#include "packingsolver/rectangle/optimize.hpp"

#include "columngenerationsolver/commons.hpp"

namespace packingsolver
{
namespace rectangle
{

struct BarRelaxationOutput: Output
{
    /** Constructor. */
    BarRelaxationOutput(const Instance& instance):
        Output(instance) { }
};

struct BarRelaxationParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** Linear programming solver. */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::CLP;
};

/**
 * Compute the bar-relaxation bound: a profit bound for 'Knapsack', a cost
 * bound for 'BinPacking' (bin count) and 'VariableSizedBinPacking' (total
 * cost), or an infeasibility proof for 'Feasibility'.
 *
 * Only supported for the 'Knapsack', 'Feasibility', 'BinPacking' and
 * 'VariableSizedBinPacking' objectives, and 'BinPacking' additionally
 * requires a single bin type (see the file-level comment above).
 */
BarRelaxationOutput bar_relaxation(
        const Instance& instance,
        const BarRelaxationParameters& parameters);

}
}

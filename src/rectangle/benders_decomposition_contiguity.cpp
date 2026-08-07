#include "rectangle/benders_decomposition_contiguity.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/solution_builder.hpp"
#include "rectangle/onedimentional_contiguity/milp.hpp"
#include "rectangle/onedimentional_contiguity/tree_search.hpp"

#include "mathoptsolverscmake/mathopt.hpp"
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangle;
using namespace packingsolver::rectangle::onedimentional_contiguity;

namespace
{

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// y-check //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * A unit with an already-fixed x-position (as chosen by the BMP), input to
 * 'y_check'.
 */
struct YCheckUnit
{
    Length x;
    Length width;
    Length height;
};

/**
 * One level of 'y_check''s search tree - the iterative counterpart of one
 * activation of the (former) recursive 'y_check_dfs' - covering the "niche"
 * (see 'y_check' below) as it stood when this level was entered, together
 * with enough state to resume its branching (the next unplaced unit to try,
 * whether the "close the niche" branch has been tried yet) and to undo
 * whichever branch is currently "in flight" one level down, should it fail.
 */
struct YCheckFrame
{
    Length niche_left;
    Length niche_right;
    Length min_height;
    ItemPos number_of_unplaced;

    /** Resume point for the loop over candidate units. */
    size_t next_unit_id = 0;

    /** Whether the "close the niche" branch has already been tried. */
    bool close_tried = false;

    /**
     * Columns touched, and (for a unit placement) which unit, by the
     * branch currently being explored one level down - so it can be
     * undone if that subtree fails.
     */
    bool action_is_close = false;
    size_t action_unit_id = 0;
    Length action_left = 0;
    Length action_right = 0;
};

/**
 * The 'y-check' slave problem (BSP): given each selected unit's fixed
 * x-position (and, implicitly, its orientation, via 'width'/'height'), find
 * y-positions for every unit such that no two overlap, if any exist. On
 * success, 'y' (resized to 'units.size()') holds each unit's y-coordinate,
 * in the same order as 'units'.
 *
 * Iterative (explicit-stack) version of the niche/skyline branch-and-bound
 * of Côté, Dell'Amico & Iori (2014), §3 ("Enumeration Tree for Problem
 * y-check"): each 'YCheckFrame' pushed onto 'stack' is one activation of
 * what used to be a recursive call, entered by finding the current "niche"
 * - the leftmost maximal run of columns at the skyline's current
 * global-minimum height - and branching on every still-unplaced unit whose
 * x-interval is entirely contained in the niche (place it at the niche's
 * current height), plus one more branch that closes the niche without
 * placing anything (raising its height to match its shortest neighbor, so
 * a later, still-unplaced unit spanning a wider area may become placeable
 * against it). 'backtracking' plays the role of a recursive call's return
 * value: 'true' means the branch most recently tried by the frame now on
 * top of the stack failed and must be undone before that frame's own loop
 * continues.
 */
bool y_check(
        Length bin_width,
        Length bin_height,
        const std::vector<YCheckUnit>& units,
        std::vector<Length>& y)
{
    std::vector<Length> h_used(bin_width, 0);
    std::vector<bool> placed(units.size(), false);
    y.assign(units.size(), 0);

    std::vector<YCheckFrame> stack;

    // Push a fresh frame for 'number_of_unplaced' remaining units, with its
    // niche computed from the current 'h_used' - i.e. one (formerly
    // recursive) call. Returns 'true' directly, without pushing anything,
    // on the base case (nothing left to place).
    auto push_frame = [&](ItemPos number_of_unplaced) -> bool
    {
        if (number_of_unplaced == 0)
            return true;

        YCheckFrame frame;
        frame.number_of_unplaced = number_of_unplaced;
        Length min_height = h_used[0];
        for (Length c = 1; c < bin_width; ++c)
            min_height = std::min(min_height, h_used[c]);
        Length niche_left = 0;
        while (h_used[niche_left] != min_height)
            ++niche_left;
        Length niche_right = niche_left;
        while (niche_right + 1 < bin_width && h_used[niche_right + 1] == min_height)
            ++niche_right;
        frame.min_height = min_height;
        frame.niche_left = niche_left;
        frame.niche_right = niche_right;
        stack.push_back(frame);
        return false;
    };

    if (push_frame((ItemPos)units.size()))
        return true;

    bool backtracking = false;
    while (!stack.empty()) {
        YCheckFrame& frame = stack.back();

        if (backtracking) {
            if (frame.action_is_close) {
                for (Length c = frame.action_left; c <= frame.action_right; ++c)
                    h_used[c] = frame.min_height;
            } else {
                placed[frame.action_unit_id] = false;
                for (Length c = frame.action_left; c <= frame.action_right; ++c)
                    h_used[c] = frame.min_height;
            }
            backtracking = false;
        }

        bool descended = false;
        while (frame.next_unit_id < units.size()) {
            size_t unit_id = frame.next_unit_id;
            ++frame.next_unit_id;
            if (placed[unit_id])
                continue;
            const YCheckUnit& unit = units[unit_id];
            Length unit_right = unit.x + unit.width - 1;
            if (unit.x < frame.niche_left || unit_right > frame.niche_right)
                continue;
            Length new_height = frame.min_height + unit.height;
            if (new_height > bin_height)
                continue;

            for (Length c = unit.x; c <= unit_right; ++c)
                h_used[c] = new_height;
            placed[unit_id] = true;
            y[unit_id] = frame.min_height;

            frame.action_is_close = false;
            frame.action_unit_id = unit_id;
            frame.action_left = unit.x;
            frame.action_right = unit_right;

            if (push_frame(frame.number_of_unplaced - 1))
                return true;
            descended = true;
            break;
        }
        if (descended)
            continue;

        if (!frame.close_tried) {
            frame.close_tried = true;
            Length h_left = (frame.niche_left == 0)? bin_height: h_used[frame.niche_left - 1];
            Length h_right = (frame.niche_right == bin_width - 1)? bin_height: h_used[frame.niche_right + 1];
            Length closed_height = std::min(h_left, h_right);
            // Always a strict increase (see the correctness argument in
            // the .hpp-adjacent design notes: the niche is, by
            // construction, the *leftmost* run at the *global* minimum
            // height, so any neighbor is either off the bin entirely - the
            // 'bin_height' stand-in - or strictly taller), except in the
            // fully-closed case where every column already sits at
            // 'bin_height' - guarded against below to guarantee
            // termination.
            if (closed_height > frame.min_height && closed_height <= bin_height) {
                for (Length c = frame.niche_left; c <= frame.niche_right; ++c)
                    h_used[c] = closed_height;

                frame.action_is_close = true;
                frame.action_left = frame.niche_left;
                frame.action_right = frame.niche_right;

                if (push_frame(frame.number_of_unplaced))
                    return true;
                continue;
            }
        }

        // Exhausted every option at this level: pop it and backtrack into
        // its parent.
        stack.pop_back();
        backtracking = true;
    }

    return false;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Cut strengthening /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * 'true' iff 'units' (their already-fixed x-positions, widths and heights)
 * admit no feasible y-assignment - the same BSP oracle 'y_check' provides
 * elsewhere in this file, just named for readability at every call site
 * below. An empty set is trivially feasible.
 */
bool infeasible(
        Length bin_width,
        Length bin_height,
        const std::vector<Candidate>& units)
{
    std::vector<YCheckUnit> y_check_units;
    for (const Candidate& unit: units)
        y_check_units.push_back({unit.x, unit.width, unit.height});
    std::vector<Length> y;
    return !(y_check_units.empty() || y_check(bin_width, bin_height, y_check_units, y));
}

/**
 * No-good cut strengthening, step (1) (Wang et al. 2025 §3.5.2): given
 * 'units' (already known 'infeasible'), recursively split it by a vertical
 * line at each of its own units' own x-position, into the units strictly to
 * its left and the units at or past it - a unit whose own interval
 * straddles the line belongs to neither side, and is simply dropped from
 * that particular split. Whichever side is both a strict subset and still
 * infeasible is recursed into; stops (returning 'units' unchanged) once no
 * split line yields any further reduction.
 */
std::vector<Candidate> shrink_via_split(
        Length bin_width,
        Length bin_height,
        const std::vector<Candidate>& units)
{
    for (const Candidate& split_unit: units) {
        Length split_x = split_unit.x;
        std::vector<Candidate> left;
        std::vector<Candidate> right;
        for (const Candidate& unit: units) {
            if (unit.x + unit.width <= split_x)
                left.push_back(unit);
            if (unit.x >= split_x)
                right.push_back(unit);
        }
        if (!left.empty()
                && left.size() < units.size()
                && infeasible(bin_width, bin_height, left)) {
            return shrink_via_split(bin_width, bin_height, left);
        }
        if (!right.empty()
                && right.size() < units.size()
                && infeasible(bin_width, bin_height, right)) {
            return shrink_via_split(bin_width, bin_height, right);
        }
    }
    return units;
}

/**
 * No-good cut strengthening, step (2) (Wang et al. 2025 §3.5.2): scan
 * vertical lines left to right, dropping every unit whose own left edge
 * sits exactly on the current line; then right to left, dropping every
 * unit whose own interval contains the current line. After each drop, the
 * reduction is kept only if the remaining set stays infeasible; it is
 * restored (the scan simply continues with the unreduced set) otherwise.
 */
std::vector<Candidate> shrink_via_sweep(
        Length bin_width,
        Length bin_height,
        std::vector<Candidate> subset)
{
    // Left to right: drop units whose own left edge is the current line.
    for (Length x = 0; x < bin_width; ++x) {
        std::vector<Candidate> trial;
        bool any_removed = false;
        for (const Candidate& unit: subset) {
            if (unit.x == x) {
                any_removed = true;
            } else {
                trial.push_back(unit);
            }
        }
        if (any_removed
                && !trial.empty()
                && infeasible(bin_width, bin_height, trial)) {
            subset = std::move(trial);
        }
    }
    // Right to left: drop units whose own interval contains the current line.
    for (Length x = bin_width - 1; x >= 0; --x) {
        std::vector<Candidate> trial;
        bool any_removed = false;
        for (const Candidate& unit: subset) {
            if (unit.x <= x && x < unit.x + unit.width) {
                any_removed = true;
            } else {
                trial.push_back(unit);
            }
        }
        if (any_removed
                && !trial.empty()
                && infeasible(bin_width, bin_height, trial)) {
            subset = std::move(trial);
        }
    }
    return subset;
}

/**
 * No-good cut strengthening, step (3) (Wang et al. 2025 §3.5.2): 12 scans
 * over the current subset - 6 deterministic orderings (non-decreasing
 * area, width, height, perimeter, x-coordinate, and "intersection score",
 * the number of other units in the current subset whose interval overlaps
 * this one's) plus 6 random orderings - each iterating in that order and
 * permanently removing a unit if the remaining set stays infeasible
 * without it (left alone, i.e. never actually removed from 'subset',
 * otherwise). Scans are chained sequentially: each one computes its own
 * order fresh from whatever 'subset' the previous scan left behind, so a
 * later scan can still shrink what an earlier one, in a different order,
 * could not.
 *
 * Units are compared for identity by value (see 'Candidate::operator==')
 * rather than by index into a shared array: since each unit's own
 * '(item_type_id, copy)' pair is unique within any subset built from a
 * single BMP solution, this is just as unambiguous as an index would be.
 */
std::vector<Candidate> shrink_via_removal(
        Length bin_width,
        Length bin_height,
        const Instance& instance,
        std::vector<Candidate> subset,
        std::mt19937_64& generator)
{
    auto intersection_score = [&](const Candidate& unit)
    {
        int score = 0;
        for (const Candidate& other: subset) {
            if (other == unit)
                continue;
            if (unit.x < other.x + other.width
                    && other.x < unit.x + unit.width) {
                ++score;
            }
        }
        return score;
    };

    auto scan = [&](const std::vector<Candidate>& order)
    {
        for (const Candidate& unit: order) {
            if (std::find(subset.begin(), subset.end(), unit) == subset.end())
                continue;
            std::vector<Candidate> trial;
            for (const Candidate& other: subset)
                if (!(other == unit))
                    trial.push_back(other);
            if (!trial.empty()
                    && infeasible(bin_width, bin_height, trial)) {
                subset = std::move(trial);
            }
        }
    };

    for (int order_type = 0; order_type < 6; ++order_type) {
        std::vector<Candidate> order = subset;
        switch (order_type) {
        case 0:
            std::sort(order.begin(), order.end(),
                    [&](const Candidate& a, const Candidate& b)
                    {
                        const ItemType& item_type_a = instance.item_type(a.item_type_id);
                        const ItemType& item_type_b = instance.item_type(b.item_type_id);
                        return item_type_a.rect.area() < item_type_b.rect.area();
                    });
            break;
        case 1:
            std::sort(order.begin(), order.end(),
                    [](const Candidate& a, const Candidate& b) { return a.width < b.width; });
            break;
        case 2:
            std::sort(order.begin(), order.end(),
                    [](const Candidate& a, const Candidate& b) { return a.height < b.height; });
            break;
        case 3:
            std::sort(order.begin(), order.end(),
                    [](const Candidate& a, const Candidate& b)
                    {
                        return 2 * (a.width + a.height) < 2 * (b.width + b.height);
                    });
            break;
        case 4:
            std::sort(order.begin(), order.end(),
                    [](const Candidate& a, const Candidate& b) { return a.x < b.x; });
            break;
        case 5:
            std::sort(order.begin(), order.end(),
                    [&](const Candidate& a, const Candidate& b)
                    {
                        return intersection_score(a) < intersection_score(b);
                    });
            break;
        }
        scan(order);
    }
    for (int i = 0; i < 6; ++i) {
        std::vector<Candidate> order = subset;
        std::shuffle(order.begin(), order.end(), generator);
        scan(order);
    }
    return subset;
}

/**
 * No-good cut strengthening, step (4) (Wang et al. 2025 §3.5.2, eqs.
 * (18a)-(18d)): given the final shrunk infeasible 'subset' (each unit
 * already at a fixed (x, orientation) from the BMP solution that failed
 * 'y_check'), solve an LP that widens each unit's own forbidden window
 * [l_i, r_i] as far as possible while guaranteeing every pair of units that
 * overlaps at its original fixed position keeps overlapping for any
 * positions within their own widened windows - i.e. the infeasibility
 * 'subset' witnesses is preserved throughout the whole product of windows,
 * not just at the single point actually solved. The final cut then covers
 * every one of a unit's own candidates (same orientation as originally
 * chosen - the LP's own overlap argument used that orientation's
 * width/height) whose x falls in [ceil(l_i*), floor(r_i*)], rather than
 * just the single one actually used - strictly stronger than
 * 'onedimentional_contiguity::build_positional_no_good_cut' while still
 * short of 'onedimentional_contiguity::build_covering_cut' (only sound once
 * every position has been ruled out, not merely a provably-still-infeasible
 * window).
 *
 * Rebuilds its own 'UnitsAndCandidates' from 'instance' (see
 * 'build_units_and_candidates') to look up each unit's own full candidate
 * list, needed for the final "every candidate within the lifted window"
 * step.
 */
NoGoodCut build_lifted_cut(
        Length bin_width,
        const Instance& instance,
        const std::vector<Candidate>& subset,
        const BendersDecompositionContiguityParameters& parameters)
{
    size_t n = subset.size();

    // overlaps[j] = indices (into 'subset') of units overlapping unit
    // 'subset[j]' at their original fixed positions.
    std::vector<std::vector<size_t>> overlaps(n);
    for (size_t j = 0; j < n; ++j) {
        const Candidate& candidate_j = subset[j];
        for (size_t i = 0; i < n; ++i) {
            if (i == j)
                continue;
            const Candidate& candidate_i = subset[i];
            if (candidate_j.x < candidate_i.x + candidate_i.width
                    && candidate_i.x < candidate_j.x + candidate_j.width) {
                overlaps[j].push_back(i);
            }
        }
    }

    // Variables: l_i at index 2 * i, r_i at index 2 * i + 1.
    mathoptsolverscmake::MathOptModel model((int)(2 * n), 0, 0);
    model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;
    for (size_t i = 0; i < n; ++i) {
        const Candidate& candidate_i = subset[i];
        model.variables_types[2 * i] = mathoptsolverscmake::VariableType::Continuous;
        model.variables_types[2 * i + 1] = mathoptsolverscmake::VariableType::Continuous;
        // (18c): 0 <= l_i <= x_i.
        model.variables_lower_bounds[2 * i] = 0.0;
        model.variables_upper_bounds[2 * i] = (double)candidate_i.x;
        // (18d): x_i <= r_i <= W - w_i.
        model.variables_lower_bounds[2 * i + 1] = (double)candidate_i.x;
        model.variables_upper_bounds[2 * i + 1] = (double)(bin_width - candidate_i.width);
        // (18a): max sum (r_i - l_i).
        model.objective_coefficients[2 * i] = -1.0;
        model.objective_coefficients[2 * i + 1] = 1.0;
    }
    // (18b): l_j + w_j >= r_i + 1  <=>  r_i - l_j <= w_j - 1, for every
    // ordered pair (j, i) with i overlapping j at their original
    // positions - enumerating every such ordered pair (not just unordered
    // ones) covers both directions of the inequality needed to guarantee
    // mutual overlap throughout the whole [l, r] product (see the
    // function's own doc comment).
    for (size_t j = 0; j < n; ++j) {
        const Candidate& candidate_j = subset[j];
        for (size_t i: overlaps[j]) {
            model.constraints_starts.push_back((int)model.elements_variables.size());
            model.elements_variables.push_back((int)(2 * i + 1));
            model.elements_coefficients.push_back(1.0);
            model.elements_variables.push_back((int)(2 * j));
            model.elements_coefficients.push_back(-1.0);
            model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            model.constraints_upper_bounds.push_back((double)candidate_j.width - 1.0);
        }
    }

    if (parameters.master_problem_milp_solver != mathoptsolverscmake::SolverName::Highs)
        throw std::invalid_argument(FUNC_SIGNATURE);

    std::vector<double> solution;
#ifdef HIGHS_FOUND
    Highs highs;
    mathoptsolverscmake::reduce_printout(highs);
    mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
    mathoptsolverscmake::load(highs, model);
    mathoptsolverscmake::solve(highs);
    bool proven_infeasible =
        (highs.getModelStatus() == HighsModelStatus::kInfeasible
         || highs.getModelStatus() == HighsModelStatus::kUnboundedOrInfeasible);
    if (!proven_infeasible)
        solution = mathoptsolverscmake::get_solution(highs);
#else
    throw std::invalid_argument(FUNC_SIGNATURE);
#endif

    // The LP is always feasible ('l_i = r_i = x_i' for every i trivially
    // satisfies every (18b) row, since it just restates that the units
    // already overlap at their own original positions), so only a timeout
    // could leave 'solution' empty - fall back to the un-lifted window
    // ([x_i, x_i], still valid, just not widened) in that case.
    UnitsAndCandidates units_and_candidates = build_units_and_candidates(instance, instance.bin_type(0));
    NoGoodCut cut;
    cut.upper_bound = (ItemPos)n - 1;
    for (size_t i = 0; i < n; ++i) {
        const Candidate& candidate_i = subset[i];
        Length l = solution.empty()?
            candidate_i.x:
            (Length)std::ceil(solution[2 * i] - 1e-6);
        Length r = solution.empty()?
            candidate_i.x:
            (Length)std::floor(solution[2 * i + 1] + 1e-6);
        const std::vector<size_t>& unit_candidates
            = units_and_candidates.candidates_by_item_type_and_copy
                [candidate_i.item_type_id][candidate_i.copy];
        for (size_t candidate_id: unit_candidates) {
            const Candidate& other = units_and_candidates.candidates[candidate_id];
            if (other.rotate == candidate_i.rotate && other.x >= l && other.x <= r)
                cut.candidate_ids.push_back(candidate_id);
        }
    }
    return cut;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// LBD /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Result of 'run_lbd' below.
 */
struct LbdResult
{
    enum class Status
    {
        /** A feasible pattern for the whole item set was found (possibly with different x-positions than the outer BMP's own choice). */
        Feasible,
        /** No pattern exists for this item set at all, at any x-positions. */
        Infeasible,
        /** Timed out before reaching either conclusion. */
        Inconclusive,
    };

    /** Constructor. */
    LbdResult(const Instance& instance): solution(instance) { }

    Status status = Status::Inconclusive;

    /**
     * The pattern for the item set, in 'instance''s own bin/item type ids
     * (only meaningful if 'status == Feasible').
     */
    Solution solution;
};

/**
 * Local Benders Decomposition (LBD, Wang et al. 2025 §3.4).
 *
 * Given the item set I selected by an outer BMP solution whose own (unit,
 * x, orientation) choice failed 'y_check', LBD either finds *some* feasible
 * pattern using exactly I (any x-positions, not just the outer BMP's own
 * choice) or proves that none exists at all - the latter a strictly
 * stronger fact than the outer BMP's specific combination failing, since it
 * rules out every possible positioning of I at once, not just the one the
 * outer BMP happened to propose.
 *
 * Only ever called for 'Knapsack' (see the caller in
 * 'benders_decomposition_contiguity' below): for 'Feasibility', I is always
 * every unit in the instance (every item type mandatory), never a genuine
 * subset the BMP chose among several - LBD would then just recurse into an
 * equivalent, equally-sized copy of the very problem being solved, so the
 * caller cuts the specific failed combination directly instead.
 *
 * LBMP(I) (Wang et al. 2025 eq. (10a)-(10c): the same BMP MILP, restricted
 * to I with every unit now mandatory rather than optional, and with no
 * profit objective - a pure feasibility question) is exactly a fresh,
 * single-bin 'Feasibility' instance containing only the item types with a
 * unit in I, each with as many copies as I has units of it (Feasibility
 * resolves 'copies_min' to 'copies' automatically at 'build()' - see
 * 'InstanceBuilder::build()' - so every one of them is mandatory without
 * having to set it explicitly). This is therefore implemented as a direct
 * recursive call to 'benders_decomposition_contiguity' itself on that
 * sub-instance, rather than as its own separate
 * MILP-plus-manual-no-good-cut loop: the recursive call already provides
 * exactly the BMP/BSP loop (its own
 * no-good cuts, and, if it in turn gets stuck, its own nested LBD) LBMP(I)
 * needs, with nothing left here to reimplement. The bin type (dimensions,
 * resources, ...) is copied unchanged from 'instance' via the
 * 'add_bin_type(instance, id)' "copy from original" overload, which also
 * carries every relevant resource's consumption schedule over (see
 * 'add_item_type(instance, id)') - so this sub-instance remains subject to
 * the same resource constraints as 'instance' itself, even though the
 * outer BMP's own 'add_resource_constraints' rows already guarantee I
 * itself is resource-feasible (repositioning I can't change its resource
 * consumption).
 */
LbdResult run_lbd(
        const Instance& instance,
        const std::vector<Candidate>& item_set_units,
        const BendersDecompositionContiguityParameters& parameters)
{
    // Count how many units of each item type are in the item set, and
    // record, for each item type added to the sub-instance (in that same
    // order), which original item type it came from - needed to translate
    // the sub-instance's own solution back into 'instance''s own item type
    // ids below.
    std::vector<ItemPos> unit_counts(instance.number_of_item_types(), 0);
    for (const Candidate& unit: item_set_units)
        ++unit_counts[unit.item_type_id];

    InstanceBuilder lbmp_instance_builder;
    lbmp_instance_builder.set_objective(Objective::Feasibility);
    lbmp_instance_builder.set_parameters(instance.parameters());
    lbmp_instance_builder.add_bin_type(instance, 0);
    std::vector<ItemTypeId> sub_to_original_item_type_id;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (unit_counts[item_type_id] == 0)
            continue;
        ItemTypeId sub_item_type_id = lbmp_instance_builder.add_item_type(instance, item_type_id);
        lbmp_instance_builder.set_item_type_copies(sub_item_type_id, unit_counts[item_type_id]);
        sub_to_original_item_type_id.push_back(item_type_id);
    }
    Instance lbmp_instance = lbmp_instance_builder.build();

    BendersDecompositionContiguityParameters lbmp_parameters;
    lbmp_parameters.verbosity_level = 0;
    lbmp_parameters.timer = parameters.timer;
    lbmp_parameters.optimization_mode = parameters.optimization_mode;
    lbmp_parameters.use_tree_search = parameters.use_tree_search;
    lbmp_parameters.master_problem_milp_solver = parameters.master_problem_milp_solver;
    lbmp_parameters.master_problem_tree_search_guide_id = parameters.master_problem_tree_search_guide_id;
    lbmp_parameters.master_problem_tree_search_not_anytime_queue_size = parameters.master_problem_tree_search_not_anytime_queue_size;
    lbmp_parameters.seed = parameters.seed;
    BendersDecompositionContiguityOutput lbmp_output = benders_decomposition_contiguity(lbmp_instance, lbmp_parameters);

    LbdResult result(instance);
    if (lbmp_output.is_proven_infeasible) {
        result.status = LbdResult::Status::Infeasible;
        return result;
    }
    const Solution& lbmp_solution = lbmp_output.solution_pool.best();
    if (!lbmp_solution.feasible() || !lbmp_solution.full())
        return result;  // Inconclusive (timed out).

    result.status = LbdResult::Status::Feasible;
    result.solution.append_bin(lbmp_solution, 0, 1, {}, sub_to_original_item_type_id);
    return result;
}

}

BendersDecompositionContiguityOutput packingsolver::rectangle::benders_decomposition_contiguity(
        const Instance& instance,
        const BendersDecompositionContiguityParameters& parameters)
{
    if (instance.objective() != Objective::Knapsack
            && instance.objective() != Objective::Feasibility)
        throw std::invalid_argument(FUNC_SIGNATURE);
    if (instance.number_of_bin_types() != 1
            || instance.bin_type(0).copies != 1)
        throw std::invalid_argument(FUNC_SIGNATURE);

    BendersDecompositionContiguityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    const BinType& bin_type = instance.bin_type(0);

    std::vector<NoGoodCut> no_good_cuts;
    std::mt19937_64 generator(parameters.seed);

    for (output.number_of_iterations = 0;
            ;
            ++output.number_of_iterations) {
        if (parameters.maximum_number_of_iterations >= 0
                && output.number_of_iterations >= parameters.maximum_number_of_iterations) {
            break;
        }
        if (parameters.timer.needs_to_end())
            break;

        // The master problem is solved repeatedly, once per Benders
        // iteration - always as a bounded inner-loop building block, never
        // as a standalone anytime search (see 'onedimentional_contiguity::
        // TreeSearchParameters::optimization_mode'), regardless of this
        // algorithm's own 'optimization_mode'. Only the 'NotAnytimeSequential'
        // case is preserved as such (matching 'benders_decomposition.cpp''s
        // own sub-solves): everything else (including 'Anytime') maps to
        // 'NotAnytimeDeterministic'.
        OptimizationMode master_problem_optimization_mode
            = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
            OptimizationMode::NotAnytimeSequential:
            OptimizationMode::NotAnytimeDeterministic;

        OnedimensionalContiguityResult bmp_result;
        if (parameters.use_tree_search) {
            TreeSearchParameters tree_search_parameters;
            tree_search_parameters.timer = parameters.timer;
            tree_search_parameters.optimization_mode = master_problem_optimization_mode;
            tree_search_parameters.guide_id = parameters.master_problem_tree_search_guide_id;
            tree_search_parameters.not_anytime_queue_size = parameters.master_problem_tree_search_not_anytime_queue_size;
            bmp_result = tree_search(instance, no_good_cuts, tree_search_parameters);
        } else {
            MilpParameters milp_parameters;
            milp_parameters.timer = parameters.timer;
            milp_parameters.optimization_mode = master_problem_optimization_mode;
            milp_parameters.solver = parameters.master_problem_milp_solver;
            bmp_result = milp(instance, no_good_cuts, milp_parameters);
        }

        if (parameters.timer.needs_to_end())
            break;

        if (bmp_result.infeasible) {
            // The relaxation itself has no feasible solution (including
            // every cut added so far, each a sound necessary condition):
            // the original instance has none either.
            algorithm_formatter.update_is_proven_infeasible();
            break;
        }

        if (instance.objective() == Objective::Knapsack)
            algorithm_formatter.update_knapsack_bound(bmp_result.objective_value);

        if (parameters.timer.needs_to_end())
            break;

        std::vector<YCheckUnit> y_check_units;
        for (const Candidate& candidate: bmp_result.selected_units)
            y_check_units.push_back({candidate.x, candidate.width, candidate.height});
        std::vector<Length> y;
        bool feasible = y_check_units.empty()
            || y_check(bin_type.rect.x, bin_type.rect.y, y_check_units, y);

        if (feasible) {
            SolutionBuilder solution_builder(instance);
            if (!bmp_result.selected_units.empty()) {
                BinPos bin_pos = solution_builder.add_bin(0, 1);
                for (size_t index = 0; index < bmp_result.selected_units.size(); ++index) {
                    const Candidate& candidate = bmp_result.selected_units[index];
                    solution_builder.add_item(
                            bin_pos,
                            candidate.item_type_id,
                            {candidate.x, y[index]},
                            candidate.rotate);
                }
            }
            Solution solution = solution_builder.build();
            std::stringstream ss;
            ss << "BDC it " << output.number_of_iterations;
            algorithm_formatter.update_solution(solution, ss.str());
            break;
        }

        if (instance.objective() == Objective::Knapsack) {
            // The BMP's exact (unit, x, orientation) choice is infeasible,
            // but a different x-positioning of the very same item *subset*
            // might still work (Wang et al. 2025 §3.4): escalate to LBD
            // before giving up on this subset entirely, rather than
            // cutting it off right away. Only meaningful when the item set
            // is genuinely a subset the BMP chose to select - see the
            // 'Feasibility' branch below for why it is skipped there.
            LbdResult lbd_result = run_lbd(
                    instance, bmp_result.selected_units, parameters);

            if (lbd_result.status == LbdResult::Status::Inconclusive)
                break;

            if (lbd_result.status == LbdResult::Status::Feasible) {
                std::stringstream ss;
                ss << "BDC it " << output.number_of_iterations << " (LBD)";
                algorithm_formatter.update_solution(lbd_result.solution, ss.str());
                break;
            }

            // LBD proved that no positioning of this item set is feasible
            // at all - a strictly stronger fact than the BMP's own (unit,
            // x, orientation) combination failing on its own, so cut off
            // the whole item set at any position (Wang et al. 2025's
            // §3.1.1 covering cut) rather than just that one combination -
            // a positional cut alone would leave the BMP free to keep
            // proposing other positionings of this same already-proven-
            // infeasible item set, each needing its own (now recursive)
            // call to 'run_lbd' to re-derive the same fact.
            no_good_cuts.push_back(build_covering_cut(
                    instance, bmp_result.selected_units));
        } else {
            // 'Feasibility' always sets 'copies_min == copies' (see
            // 'onedimentional_contiguity::milp'/'::tree_search'), so the
            // BMP's own item set is never a genuine *subset* choice - it is
            // always every unit in the instance. LBD's own sub-instance
            // ('run_lbd') would then be, up to the no-good cuts accumulated
            // so far at this level (which it does not inherit), essentially
            // 'instance' itself: escalating to it would recurse into an
            // equivalent, equally-sized copy of this very problem - at best
            // redundant (this loop's own no-good cuts already retry
            // different positionings of the same, already-mandatory item
            // set directly), at worst compounding into unboundedly deep
            // recursion if that copy also needs to escalate. Cut off just
            // the specific failed (unit, x, orientation) combination
            // instead - this is exactly LBMP's own inner loop (Wang et al.
            // 2025 §3.4/§3.5.2), so its own cut strengthening procedure
            // applies here directly: shrink the infeasible unit set (steps
            // (1)-(3): binary split, line sweep, 12-scan iterative removal)
            // and lift the surviving units' own forbidden windows via LP
            // (step (4)) before cutting, rather than the raw,
            // single-position
            // 'onedimentional_contiguity::build_positional_no_good_cut'.
            std::vector<Candidate> shrunk = shrink_via_split(
                    bin_type.rect.x, bin_type.rect.y,
                    bmp_result.selected_units);
            shrunk = shrink_via_sweep(
                    bin_type.rect.x, bin_type.rect.y, std::move(shrunk));
            shrunk = shrink_via_removal(
                    bin_type.rect.x, bin_type.rect.y, instance,
                    std::move(shrunk), generator);
            no_good_cuts.push_back(build_lifted_cut(
                    bin_type.rect.x, instance, shrunk, parameters));
        }
    }

    algorithm_formatter.end();
    return output;
}

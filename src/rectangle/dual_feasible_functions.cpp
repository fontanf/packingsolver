#include "rectangle/dual_feasible_functions.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"

#include <array>
#include <map>

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

Length f_ccm_0(
        Length capacity,
        Length k,
        Length length)
{
    if (length > capacity - k) {
        return capacity;
    } else if (length < k) {
        return 0;
    } else {
        return length;
    }
}

Length f_ccm_1(
        Length capacity,
        Length k,
        Length length,
        ItemPos value)
{
    if (length > capacity / 2) {
        // MC(C, S) − MC(C − x, S)
        if (value < 0)
            throw std::invalid_argument(
                    FUNC_SIGNATURE + "; "
                    "capacity: " + std::to_string(capacity) + "; "
                    "k: " + std::to_string(k) + "; "
                    "length: " + std::to_string(length) + "; "
                    "value: " + std::to_string(value) + "; ");
        return value;
    } else if (length < k) {
        return 0;
    } else {
        return 1;
    }
}

Length f_ccm_2(
        Length capacity,
        Length k,
        Length length)
{
    if (length > capacity / 2) {
        return 2 * (capacity / k - (capacity - length) / k);
    } else if (length == capacity / 2 && capacity % 2 == 0) {
        return capacity / k;
    } else {
        return 2 * (length / k);
    }
}

/**
 * Greedy maximum cardinality: given a pool of piece lengths sorted
 * ascending (with multiplicities), greedily fill 'capacity' and return how
 * many pieces were used. Sorting ascending and greedily filling is optimal
 * for maximizing the count of pieces packed subject to a sum constraint.
 */
ItemPos greedy_maximum_cardinality(
        Length capacity,
        const std::vector<std::pair<Length, ItemPos>>& sorted_pool)
{
    Length used = 0;
    ItemPos count = 0;
    for (const auto& p: sorted_pool) {
        Length length = p.first;
        ItemPos copies = p.second;
        if (used + copies * length < capacity) {
            used += copies * length;
            count += copies;
        } else {
            count += (capacity - used) / length;
            break;
        }
    }
    return count;
}

/**
 * Length 'item_type' effectively contributes to the "medium items" pool on
 * one axis (breakpoint 'k', 'half_capacity' being half the bin's capacity
 * on that axis) - used only to upper-bound how many medium items can be
 * packed alongside a big item (f_ccm_1's "MC(C, S) - MC(C - x, S)" term).
 *
 * 'own_dimension' is the item's dimension along this axis if it were
 * oriented that way (rect.x for width, rect.y for height); 'other_dimension'
 * is its other dimension, only relevant if the item is not oriented.
 *
 * For an unoriented item, either dimension could end up on this axis;
 * using whichever qualifying dimension is smallest can only overestimate -
 * never underestimate - the true achievable count, keeping the result a
 * valid upper bound regardless of which orientation actually ends up being
 * used. Returns -1 if the item cannot be "medium" on this axis in any
 * orientation.
 */
Length medium_pool_length(
        const ItemType& item_type,
        Length own_dimension,
        Length other_dimension,
        Length k,
        Length half_capacity)
{
    if (item_type.oriented) {
        if (k <= own_dimension && own_dimension <= half_capacity)
            return own_dimension;
        return -1;
    }
    Length lo = (std::min)(own_dimension, other_dimension);
    Length hi = (std::max)(own_dimension, other_dimension);
    if (k <= lo && lo <= half_capacity)
        return lo;
    if (k <= hi && hi <= half_capacity)
        return hi;
    return -1;
}

/**
 * f_ccm_1 for a specific assumed length on one axis, looking up the
 * "maximum cardinality" value it needs only when it actually falls in the
 * "big item" branch (length > capacity / 2); 'excluded_cardinality' must
 * have an entry for 'length' whenever this branch is taken.
 */
Length f_ccm_1_axis(
        Length capacity,
        Length k,
        Length length,
        ItemPos full_cardinality,
        const std::map<Length, ItemPos>& excluded_cardinality)
{
    if (length > capacity / 2) {
        ItemPos excluded = excluded_cardinality.at(length);
        return f_ccm_1(capacity, k, length, full_cardinality - excluded);
    }
    return f_ccm_1(capacity, k, length, 0);
}

/**
 * Coefficient of 'item_type' for breakpoints (k, l) and DFF families
 * (family_w, family_h in {0: f_ccm_0, 1: f_ccm_1, 2: f_ccm_2}).
 *
 * If the item is not oriented, its true contribution depends on an
 * orientation choice this per-item-type coefficient scheme doesn't track;
 * using the smaller of its two orientations' coefficients stays a valid
 * (safe) lower bound on the true contribution regardless of which
 * orientation actually ends up being used.
 */
Length item_coefficient(
        const ItemType& item_type,
        int family_w,
        int family_h,
        Length k,
        Length l,
        Length bin_w,
        Length bin_h,
        ItemPos full_cardinality_w,
        const std::map<Length, ItemPos>& excluded_cardinality_w,
        ItemPos full_cardinality_h,
        const std::map<Length, ItemPos>& excluded_cardinality_h)
{
    auto eval_w = [&](Length length) -> Length
    {
        switch (family_w) {
        case 0: return f_ccm_0(bin_w, k, length);
        case 1: return f_ccm_1_axis(bin_w, k, length, full_cardinality_w, excluded_cardinality_w);
        default: return f_ccm_2(bin_w, k, length);
        }
    };
    auto eval_h = [&](Length length) -> Length
    {
        switch (family_h) {
        case 0: return f_ccm_0(bin_h, l, length);
        case 1: return f_ccm_1_axis(bin_h, l, length, full_cardinality_h, excluded_cardinality_h);
        default: return f_ccm_2(bin_h, l, length);
        }
    };

    Length a = eval_w(item_type.rect.x) * eval_h(item_type.rect.y);
    if (item_type.oriented)
        return a;
    Length b = eval_w(item_type.rect.y) * eval_h(item_type.rect.x);
    return (std::min)(a, b);
}

/**
 * Precomputed tables shared by every (k, l, family_w, family_h) combo of
 * the breakpoint sweep: candidate breakpoints on each axis, and the
 * "maximum cardinality" bookkeeping f_ccm_1 needs. These only depend on
 * 'instance' and 'bin_type', not on any particular candidate selection, so
 * they are computed once and reused - both across every combo of a single
 * sweep, and across every call site that runs a sweep over the same
 * instance (the bin-count/profit bound below, and the Benders
 * decomposition pre-subproblem cut checker).
 */
struct DualFeasibleFunctionsTables
{
    std::vector<Length> widths;
    std::vector<Length> heights;
    std::vector<ItemPos> full_cardinality_w;
    std::vector<std::map<Length, ItemPos>> excluded_cardinality_w;
    std::vector<ItemPos> full_cardinality_h;
    std::vector<std::map<Length, ItemPos>> excluded_cardinality_h;
};

DualFeasibleFunctionsTables compute_dual_feasible_functions_tables(
        const Instance& instance,
        const BinType& bin_type)
{
    DualFeasibleFunctionsTables tables;

    // Compute all distinct widths and heights. Both dimensions of every
    // non-oriented item type feed both axes: it may end up presenting
    // either side along the bin's width or its height, so a breakpoint
    // that only matters for one orientation should not be missed.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        std::vector<Length> width_candidates = item_type.oriented ?
            std::vector<Length>{item_type.rect.x}:
            std::vector<Length>{item_type.rect.x, item_type.rect.y};
        for (Length d: width_candidates) {
            // A dimension exceeding the bin's own capacity on this axis can
            // never be validly placed along it at all (this happens for a
            // non-oriented item whose *other* dimension - the one feeding
            // this axis's table only because rotation is allowed - is
            // bigger than the bin itself here), so folding it via
            // 'capacity - d' would produce a negative breakpoint, pushing k
            // outside the CCM functions' valid domain of [1, capacity/2]
            // and corrupting the bound. Simply skip it: the item's other,
            // valid dimension still contributes its own breakpoint normally.
            if (d > bin_type.rect.x)
                continue;
            if (d == bin_type.rect.x) {
            } else if (d <= bin_type.rect.x / 2) {
                tables.widths.push_back(d);
            } else {
                tables.widths.push_back(bin_type.rect.x - d);
            }
        }
        std::vector<Length> height_candidates = item_type.oriented ?
            std::vector<Length>{item_type.rect.y}:
            std::vector<Length>{item_type.rect.x, item_type.rect.y};
        for (Length d: height_candidates) {
            if (d > bin_type.rect.y)
                continue;
            if (d == bin_type.rect.y) {
            } else if (d <= bin_type.rect.y / 2) {
                tables.heights.push_back(d);
            } else {
                tables.heights.push_back(bin_type.rect.y - d);
            }
        }
    }
    // capacity / 2 is where f_ccm_0/f_ccm_2 themselves switch branch: it is
    // a meaningful breakpoint on its own, regardless of whether any item
    // dimension happens to fold onto it (this matters most for rotation:
    // e.g. an item that is "big" in both orientations, on both axes, needs
    // this breakpoint to be recognized as such if no item dimension is
    // exactly at half the bin's capacity).
    // A breakpoint of 0 is never valid (f_ccm_0/f_ccm_1/f_ccm_2 all divide
    // by k), which capacity / 2 degenerates to when capacity is 1.
    if (bin_type.rect.x / 2 > 0)
        tables.widths.push_back(bin_type.rect.x / 2);
    if (bin_type.rect.y / 2 > 0)
        tables.heights.push_back(bin_type.rect.y / 2);
    sort(tables.widths.begin(), tables.widths.end());
    sort(tables.heights.begin(), tables.heights.end());
    tables.widths.erase(unique(tables.widths.begin(), tables.widths.end()), tables.widths.end());
    tables.heights.erase(unique(tables.heights.begin(), tables.heights.end()), tables.heights.end());

    // Distinct "big" (> half the bin's capacity on that axis) dimension
    // values that some item type might present along each axis - these are
    // the only values f_ccm_1_axis will ever need an excluded-cardinality
    // entry for.
    std::vector<Length> big_values_w;
    std::vector<Length> big_values_h;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.rect.x > bin_type.rect.x / 2)
            big_values_w.push_back(item_type.rect.x);
        if (!item_type.oriented && item_type.rect.y > bin_type.rect.x / 2)
            big_values_w.push_back(item_type.rect.y);
        if (item_type.rect.y > bin_type.rect.y / 2)
            big_values_h.push_back(item_type.rect.y);
        if (!item_type.oriented && item_type.rect.x > bin_type.rect.y / 2)
            big_values_h.push_back(item_type.rect.x);
    }
    sort(big_values_w.begin(), big_values_w.end());
    big_values_w.erase(unique(big_values_w.begin(), big_values_w.end()), big_values_w.end());
    sort(big_values_h.begin(), big_values_h.end());
    big_values_h.erase(unique(big_values_h.begin(), big_values_h.end()), big_values_h.end());

    // Compute maximum cardinalities.
    tables.full_cardinality_w.resize(tables.widths.size());
    tables.excluded_cardinality_w.resize(tables.widths.size());
    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)tables.widths.size(); ++k_pos) {
        Length k = tables.widths[k_pos];
        std::vector<std::pair<Length, ItemPos>> pool;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            Length length = medium_pool_length(
                    item_type,
                    item_type.rect.x,
                    item_type.rect.y,
                    k,
                    bin_type.rect.x / 2);
            if (length >= 0)
                pool.push_back({length, item_type.copies});
        }
        sort(pool.begin(), pool.end());
        tables.full_cardinality_w[k_pos] = greedy_maximum_cardinality(bin_type.rect.x, pool);
        for (Length big_value: big_values_w) {
            tables.excluded_cardinality_w[k_pos][big_value] = greedy_maximum_cardinality(
                    bin_type.rect.x - big_value,
                    pool);
        }
    }
    tables.full_cardinality_h.resize(tables.heights.size());
    tables.excluded_cardinality_h.resize(tables.heights.size());
    for (ItemTypeId l_pos = 0; l_pos < (ItemTypeId)tables.heights.size(); ++l_pos) {
        Length l = tables.heights[l_pos];
        std::vector<std::pair<Length, ItemPos>> pool;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            Length length = medium_pool_length(
                    item_type,
                    item_type.rect.y,
                    item_type.rect.x,
                    l,
                    bin_type.rect.y / 2);
            if (length >= 0)
                pool.push_back({length, item_type.copies});
        }
        sort(pool.begin(), pool.end());
        tables.full_cardinality_h[l_pos] = greedy_maximum_cardinality(bin_type.rect.y, pool);
        for (Length big_value: big_values_h) {
            tables.excluded_cardinality_h[l_pos][big_value] = greedy_maximum_cardinality(
                    bin_type.rect.y - big_value,
                    pool);
        }
    }

    return tables;
}

/**
 * 1D relaxation (Dantzig / Dembo-Hammer) profit upper bound: sort items by
 * decreasing profit / volume ratio and greedily fill 'capacity', taking the
 * last item fractionally. Items with a scaled volume of 0 are free (they
 * consume no capacity) and are always fully included.
 *
 * 'volumes[item_type_id]' is the item's scaled volume under some dual
 * feasible function; since that function preserves packing feasibility,
 * this is a valid upper bound on the knapsack objective for any choice of
 * dual feasible function.
 */
Profit dantzig_profit_bound(
        const Instance& instance,
        const std::vector<Length>& volumes,
        double capacity)
{
    Profit bound = 0.0;
    std::vector<ItemTypeId> sorted_item_type_ids;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        // An item that doesn't fit the bin at all can never be packed,
        // regardless of what its (possibly zero, after DFF rounding)
        // scaled volume suggests.
        if (!instance.fits_some_bin(item_type_id))
            continue;
        const ItemType& item_type = instance.item_type(item_type_id);
        if (volumes[item_type_id] <= 0) {
            bound += item_type.profit * item_type.copies;
        } else {
            sorted_item_type_ids.push_back(item_type_id);
        }
    }
    std::sort(
            sorted_item_type_ids.begin(),
            sorted_item_type_ids.end(),
            [&instance, &volumes](
                ItemTypeId item_type_id_1,
                ItemTypeId item_type_id_2)
            {
                const ItemType& item_type_1 = instance.item_type(item_type_id_1);
                const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                return item_type_1.profit * volumes[item_type_id_2]
                    > item_type_2.profit * volumes[item_type_id_1];
            });
    double remaining_capacity = capacity;
    for (ItemTypeId item_type_id: sorted_item_type_ids) {
        if (remaining_capacity <= 0)
            break;
        const ItemType& item_type = instance.item_type(item_type_id);
        double item_total_volume = (double)volumes[item_type_id] * item_type.copies;
        if (item_total_volume <= remaining_capacity) {
            bound += item_type.profit * item_type.copies;
            remaining_capacity -= item_total_volume;
        } else {
            bound += item_type.profit * (remaining_capacity / volumes[item_type_id]);
            remaining_capacity = 0;
        }
    }
    return bound;
}

}

DualFeasibleFunctionsOutput packingsolver::rectangle::dual_feasible_functions(
        const Instance& instance,
        const DualFeasibleFunctionsParameters& parameters)
{
    const BinType& bin_type = instance.bin_type(0);

    if (instance.objective() != Objective::BinPacking
            && instance.objective() != Objective::Feasibility
            && instance.objective() != Objective::Knapsack) {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }
    if (instance.number_of_bin_types() != 1) {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }

    DualFeasibleFunctionsOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    bool all_items_oriented = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (!item_type.oriented) {
            all_items_oriented = false;
            break;
        }
    }

    // If there are some non-oriented items, also run the strategy from
    // clautiaux2007 (build a modified instance containing, for each item,
    // one copy per orientation; compute the bound on that fully-oriented,
    // squared-up modified instance; divide it by 2), on top of the sweep
    // below (which now handles rotation itself via item_coefficient's
    // min-of-two-orientations). Neither technique dominates the other in
    // general - e.g. this one pays no "squaring tax" on non-square bins,
    // but doesn't get f_ccm_1's cross-item bookkeeping for ambiguous items
    // - so take the max of both.
    //
    // This doesn't apply to the Knapsack objective: the recursive doubling
    // strategy below only produces a bin count bound, which does not
    // translate into a knapsack profit bound (profit does not simply halve
    // when an item is duplicated once per orientation).
    BinPos clautiaux_bound = 0;
    if (!all_items_oriented && instance.objective() != Objective::Knapsack) {
        for (;;) {
            // Build modified instance.
            // Always use 'BinPacking' here, regardless of the original
            // instance's objective: this modified instance only exists to
            // compute a bin count lower bound via the recursive call below,
            // never to be checked for feasibility itself.
            InstanceBuilder modified_instance_builder;
            modified_instance_builder.set_objective(Objective::BinPacking);
            modified_instance_builder.set_parameters(instance.parameters());
            // Add bins and dummy items.
            if (bin_type.rect.x == bin_type.rect.y) {
                BinTypeId modified_bin_type_id = modified_instance_builder.add_bin_type(
                        bin_type.rect.x,
                        bin_type.rect.y);
                modified_instance_builder.set_bin_type_copies(
                        modified_bin_type_id,
                        2 * instance.number_of_items());
            } else if (bin_type.rect.x > bin_type.rect.y) {
                BinTypeId modified_bin_type_id = modified_instance_builder.add_bin_type(
                        bin_type.rect.x,
                        bin_type.rect.x);
                modified_instance_builder.set_bin_type_copies(
                        modified_bin_type_id,
                        2 * instance.number_of_items() + clautiaux_bound);
                if (clautiaux_bound > 0) {
                    ItemTypeId modified_item_type_id = modified_instance_builder.add_item_type(
                            bin_type.rect.x,
                            bin_type.rect.x - bin_type.rect.y);
                    modified_instance_builder.set_item_type_copies(
                            modified_item_type_id,
                            clautiaux_bound);
                }
            } else if (bin_type.rect.x < bin_type.rect.y) {
                BinTypeId modified_bin_type_id = modified_instance_builder.add_bin_type(
                        bin_type.rect.y,
                        bin_type.rect.y);
                modified_instance_builder.set_bin_type_copies(
                        modified_bin_type_id,
                        2 * instance.number_of_items() + clautiaux_bound);
                if (clautiaux_bound > 0) {
                    ItemTypeId modified_item_type_id = modified_instance_builder.add_item_type(
                            bin_type.rect.y - bin_type.rect.x,
                            bin_type.rect.y);
                    modified_instance_builder.set_item_type_copies(
                            modified_item_type_id,
                            clautiaux_bound);
                }
            }
            // Add items.
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.rect.x == item_type.rect.y) {
                    ItemTypeId modified_item_type_id = modified_instance_builder.add_item_type(
                            item_type.rect.x,
                            item_type.rect.y,
                            true);
                    modified_instance_builder.set_item_type_profit(
                            modified_item_type_id,
                            item_type.profit);
                    modified_instance_builder.set_item_type_copies(
                            modified_item_type_id,
                            2 * item_type.copies);
                } else {
                    ItemTypeId modified_item_type_id_1 = modified_instance_builder.add_item_type(
                            item_type.rect.x,
                            item_type.rect.y,
                            true);
                    modified_instance_builder.set_item_type_profit(
                            modified_item_type_id_1,
                            item_type.profit);
                    modified_instance_builder.set_item_type_copies(
                            modified_item_type_id_1,
                            item_type.copies);
                    ItemTypeId modified_item_type_id_2 = modified_instance_builder.add_item_type(
                            item_type.rect.y,
                            item_type.rect.x,
                            true);
                    modified_instance_builder.set_item_type_profit(
                            modified_item_type_id_2,
                            item_type.profit);
                    modified_instance_builder.set_item_type_copies(
                            modified_item_type_id_2,
                            item_type.copies);
                }
            }
            Instance modified_instance = modified_instance_builder.build();

            // Compute the bound on the modified instance.
            DualFeasibleFunctionsParameters modified_parameters;
            modified_parameters.verbosity_level = 0;
            auto modified_output = dual_feasible_functions(
                    modified_instance,
                    modified_parameters);

            // Retrieve the bound of the original instance.
            BinPos bound_cur = (modified_output.bin_packing_bound - 1) / 2 + 1;
            if (clautiaux_bound >= bound_cur)
                break;
            clautiaux_bound = bound_cur;

            if (bin_type.rect.x == bin_type.rect.y)
                break;
        }
    }

    DualFeasibleFunctionsTables tables = compute_dual_feasible_functions_tables(instance, bin_type);
    const std::vector<Length>& widths = tables.widths;
    const std::vector<Length>& heights = tables.heights;
    const std::vector<ItemPos>& full_cardinality_w = tables.full_cardinality_w;
    const std::vector<std::map<Length, ItemPos>>& excluded_cardinality_w = tables.excluded_cardinality_w;
    const std::vector<ItemPos>& full_cardinality_h = tables.full_cardinality_h;
    const std::vector<std::map<Length, ItemPos>>& excluded_cardinality_h = tables.excluded_cardinality_h;

    BinPos bound = 0;
    Profit knapsack_bound = std::numeric_limits<Profit>::infinity();

    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)widths.size(); ++k_pos) {
        Length k = widths[k_pos];

        for (ItemTypeId l_pos = 0; l_pos < (ItemTypeId)heights.size(); ++l_pos) {
            Length l = heights[l_pos];

            std::array<Length, 3> f_w_bin = {
                f_ccm_0(bin_type.rect.x, k, bin_type.rect.x),
                f_ccm_1(bin_type.rect.x, k, bin_type.rect.x, full_cardinality_w[k_pos]),
                f_ccm_2(bin_type.rect.x, k, bin_type.rect.x)};
            std::array<Length, 3> f_h_bin = {
                f_ccm_0(bin_type.rect.y, l, bin_type.rect.y),
                f_ccm_1(bin_type.rect.y, l, bin_type.rect.y, full_cardinality_h[l_pos]),
                f_ccm_2(bin_type.rect.y, l, bin_type.rect.y)};

            std::array<std::array<Length, 3>, 3> sums{};
            std::array<std::array<std::vector<Length>, 3>, 3> volumes;
            if (instance.objective() == Objective::Knapsack) {
                for (auto& row: volumes)
                    for (auto& v: row)
                        v.resize(instance.number_of_item_types());
            }

            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                for (int family_w = 0; family_w < 3; ++family_w) {
                    for (int family_h = 0; family_h < 3; ++family_h) {
                        Length c = item_coefficient(
                                item_type,
                                family_w,
                                family_h,
                                k,
                                l,
                                bin_type.rect.x,
                                bin_type.rect.y,
                                full_cardinality_w[k_pos],
                                excluded_cardinality_w[k_pos],
                                full_cardinality_h[l_pos],
                                excluded_cardinality_h[l_pos]);
                        if (instance.objective() == Objective::Knapsack) {
                            volumes[family_w][family_h][item_type_id] = c;
                        } else {
                            sums[family_w][family_h] += item_type.copies * c;
                        }
                    }
                }
            }

            if (instance.objective() == Objective::Knapsack) {
                for (int family_w = 0; family_w < 3; ++family_w) {
                    for (int family_h = 0; family_h < 3; ++family_h) {
                        Length capacity_single = f_w_bin[family_w] * f_h_bin[family_h];
                        if (capacity_single <= 0)
                            continue;
                        double capacity = (double)capacity_single * bin_type.copies;
                        Profit bound_combo = dantzig_profit_bound(
                                instance,
                                volumes[family_w][family_h],
                                capacity);
                        knapsack_bound = (std::min)(knapsack_bound, bound_combo);
                    }
                }
            } else {
                for (int family_w = 0; family_w < 3; ++family_w) {
                    for (int family_h = 0; family_h < 3; ++family_h) {
                        BinPos bound_combo = std::ceil(
                                (double)sums[family_w][family_h]
                                / (f_w_bin[family_w] * f_h_bin[family_h]));
                        bound = (std::max)(bound, bound_combo);
                    }
                }
            }
        }
    }

    if (instance.objective() == Objective::BinPacking) {
        bound = (std::max)(bound, clautiaux_bound);
        algorithm_formatter.update_bin_packing_bound(bound);
    } else if (instance.objective() == Objective::Feasibility) {
        bound = (std::max)(bound, clautiaux_bound);
        if (bound > instance.number_of_bins())
            algorithm_formatter.update_is_proven_infeasible();
    } else if (instance.objective() == Objective::Knapsack) {
        // This bound ignores resources entirely. A 'penalize' resource with
        // a negative penalty *increases* the reported profit when
        // triggered (see 'Resource'), so add back the worst case - every
        // such resource triggering at once - to keep the bound valid.
        knapsack_bound += negative_penalty_sum(instance);
        algorithm_formatter.update_knapsack_bound(knapsack_bound);
    }

    algorithm_formatter.end();
    return output;
}

DualFeasibleFunctionsCut packingsolver::rectangle::find_most_violated_dual_feasible_function_cut(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);

    DualFeasibleFunctionsTables tables = compute_dual_feasible_functions_tables(instance, bin_type);

    DualFeasibleFunctionsCut best;

    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)tables.widths.size(); ++k_pos) {
        Length k = tables.widths[k_pos];

        for (ItemTypeId l_pos = 0; l_pos < (ItemTypeId)tables.heights.size(); ++l_pos) {
            Length l = tables.heights[l_pos];

            std::array<Length, 3> f_w_bin = {
                f_ccm_0(bin_type.rect.x, k, bin_type.rect.x),
                f_ccm_1(bin_type.rect.x, k, bin_type.rect.x, tables.full_cardinality_w[k_pos]),
                f_ccm_2(bin_type.rect.x, k, bin_type.rect.x)};
            std::array<Length, 3> f_h_bin = {
                f_ccm_0(bin_type.rect.y, l, bin_type.rect.y),
                f_ccm_1(bin_type.rect.y, l, bin_type.rect.y, tables.full_cardinality_h[l_pos]),
                f_ccm_2(bin_type.rect.y, l, bin_type.rect.y)};

            for (int family_w = 0; family_w < 3; ++family_w) {
                for (int family_h = 0; family_h < 3; ++family_h) {
                    Length bin_coefficient = f_w_bin[family_w] * f_h_bin[family_h];
                    if (bin_coefficient <= 0)
                        continue;

                    double sum = 0.0;
                    for (const auto& p: selected_items) {
                        const ItemType& item_type = instance.item_type(p.first);
                        ItemPos copies = p.second;
                        sum += copies * item_coefficient(
                                item_type,
                                family_w,
                                family_h,
                                k,
                                l,
                                bin_type.rect.x,
                                bin_type.rect.y,
                                tables.full_cardinality_w[k_pos],
                                tables.excluded_cardinality_w[k_pos],
                                tables.full_cardinality_h[l_pos],
                                tables.excluded_cardinality_h[l_pos]);
                    }

                    double violation = sum - bin_coefficient;
                    if (violation > best.violation) {
                        best.found = true;
                        best.violation = violation;
                        best.bound = bin_coefficient;
                        best.coefficients.assign(instance.number_of_item_types(), 0.0);
                        for (ItemTypeId item_type_id = 0;
                                item_type_id < instance.number_of_item_types();
                                ++item_type_id) {
                            best.coefficients[item_type_id] = item_coefficient(
                                    instance.item_type(item_type_id),
                                    family_w,
                                    family_h,
                                    k,
                                    l,
                                    bin_type.rect.x,
                                    bin_type.rect.y,
                                    tables.full_cardinality_w[k_pos],
                                    tables.excluded_cardinality_w[k_pos],
                                    tables.full_cardinality_h[l_pos],
                                    tables.excluded_cardinality_h[l_pos]);
                        }
                    }
                }
            }
        }
    }

    return best;
}

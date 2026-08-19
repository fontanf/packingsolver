#include "onedimensional/dual_feasible_functions.hpp"

#include "packingsolver/onedimensional/algorithm_formatter.hpp"

#include <array>
#include <map>

using namespace packingsolver;
using namespace packingsolver::onedimensional;

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
 * Item's reduced length: an item packed right after another copy of the
 * same type only needs 'length - nesting_length' of room. Using this
 * reduced value keeps every bound below a valid necessary condition
 * (sum of true per-item consumptions in any feasible packing is always
 * >= sum of reduced lengths), matching the convention already used by
 * 'onedimensional::optimize''s own length-based bin packing/knapsack
 * bounds.
 */
Length reduced_length(const ItemType& item_type)
{
    return item_type.length - (std::max)(item_type.nesting_length, (Length)0);
}

/**
 * Coefficient of an item of length 'length' for breakpoint 'k' and DFF
 * family ('family' in {0: f_ccm_0, 1: f_ccm_1, 2: f_ccm_2}).
 */
Length item_coefficient(
        Length length,
        int family,
        Length k,
        Length capacity,
        ItemPos full_cardinality,
        const std::map<Length, ItemPos>& excluded_cardinality)
{
    switch (family) {
    case 0: return f_ccm_0(capacity, k, length);
    case 1: {
        if (length > capacity / 2) {
            ItemPos excluded = excluded_cardinality.at(length);
            return f_ccm_1(capacity, k, length, full_cardinality - excluded);
        }
        return f_ccm_1(capacity, k, length, 0);
    } default: return f_ccm_2(capacity, k, length);
    }
}

/**
 * Precomputed tables shared by every (k, family) combo of the breakpoint
 * sweep: candidate breakpoints, and the "maximum cardinality" bookkeeping
 * f_ccm_1 needs. These only depend on 'instance' and 'bin_type', so they
 * are computed once and reused across the whole sweep.
 */
struct DualFeasibleFunctionsTables
{
    std::vector<Length> breakpoints;
    std::vector<ItemPos> full_cardinality;
    std::vector<std::map<Length, ItemPos>> excluded_cardinality;
};

DualFeasibleFunctionsTables compute_dual_feasible_functions_tables(
        const Instance& instance,
        const BinType& bin_type)
{
    DualFeasibleFunctionsTables tables;

    // Compute all distinct breakpoint candidates.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Length length = reduced_length(item_type);
        // An item longer than the bin can never be validly placed at all;
        // folding it via 'capacity - length' would produce a negative
        // breakpoint, pushing k outside the CCM functions' valid domain of
        // [1, capacity / 2] and corrupting the bound.
        if (length > bin_type.length)
            continue;
        if (length == bin_type.length) {
        } else if (length <= bin_type.length / 2) {
            tables.breakpoints.push_back(length);
        } else {
            tables.breakpoints.push_back(bin_type.length - length);
        }
    }
    // capacity / 2 is where f_ccm_0/f_ccm_2 themselves switch branch: it is
    // a meaningful breakpoint on its own, regardless of whether any item
    // length happens to fold onto it.
    // A breakpoint of 0 is never valid (f_ccm_0/f_ccm_1/f_ccm_2 all divide
    // by k), which capacity / 2 degenerates to when capacity is 1.
    if (bin_type.length / 2 > 0)
        tables.breakpoints.push_back(bin_type.length / 2);
    sort(tables.breakpoints.begin(), tables.breakpoints.end());
    tables.breakpoints.erase(
            unique(tables.breakpoints.begin(), tables.breakpoints.end()),
            tables.breakpoints.end());

    // Distinct "big" (> half the bin's capacity) length values that some
    // item type might present - the only values f_ccm_1's item_coefficient
    // will ever need an excluded-cardinality entry for.
    std::vector<Length> big_values;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Length length = reduced_length(item_type);
        if (length > bin_type.length / 2)
            big_values.push_back(length);
    }
    sort(big_values.begin(), big_values.end());
    big_values.erase(unique(big_values.begin(), big_values.end()), big_values.end());

    // Compute maximum cardinalities.
    tables.full_cardinality.resize(tables.breakpoints.size());
    tables.excluded_cardinality.resize(tables.breakpoints.size());
    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)tables.breakpoints.size(); ++k_pos) {
        Length k = tables.breakpoints[k_pos];
        std::vector<std::pair<Length, ItemPos>> pool;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            Length length = reduced_length(item_type);
            if (k <= length && length <= bin_type.length / 2)
                pool.push_back({length, item_type.copies});
        }
        sort(pool.begin(), pool.end());
        tables.full_cardinality[k_pos] = greedy_maximum_cardinality(bin_type.length, pool);
        for (Length big_value: big_values) {
            tables.excluded_cardinality[k_pos][big_value] = greedy_maximum_cardinality(
                    bin_type.length - big_value,
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
        // An item that doesn't fit any bin can never be packed, regardless
        // of what its (possibly zero, after DFF rounding) scaled volume
        // suggests.
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

DualFeasibleFunctionsOutput packingsolver::onedimensional::dual_feasible_functions(
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

    DualFeasibleFunctionsTables tables = compute_dual_feasible_functions_tables(instance, bin_type);
    const std::vector<Length>& breakpoints = tables.breakpoints;
    const std::vector<ItemPos>& full_cardinality = tables.full_cardinality;
    const std::vector<std::map<Length, ItemPos>>& excluded_cardinality = tables.excluded_cardinality;

    BinPos bound = 0;
    Profit knapsack_bound = std::numeric_limits<Profit>::infinity();

    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)breakpoints.size(); ++k_pos) {
        Length k = breakpoints[k_pos];

        std::array<Length, 3> f_bin = {
            f_ccm_0(bin_type.length, k, bin_type.length),
            f_ccm_1(bin_type.length, k, bin_type.length, full_cardinality[k_pos]),
            f_ccm_2(bin_type.length, k, bin_type.length)};

        std::array<Length, 3> sums{};
        std::array<std::vector<Length>, 3> volumes;
        if (instance.objective() == Objective::Knapsack) {
            for (auto& v: volumes)
                v.resize(instance.number_of_item_types());
        }

        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            Length length = reduced_length(item_type);
            for (int family = 0; family < 3; ++family) {
                Length c = item_coefficient(
                        length,
                        family,
                        k,
                        bin_type.length,
                        full_cardinality[k_pos],
                        excluded_cardinality[k_pos]);
                if (instance.objective() == Objective::Knapsack) {
                    volumes[family][item_type_id] = c;
                } else {
                    sums[family] += item_type.copies * c;
                }
            }
        }

        if (instance.objective() == Objective::Knapsack) {
            for (int family = 0; family < 3; ++family) {
                if (f_bin[family] <= 0)
                    continue;
                double capacity = (double)f_bin[family] * bin_type.copies;
                Profit bound_combo = dantzig_profit_bound(
                        instance,
                        volumes[family],
                        capacity);
                knapsack_bound = (std::min)(knapsack_bound, bound_combo);
            }
        } else {
            for (int family = 0; family < 3; ++family) {
                if (f_bin[family] <= 0)
                    continue;
                BinPos bound_combo = std::ceil((double)sums[family] / f_bin[family]);
                bound = (std::max)(bound, bound_combo);
            }
        }
    }

    if (instance.objective() == Objective::BinPacking) {
        algorithm_formatter.update_bin_packing_bound(bound);
    } else if (instance.objective() == Objective::Feasibility) {
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

#include "box/dual_feasible_functions.hpp"

#include "packingsolver/box/algorithm_formatter.hpp"

#include <array>
#include <limits>
#include <map>

using namespace packingsolver;
using namespace packingsolver::box;

namespace
{

/** Number of families of dual feasible functions used per axis. */
const int number_of_families = 3;

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
        const std::vector<Volume>& volumes,
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

Length axis_component(const Box& box, int axis_id)
{
    return (axis_id == 0) ? box.x: (axis_id == 1) ? box.y: box.z;
}

/**
 * Distinct values 'item_type' could present along 'axis_id', across all of
 * its allowed rotations.
 */
std::vector<Length> possible_axis_values(
        const ItemType& item_type,
        int axis_id)
{
    std::vector<Length> values;
    for (Rotation rotation: item_type.rotations)
        values.push_back(axis_component(item_type.box.rotate(rotation), axis_id));
    sort(values.begin(), values.end());
    values.erase(unique(values.begin(), values.end()), values.end());
    return values;
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
 * Smallest value 'item_type' could present along 'axis_id' that still
 * qualifies as "medium" ('k' <= value <= 'half_capacity'), across all its
 * allowed rotations - or -1 if none qualify.
 *
 * Used only to upper-bound how many medium items can be packed alongside a
 * big item (f_ccm_1's "MC(C, S) - MC(C - x, S)" term). Using the smallest
 * qualifying value (rather than requiring every rotation to qualify) can
 * only overestimate - never underestimate - the true achievable count,
 * keeping the result a valid upper bound regardless of which rotation the
 * item actually ends up using.
 */
Length medium_pool_length(
        const ItemType& item_type,
        int axis_id,
        Length k,
        Length half_capacity)
{
    Length best = -1;
    for (Length v: possible_axis_values(item_type, axis_id)) {
        if (k <= v && v <= half_capacity && (best < 0 || v < best))
            best = v;
    }
    return best;
}

/**
 * Precomputed tables for one axis: candidate breakpoints, and the "maximum
 * cardinality" bookkeeping f_ccm_1 needs. These only depend on 'instance'
 * and the bin's length on this axis, not on any item's chosen rotation.
 */
struct AxisTables
{
    std::vector<Length> thresholds;
    std::vector<ItemPos> full_cardinality;
    std::vector<std::map<Length, ItemPos>> excluded_cardinality;
};

AxisTables compute_axis_tables(
        const Instance& instance,
        Length bin_length,
        int axis_id)
{
    AxisTables tables;

    // Candidate breakpoints: every distinct rotation-achievable value on
    // this axis (folded), across all item types - both dimensions of a
    // rotatable item feed the same axis's breakpoint list, since it may
    // end up presenting any of them there.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (Length v: possible_axis_values(item_type, axis_id)) {
            // A dimension exceeding the bin's own capacity on this axis can
            // never be validly placed along it at all (this happens for an
            // item whose *other* dimension - the one feeding this axis's
            // table only because rotation is allowed - is bigger than the
            // bin itself here), so folding it via 'bin_length - v' would
            // produce a negative breakpoint, pushing it outside the CCM
            // functions' valid domain of [1, bin_length/2] and corrupting
            // the bound. Simply skip it: the item's other, valid dimension
            // still contributes its own breakpoint normally.
            if (v > bin_length)
                continue;
            if (v == bin_length) {
            } else if (v <= bin_length / 2) {
                tables.thresholds.push_back(v);
            } else {
                tables.thresholds.push_back(bin_length - v);
            }
        }
    }
    // capacity / 2 is where f_ccm_0/f_ccm_2 themselves switch branch: it is
    // a meaningful breakpoint on its own, regardless of whether any item
    // dimension happens to fold onto it (this matters most for rotation:
    // e.g. an item that is "big" in every allowed rotation, on every axis,
    // needs this breakpoint to be recognized as such if no item dimension
    // is exactly at half the bin's capacity).
    tables.thresholds.push_back(bin_length / 2);
    sort(tables.thresholds.begin(), tables.thresholds.end());
    tables.thresholds.erase(unique(tables.thresholds.begin(), tables.thresholds.end()), tables.thresholds.end());

    // Distinct "big" (> half the bin's length on this axis) values some
    // item type might present along this axis - these are the only values
    // f_ccm_1_axis will ever need an excluded-cardinality entry for.
    std::vector<Length> big_values;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (Length v: possible_axis_values(item_type, axis_id))
            // Same reasoning as above: a value exceeding the bin's own
            // capacity can never be validly placed, so it must not become a
            // "big value" either - 'bin_length - big_value' feeds directly
            // into greedy_maximum_cardinality() below as a capacity, and
            // going negative there would corrupt the excluded-cardinality
            // bookkeeping f_ccm_1 relies on.
            if (v > bin_length / 2 && v <= bin_length)
                big_values.push_back(v);
    }
    sort(big_values.begin(), big_values.end());
    big_values.erase(unique(big_values.begin(), big_values.end()), big_values.end());

    tables.full_cardinality.resize(tables.thresholds.size());
    tables.excluded_cardinality.resize(tables.thresholds.size());
    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)tables.thresholds.size(); ++k_pos) {
        Length k = tables.thresholds[k_pos];
        std::vector<std::pair<Length, ItemPos>> pool;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            Length length = medium_pool_length(item_type, axis_id, k, bin_length / 2);
            if (length >= 0)
                pool.push_back({length, item_type.copies});
        }
        sort(pool.begin(), pool.end());
        tables.full_cardinality[k_pos] = greedy_maximum_cardinality(bin_length, pool);
        for (Length big_value: big_values) {
            tables.excluded_cardinality[k_pos][big_value] = greedy_maximum_cardinality(
                    bin_length - big_value,
                    pool);
        }
    }

    return tables;
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
    if (length >= capacity) {
        // The item alone already spans the bin's full length on this axis
        // (this only happens for a rotation - possibly one that doesn't
        // even fit this axis at all, folded down to 'capacity' by the
        // caller): no room is left for anything else alongside it, so
        // nothing is excluded, exactly like the bin's own f_bin evaluation
        // (which uses this same 'value == full_cardinality' shortcut).
        // Bypassing the map also avoids depending on an excluded-cardinality
        // entry that may not exist for 'capacity' itself.
        return f_ccm_1(capacity, k, capacity, full_cardinality);
    }
    if (length > capacity / 2) {
        ItemPos excluded = excluded_cardinality.at(length);
        return f_ccm_1(capacity, k, length, full_cardinality - excluded);
    }
    return f_ccm_1(capacity, k, length, 0);
}

/**
 * Coefficient of 'item_type' for breakpoints 'k' (one per axis) and DFF
 * families 'families' (one per axis, in {0: f_ccm_0, 1: f_ccm_1, 2:
 * f_ccm_2}), taking the minimum over all of the item's allowed rotations.
 *
 * The item's true contribution depends on a rotation choice this
 * per-item-type coefficient scheme doesn't track; using the smallest
 * coefficient among its allowed rotations stays a valid (safe) lower bound
 * on the true contribution regardless of which rotation actually ends up
 * being used.
 */
Volume item_coefficient(
        const ItemType& item_type,
        const std::array<int, 3>& families,
        const std::array<Length, 3>& k,
        const std::array<Length, 3>& bin_lengths,
        const std::array<ItemPos, 3>& full_cardinality,
        const std::array<const std::map<Length, ItemPos>*, 3>& excluded_cardinality)
{
    auto eval_axis = [&](int axis_id, Length length) -> Length
    {
        Length capacity = bin_lengths[axis_id];
        // A rotation can present a dimension exceeding the bin's own
        // capacity on this axis (impossible to place, but still evaluated
        // here since the item's *other* dimension on this rotation might
        // still be fine on the other axes). Every dual feasible function
        // is non-decreasing and must saturate at its value for
        // length == capacity; without this clamp, f_ccm_2 in particular can
        // return a value exceeding 'capacity' itself for length > capacity,
        // corrupting the bound.
        length = (std::min)(length, capacity);
        switch (families[axis_id]) {
        case 0: return f_ccm_0(capacity, k[axis_id], length);
        case 1: return f_ccm_1_axis(capacity, k[axis_id], length, full_cardinality[axis_id], *excluded_cardinality[axis_id]);
        default: return f_ccm_2(capacity, k[axis_id], length);
        }
    };

    Volume best = -1;
    for (Rotation rotation: item_type.rotations) {
        Box effective_box = item_type.box.rotate(rotation);
        Volume value = 1;
        for (int axis_id = 0; axis_id < 3; ++axis_id)
            value *= eval_axis(axis_id, axis_component(effective_box, axis_id));
        if (best < 0 || value < best)
            best = value;
    }
    return best;
}

}

DualFeasibleFunctionsOutput packingsolver::box::dual_feasible_functions(
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

    std::array<Length, 3> bin_lengths = {
            bin_type.box.x,
            bin_type.box.y,
            bin_type.box.z};

    std::array<AxisTables, 3> tables;
    for (int axis_id = 0; axis_id < 3; ++axis_id)
        tables[axis_id] = compute_axis_tables(instance, bin_lengths[axis_id], axis_id);

    BinPos bound = 0;
    Profit knapsack_bound = std::numeric_limits<Profit>::infinity();

    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)tables[0].thresholds.size(); ++k_pos) {
        for (ItemTypeId l_pos = 0; l_pos < (ItemTypeId)tables[1].thresholds.size(); ++l_pos) {
            for (ItemTypeId m_pos = 0; m_pos < (ItemTypeId)tables[2].thresholds.size(); ++m_pos) {
                std::array<ItemTypeId, 3> pos = {k_pos, l_pos, m_pos};
                std::array<Length, 3> k;
                std::array<ItemPos, 3> full_cardinality;
                std::array<const std::map<Length, ItemPos>*, 3> excluded_cardinality;
                for (int axis_id = 0; axis_id < 3; ++axis_id) {
                    k[axis_id] = tables[axis_id].thresholds[pos[axis_id]];
                    full_cardinality[axis_id] = tables[axis_id].full_cardinality[pos[axis_id]];
                    excluded_cardinality[axis_id] = &tables[axis_id].excluded_cardinality[pos[axis_id]];
                }

                // Value of each of the 'number_of_families' families of
                // dual feasible functions applied to the bin's dimension,
                // for each axis.
                std::array<std::array<Length, number_of_families>, 3> f_bin;
                for (int axis_id = 0; axis_id < 3; ++axis_id) {
                    Length capacity = bin_lengths[axis_id];
                    f_bin[axis_id] = {
                            f_ccm_0(capacity, k[axis_id], capacity),
                            f_ccm_1(capacity, k[axis_id], capacity, full_cardinality[axis_id]),
                            f_ccm_2(capacity, k[axis_id], capacity)};
                }

                // Sum, for each of the 'number_of_families'^3 combinations
                // of families (one per axis), of the transformed volume of
                // the items. For the Knapsack objective, the per-item
                // scaled volumes are needed individually (to run a Dantzig
                // bound over the selection), rather than summed over all
                // items.
                std::array<std::array<std::array<Volume, number_of_families>, number_of_families>, number_of_families> sum;
                std::array<std::array<std::array<std::vector<Volume>, number_of_families>, number_of_families>, number_of_families> volumes;
                if (instance.objective() == Objective::Knapsack) {
                    for (auto& volumes_yz: volumes)
                        for (auto& volumes_z: volumes_yz)
                            for (auto& v: volumes_z)
                                v.resize(instance.number_of_item_types());
                } else {
                    for (auto& sum_yz: sum) {
                        for (auto& sum_z: sum_yz)
                            sum_z.fill(0);
                    }
                }

                for (ItemTypeId item_type_id = 0;
                        item_type_id < instance.number_of_item_types();
                        ++item_type_id) {
                    const ItemType& item_type = instance.item_type(item_type_id);

                    if (instance.objective() == Objective::Knapsack) {
                        for (int fx = 0; fx < number_of_families; ++fx) {
                            for (int fy = 0; fy < number_of_families; ++fy) {
                                for (int fz = 0; fz < number_of_families; ++fz) {
                                    volumes[fx][fy][fz][item_type_id] = item_coefficient(
                                            item_type,
                                            {fx, fy, fz},
                                            k,
                                            bin_lengths,
                                            full_cardinality,
                                            excluded_cardinality);
                                }
                            }
                        }
                    } else {
                        for (int fx = 0; fx < number_of_families; ++fx) {
                            for (int fy = 0; fy < number_of_families; ++fy) {
                                for (int fz = 0; fz < number_of_families; ++fz) {
                                    sum[fx][fy][fz] += item_type.copies * item_coefficient(
                                            item_type,
                                            {fx, fy, fz},
                                            k,
                                            bin_lengths,
                                            full_cardinality,
                                            excluded_cardinality);
                                }
                            }
                        }
                    }
                }

                if (instance.objective() == Objective::Knapsack) {
                    for (int fx = 0; fx < number_of_families; ++fx) {
                        for (int fy = 0; fy < number_of_families; ++fy) {
                            for (int fz = 0; fz < number_of_families; ++fz) {
                                Volume capacity_single = f_bin[0][fx] * f_bin[1][fy] * f_bin[2][fz];
                                if (capacity_single <= 0)
                                    continue;
                                double capacity = (double)capacity_single * bin_type.copies;
                                Profit bound_combo = dantzig_profit_bound(
                                        instance,
                                        volumes[fx][fy][fz],
                                        capacity);
                                knapsack_bound = (std::min)(knapsack_bound, bound_combo);
                            }
                        }
                    }
                } else {
                    for (int fx = 0; fx < number_of_families; ++fx) {
                        for (int fy = 0; fy < number_of_families; ++fy) {
                            for (int fz = 0; fz < number_of_families; ++fz) {
                                Volume denominator = f_bin[0][fx] * f_bin[1][fy] * f_bin[2][fz];
                                if (denominator <= 0)
                                    continue;
                                BinPos bound_cur = std::ceil((double)sum[fx][fy][fz] / (double)denominator);
                                bound = (std::max)(bound, bound_cur);
                            }
                        }
                    }
                }
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

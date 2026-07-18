#include "box/dual_feasible_functions.hpp"

#include "packingsolver/box/algorithm_formatter.hpp"

#include <array>
#include <functional>

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
 * Effective (rotated) dimensions of an item.
 *
 * Only valid for oriented items (a single allowed rotation), which is the
 * only case handled by this bound.
 */
Box item_effective_box(const ItemType& item_type)
{
    return item_type.box.rotate(item_type.rotations.front());
}

/**
 * Compute the distinct candidate threshold values for one axis: for each
 * item, if its dimension in this axis equals the bin's, it is ignored;
 * otherwise it (or its 'folded' complement, if it is more than half the
 * bin's dimension) is added as a candidate threshold.
 */
std::vector<Length> compute_thresholds(
        const Instance& instance,
        Length bin_length,
        const std::function<Length(const Box&)>& axis)
{
    std::vector<Length> values;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        Length item_length = axis(item_effective_box(instance.item_type(item_type_id)));
        if (item_length == bin_length) {
        } else if (item_length <= bin_length / 2) {
            values.push_back(item_length);
        } else {
            values.push_back(bin_length - item_length);
        }
    }
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

/**
 * Compute, for each threshold value and each item type id ('number_of_item_types()'
 * meaning 'no excluded item type'), the maximum number of copies of 'small'
 * items (dimension <= bin_length / 2 in this axis, and >= the threshold)
 * that fit in the bin (minus the excluded item's dimension, if any) along
 * this axis.
 */
std::vector<std::vector<ItemPos>> compute_maximum_cardinalities(
        const Instance& instance,
        Length bin_length,
        const std::vector<Length>& thresholds,
        const std::function<Length(const Box&)>& axis)
{
    std::vector<std::vector<ItemPos>> maximum_cardinality(thresholds.size());
    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)thresholds.size(); ++k_pos) {
        Length k = thresholds[k_pos];
        std::vector<ItemTypeId> sorted_item_type_ids;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            Length item_length = axis(item_effective_box(instance.item_type(item_type_id)));
            if (k <= item_length && item_length <= bin_length / 2)
                sorted_item_type_ids.push_back(item_type_id);
        }
        std::sort(
                sorted_item_type_ids.begin(),
                sorted_item_type_ids.end(),
                [&instance, &axis](
                    ItemTypeId item_type_id_1,
                    ItemTypeId item_type_id_2)
                {
                    return axis(item_effective_box(instance.item_type(item_type_id_1)))
                        < axis(item_effective_box(instance.item_type(item_type_id_2)));
                });
        maximum_cardinality[k_pos] = std::vector<ItemPos>(instance.number_of_item_types() + 1, -10000);
        for (ItemTypeId item_type_id = 0;
                item_type_id <= instance.number_of_item_types();
                ++item_type_id) {
            Length capacity = bin_length;
            if (item_type_id < instance.number_of_item_types()) {
                Length item_length = axis(item_effective_box(instance.item_type(item_type_id)));
                if (item_length <= bin_length / 2)
                    continue;
                capacity -= item_length;
            }
            Length length_cur = 0;
            maximum_cardinality[k_pos][item_type_id] = 0;
            for (ItemTypeId item_type_id_cur: sorted_item_type_ids) {
                const ItemType& item_type_cur = instance.item_type(item_type_id_cur);
                Length item_length_cur = axis(item_effective_box(item_type_cur));
                if (length_cur + item_type_cur.copies * item_length_cur < capacity) {
                    length_cur += item_type_cur.copies * item_length_cur;
                    maximum_cardinality[k_pos][item_type_id] += item_type_cur.copies;
                } else {
                    ItemPos copies = (capacity - length_cur) / item_length_cur;
                    maximum_cardinality[k_pos][item_type_id] += copies;
                    break;
                }
            }
        }
    }
    return maximum_cardinality;
}

}

DualFeasibleFunctionsOutput packingsolver::box::dual_feasible_functions(
        const Instance& instance,
        const DualFeasibleFunctionsParameters& parameters)
{
    const BinType& bin_type = instance.bin_type(0);

    if (instance.objective() != Objective::BinPacking) {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }
    if (instance.number_of_bin_types() != 1) {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }

    DualFeasibleFunctionsOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // This bound is only implemented for fully oriented items, i.e. items
    // with a single allowed rotation; skip it otherwise.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (instance.item_type(item_type_id).rotations.size() != 1) {
            algorithm_formatter.end();
            return output;
        }
    }

    std::array<std::function<Length(const Box&)>, 3> axes = {
            std::function<Length(const Box&)>([](const Box& box) { return box.x; }),
            std::function<Length(const Box&)>([](const Box& box) { return box.y; }),
            std::function<Length(const Box&)>([](const Box& box) { return box.z; })};
    std::array<Length, 3> bin_lengths = {
            bin_type.box.x,
            bin_type.box.y,
            bin_type.box.z};

    std::array<std::vector<Length>, 3> thresholds;
    std::array<std::vector<std::vector<ItemPos>>, 3> maximum_cardinalities;
    for (int axis_id = 0; axis_id < 3; ++axis_id) {
        thresholds[axis_id] = compute_thresholds(instance, bin_lengths[axis_id], axes[axis_id]);
        maximum_cardinalities[axis_id] = compute_maximum_cardinalities(
                instance,
                bin_lengths[axis_id],
                thresholds[axis_id],
                axes[axis_id]);
    }

    BinPos bound = 0;

    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)thresholds[0].size(); ++k_pos) {
        for (ItemTypeId l_pos = 0; l_pos < (ItemTypeId)thresholds[1].size(); ++l_pos) {
            for (ItemTypeId m_pos = 0; m_pos < (ItemTypeId)thresholds[2].size(); ++m_pos) {
                std::array<ItemTypeId, 3> pos = {k_pos, l_pos, m_pos};
                std::array<Length, 3> k = {
                        thresholds[0][k_pos],
                        thresholds[1][l_pos],
                        thresholds[2][m_pos]};

                // Value of each of the 'number_of_families' families of
                // dual feasible functions applied to the bin's dimension,
                // for each axis.
                std::array<std::array<Length, number_of_families>, 3> f_bin;
                for (int axis_id = 0; axis_id < 3; ++axis_id) {
                    Length capacity = bin_lengths[axis_id];
                    ItemPos value = maximum_cardinalities[axis_id][pos[axis_id]][instance.number_of_item_types()];
                    f_bin[axis_id] = {
                            f_ccm_0(capacity, k[axis_id], capacity),
                            f_ccm_1(capacity, k[axis_id], capacity, value),
                            f_ccm_2(capacity, k[axis_id], capacity)};
                }

                // Sum, for each of the 'number_of_families'^3 combinations
                // of families (one per axis), of the transformed volume of
                // the items.
                std::array<std::array<std::array<Volume, number_of_families>, number_of_families>, number_of_families> sum;
                for (auto& sum_yz: sum) {
                    for (auto& sum_z: sum_yz)
                        sum_z.fill(0);
                }

                for (ItemTypeId item_type_id = 0;
                        item_type_id < instance.number_of_item_types();
                        ++item_type_id) {
                    const ItemType& item_type = instance.item_type(item_type_id);
                    Box effective_box = item_effective_box(item_type);
                    std::array<Length, 3> item_lengths = {
                            effective_box.x,
                            effective_box.y,
                            effective_box.z};

                    std::array<std::array<Length, number_of_families>, 3> f_item;
                    for (int axis_id = 0; axis_id < 3; ++axis_id) {
                        Length capacity = bin_lengths[axis_id];
                        ItemPos value
                            = maximum_cardinalities[axis_id][pos[axis_id]][instance.number_of_item_types()]
                            - maximum_cardinalities[axis_id][pos[axis_id]][item_type_id];
                        f_item[axis_id] = {
                                f_ccm_0(capacity, k[axis_id], item_lengths[axis_id]),
                                f_ccm_1(capacity, k[axis_id], item_lengths[axis_id], value),
                                f_ccm_2(capacity, k[axis_id], item_lengths[axis_id])};
                    }

                    for (int fx = 0; fx < number_of_families; ++fx) {
                        for (int fy = 0; fy < number_of_families; ++fy) {
                            for (int fz = 0; fz < number_of_families; ++fz) {
                                sum[fx][fy][fz] += item_type.copies
                                    * f_item[0][fx] * f_item[1][fy] * f_item[2][fz];
                            }
                        }
                    }
                }

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

    algorithm_formatter.update_bin_packing_bound(bound);

    algorithm_formatter.end();
    return output;
}

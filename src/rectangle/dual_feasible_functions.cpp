#include "rectangle/dual_feasible_functions.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"

#include <array>

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

    // These bounds are only valid if all items are oriented.
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
    if (!all_items_oriented) {
        // The recursive doubling strategy below only produces a bin count
        // bound, which does not translate into a knapsack profit bound
        // (profit does not simply halve when an item is duplicated once
        // per orientation), so skip it entirely for the Knapsack objective.
        if (instance.objective() == Objective::Knapsack) {
            algorithm_formatter.end();
            return output;
        }

        // If there are some non-oriented items, we use the strategy from
        // clautiaux2007:
        // - Build a modified instance containing for each item of the original
        //   instance, one item for each orientation.
        // - Compute the bound on the modified instance.
        // - Divide it by 2 to get the bound of the original instance.

        BinPos bound = 0;
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
                        2 * instance.number_of_items() + bound);
                if (bound > 0) {
                    ItemTypeId modified_item_type_id = modified_instance_builder.add_item_type(
                            bin_type.rect.x,
                            bin_type.rect.x - bin_type.rect.y);
                    modified_instance_builder.set_item_type_copies(
                            modified_item_type_id,
                            bound);
                }
            } else if (bin_type.rect.x < bin_type.rect.y) {
                BinTypeId modified_bin_type_id = modified_instance_builder.add_bin_type(
                        bin_type.rect.y,
                        bin_type.rect.y);
                modified_instance_builder.set_bin_type_copies(
                        modified_bin_type_id,
                        2 * instance.number_of_items() + bound);
                if (bound > 0) {
                    ItemTypeId modified_item_type_id = modified_instance_builder.add_item_type(
                            bin_type.rect.y - bin_type.rect.x,
                            bin_type.rect.y);
                    modified_instance_builder.set_item_type_copies(
                            modified_item_type_id,
                            bound);
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
            if (bound >= bound_cur)
                break;
            bound = bound_cur;
            if (instance.objective() == Objective::BinPacking) {
                algorithm_formatter.update_bin_packing_bound(bound);
            } else if (instance.objective() == Objective::Feasibility) {
                if (bound > instance.number_of_bins())
                    algorithm_formatter.update_is_proven_infeasible();
            }

            if (bin_type.rect.x == bin_type.rect.y)
                break;
        }
        algorithm_formatter.end();
        return output;
    }

    // Compute all distinct widths and heights.
    std::vector<Length> widths;
    std::vector<Length> heights;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.rect.x == bin_type.rect.x) {
        } else if (item_type.rect.x <= bin_type.rect.x / 2) {
            widths.push_back(item_type.rect.x);
        } else {
            widths.push_back(bin_type.rect.x - item_type.rect.x);
        }
        if (item_type.rect.y == bin_type.rect.y) {
        } else if (item_type.rect.y <= bin_type.rect.y / 2) {
            heights.push_back(item_type.rect.y);
        } else {
            heights.push_back(bin_type.rect.y - item_type.rect.y);
        }
    }
    sort(widths.begin(), widths.end());
    sort(heights.begin(), heights.end());
    widths.erase(unique(widths.begin(), widths.end()), widths.end());
    heights.erase(unique(heights.begin(), heights.end()), heights.end());

    // Compute maximum cardinalities.
    std::vector<std::vector<ItemPos>> maximum_cardinality_w(widths.size());
    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)widths.size(); ++k_pos) {
        Length k = widths[k_pos];
        std::vector<ItemTypeId> sorted_item_type_ids_w;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            if (k <= item_type.rect.x && item_type.rect.x <= bin_type.rect.x / 2)
                sorted_item_type_ids_w.push_back(item_type_id);
        }
        std::sort(
                sorted_item_type_ids_w.begin(),
                sorted_item_type_ids_w.end(),
                [&instance](
                    ItemTypeId item_type_id_1,
                    ItemTypeId item_type_id_2)
                {
                const ItemType& item_type_1 = instance.item_type(item_type_id_1);
                const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                return item_type_1.rect.x < item_type_2.rect.x;
                });
        maximum_cardinality_w[k_pos] = std::vector<ItemPos>(instance.number_of_item_types() + 1, -10000);
        for (ItemTypeId item_type_id = 0;
                item_type_id <= instance.number_of_item_types();
                ++item_type_id) {
            Length capacity_w = bin_type.rect.x;
            if (item_type_id < instance.number_of_item_types()) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.rect.x <= bin_type.rect.x / 2)
                    continue;
                capacity_w -= item_type.rect.x;
            }
            Length width_cur = 0;
            maximum_cardinality_w[k_pos][item_type_id] = 0;
            for (ItemTypeId item_type_id_cur: sorted_item_type_ids_w) {
                const ItemType& item_type_cur = instance.item_type(item_type_id_cur);
                if (width_cur + item_type_cur.copies * item_type_cur.rect.x < capacity_w) {
                    width_cur += item_type_cur.copies * item_type_cur.rect.x;
                    maximum_cardinality_w[k_pos][item_type_id] += item_type_cur.copies;
                } else {
                    ItemPos copies = (capacity_w - width_cur) / item_type_cur.rect.x;
                    maximum_cardinality_w[k_pos][item_type_id] += copies;
                    break;
                }
            }
        }
    }
    std::vector<std::vector<ItemPos>> maximum_cardinality_h(heights.size());
    for (ItemTypeId l_pos = 0; l_pos < (ItemTypeId)heights.size(); ++l_pos) {
        Length l = heights[l_pos];
        std::vector<ItemTypeId> sorted_item_type_ids_h;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            if (l <= item_type.rect.y && item_type.rect.y <= bin_type.rect.y / 2)
                sorted_item_type_ids_h.push_back(item_type_id);
        }
        std::sort(
                sorted_item_type_ids_h.begin(),
                sorted_item_type_ids_h.end(),
                [&instance](
                    ItemTypeId item_type_id_1,
                    ItemTypeId item_type_id_2)
                {
                const ItemType& item_type_1 = instance.item_type(item_type_id_1);
                const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                return item_type_1.rect.y < item_type_2.rect.y;
                });
        maximum_cardinality_h[l_pos] = std::vector<ItemPos>(instance.number_of_item_types() + 1, -10000);
        for (ItemTypeId item_type_id = 0;
                item_type_id <= instance.number_of_item_types();
                ++item_type_id) {
            Length capacity_h = bin_type.rect.y;
            if (item_type_id < instance.number_of_item_types()) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.rect.y <= bin_type.rect.y / 2)
                    continue;
                capacity_h -= item_type.rect.y;
            }
            Length height_cur = 0;
            maximum_cardinality_h[l_pos][item_type_id] = 0;
            for (ItemTypeId item_type_id_cur: sorted_item_type_ids_h) {
                const ItemType& item_type_cur = instance.item_type(item_type_id_cur);
                if (height_cur + item_type_cur.copies * item_type_cur.rect.y < capacity_h) {
                    height_cur += item_type_cur.copies * item_type_cur.rect.y;
                    maximum_cardinality_h[l_pos][item_type_id] += item_type_cur.copies;
                } else {
                    ItemPos copies = (capacity_h - height_cur) / item_type_cur.rect.y;
                    maximum_cardinality_h[l_pos][item_type_id] += copies;
                    break;
                }
            }
        }
    }

    BinPos bound = 0;
    Profit knapsack_bound = std::numeric_limits<Profit>::infinity();

    for (ItemTypeId k_pos = 0; k_pos < (ItemTypeId)widths.size(); ++k_pos) {
        Length k = widths[k_pos];

        for (ItemTypeId l_pos = 0; l_pos < (ItemTypeId)heights.size(); ++l_pos) {
            Length l = heights[l_pos];

            Length f_ccm_0_w_bin = f_ccm_0(bin_type.rect.x, k, bin_type.rect.x);
            Length f_ccm_0_h_bin = f_ccm_0(bin_type.rect.y, l, bin_type.rect.y);
            Length f_ccm_1_w_bin = f_ccm_1(bin_type.rect.x, k, bin_type.rect.x, maximum_cardinality_w[k_pos][instance.number_of_item_types()]);
            Length f_ccm_1_h_bin = f_ccm_1(bin_type.rect.y, l, bin_type.rect.y, maximum_cardinality_h[l_pos][instance.number_of_item_types()]);
            Length f_ccm_2_w_bin = f_ccm_2(bin_type.rect.x, k, bin_type.rect.x);
            Length f_ccm_2_h_bin = f_ccm_2(bin_type.rect.y, l, bin_type.rect.y);

            Length f_ccm_0_w_0_h_sum = 0;
            Length f_ccm_0_w_1_h_sum = 0;
            Length f_ccm_0_w_2_h_sum = 0;
            Length f_ccm_1_w_0_h_sum = 0;
            Length f_ccm_1_w_1_h_sum = 0;
            Length f_ccm_1_w_2_h_sum = 0;
            Length f_ccm_2_w_0_h_sum = 0;
            Length f_ccm_2_w_1_h_sum = 0;
            Length f_ccm_2_w_2_h_sum = 0;
            // For the Knapsack objective, the per-item scaled volumes are
            // needed individually (to run a Dantzig bound over the
            // selection), rather than summed over all items.
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

                Length f_ccm_0_w = f_ccm_0(bin_type.rect.x, k, item_type.rect.x);
                Length f_ccm_0_h = f_ccm_0(bin_type.rect.y, l, item_type.rect.y);
                Length f_ccm_1_w = f_ccm_1(bin_type.rect.x, k, item_type.rect.x, maximum_cardinality_w[k_pos][instance.number_of_item_types()] - maximum_cardinality_w[k_pos][item_type_id]);
                Length f_ccm_1_h = f_ccm_1(bin_type.rect.y, l, item_type.rect.y, maximum_cardinality_h[l_pos][instance.number_of_item_types()] - maximum_cardinality_h[l_pos][item_type_id]);
                Length f_ccm_2_w = f_ccm_2(bin_type.rect.x, k, item_type.rect.x);
                Length f_ccm_2_h = f_ccm_2(bin_type.rect.y, l, item_type.rect.y);

                if (instance.objective() == Objective::Knapsack) {
                    volumes[0][0][item_type_id] = f_ccm_0_w * f_ccm_0_h;
                    volumes[0][1][item_type_id] = f_ccm_0_w * f_ccm_1_h;
                    volumes[0][2][item_type_id] = f_ccm_0_w * f_ccm_2_h;
                    volumes[1][0][item_type_id] = f_ccm_1_w * f_ccm_0_h;
                    volumes[1][1][item_type_id] = f_ccm_1_w * f_ccm_1_h;
                    volumes[1][2][item_type_id] = f_ccm_1_w * f_ccm_2_h;
                    volumes[2][0][item_type_id] = f_ccm_2_w * f_ccm_0_h;
                    volumes[2][1][item_type_id] = f_ccm_2_w * f_ccm_1_h;
                    volumes[2][2][item_type_id] = f_ccm_2_w * f_ccm_2_h;
                } else {
                    f_ccm_0_w_0_h_sum += item_type.copies * f_ccm_0_w * f_ccm_0_h;
                    f_ccm_0_w_1_h_sum += item_type.copies * f_ccm_0_w * f_ccm_1_h;
                    f_ccm_0_w_2_h_sum += item_type.copies * f_ccm_0_w * f_ccm_2_h;
                    f_ccm_1_w_0_h_sum += item_type.copies * f_ccm_1_w * f_ccm_0_h;
                    f_ccm_1_w_1_h_sum += item_type.copies * f_ccm_1_w * f_ccm_1_h;
                    f_ccm_1_w_2_h_sum += item_type.copies * f_ccm_1_w * f_ccm_2_h;
                    f_ccm_2_w_0_h_sum += item_type.copies * f_ccm_2_w * f_ccm_0_h;
                    f_ccm_2_w_1_h_sum += item_type.copies * f_ccm_2_w * f_ccm_1_h;
                    f_ccm_2_w_2_h_sum += item_type.copies * f_ccm_2_w * f_ccm_2_h;
                }
            }

            if (instance.objective() == Objective::Knapsack) {
                std::array<Length, 3> f_w_bin = {f_ccm_0_w_bin, f_ccm_1_w_bin, f_ccm_2_w_bin};
                std::array<Length, 3> f_h_bin = {f_ccm_0_h_bin, f_ccm_1_h_bin, f_ccm_2_h_bin};
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
                BinPos bound_0_w_0_h = std::ceil((double)f_ccm_0_w_0_h_sum / (f_ccm_0_w_bin * f_ccm_0_h_bin));
                BinPos bound_0_w_1_h = std::ceil((double)f_ccm_0_w_1_h_sum / (f_ccm_0_w_bin * f_ccm_1_h_bin));
                BinPos bound_0_w_2_h = std::ceil((double)f_ccm_0_w_2_h_sum / (f_ccm_0_w_bin * f_ccm_2_h_bin));
                BinPos bound_1_w_0_h = std::ceil((double)f_ccm_1_w_0_h_sum / (f_ccm_1_w_bin * f_ccm_0_h_bin));
                BinPos bound_1_w_1_h = std::ceil((double)f_ccm_1_w_1_h_sum / (f_ccm_1_w_bin * f_ccm_1_h_bin));
                BinPos bound_1_w_2_h = std::ceil((double)f_ccm_1_w_2_h_sum / (f_ccm_1_w_bin * f_ccm_2_h_bin));
                BinPos bound_2_w_0_h = std::ceil((double)f_ccm_2_w_0_h_sum / (f_ccm_2_w_bin * f_ccm_0_h_bin));
                BinPos bound_2_w_1_h = std::ceil((double)f_ccm_2_w_1_h_sum / (f_ccm_2_w_bin * f_ccm_1_h_bin));
                BinPos bound_2_w_2_h = std::ceil((double)f_ccm_2_w_2_h_sum / (f_ccm_2_w_bin * f_ccm_2_h_bin));

                //std::cout << "k " << k << " l " << l << " bound_0_w_0_h " << bound_0_w_0_h << std::endl;
                //std::cout << "k " << k << " l " << l << " bound_0_w_1_h " << bound_0_w_1_h << std::endl;
                //std::cout << "k " << k << " l " << l << " bound_0_w_2_h " << bound_0_w_2_h << std::endl;
                //std::cout << "k " << k << " l " << l << " bound_1_w_0_h " << bound_1_w_0_h << std::endl;
                //std::cout << "k " << k << " l " << l << " bound_1_w_1_h " << bound_1_w_1_h << std::endl;
                //std::cout << " f_ccm_1_w_1_h_sum " << f_ccm_1_w_1_h_sum << std::endl;
                //std::cout << " f_ccm_1_w_bin " << f_ccm_1_w_bin << std::endl;
                //std::cout << " f_ccm_1_h_bin " << f_ccm_1_h_bin << std::endl;
                //std::cout << "k " << k << " l " << l << " bound_1_w_2_h " << bound_1_w_2_h << std::endl;
                //std::cout << "k " << k << " l " << l << " bound_2_w_0_h " << bound_2_w_0_h << std::endl;
                //std::cout << "k " << k << " l " << l << " bound_2_w_1_h " << bound_2_w_1_h << std::endl;
                //std::cout << "k " << k << " l " << l << " bound_2_w_2_h " << bound_2_w_2_h << std::endl;

                bound = (std::max)(bound, bound_0_w_0_h);
                bound = (std::max)(bound, bound_0_w_1_h);
                bound = (std::max)(bound, bound_0_w_2_h);
                bound = (std::max)(bound, bound_1_w_0_h);
                bound = (std::max)(bound, bound_1_w_1_h);
                bound = (std::max)(bound, bound_1_w_2_h);
                bound = (std::max)(bound, bound_2_w_0_h);
                bound = (std::max)(bound, bound_2_w_1_h);
                bound = (std::max)(bound, bound_2_w_2_h);
            }
        }
    }

    //std::cout << "bound " << bound << std::endl;
    if (instance.objective() == Objective::BinPacking) {
        algorithm_formatter.update_bin_packing_bound(bound);
    } else if (instance.objective() == Objective::Feasibility) {
        if (bound > instance.number_of_bins())
            algorithm_formatter.update_is_proven_infeasible();
    } else if (instance.objective() == Objective::Knapsack) {
        algorithm_formatter.update_knapsack_bound(knapsack_bound);
    }

    algorithm_formatter.end();
    return output;
}

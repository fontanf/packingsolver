#include "box/trivial.hpp"

#include "packingsolver/box/algorithm_formatter.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

TrivialBoundsOutput packingsolver::box::trivial_bounds(
        const Instance& instance,
        const TrivialBoundsParameters& parameters)
{
    TrivialBoundsOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.objective() == Objective::Knapsack) {
        // 1D continuous relaxation (volume-based Dantzig bound): sort items
        // by decreasing profit/volume ratio and greedily fill the total
        // available bin volume, taking the last item fractionally. Always a
        // valid, cheap (O(n log n), no search) upper bound, and much
        // tighter than the trivial "sum of all profits" whenever item
        // profits aren't roughly proportional to their volume.
        //
        // Items that don't fit (in any allowed rotation) in any bin type
        // can never be packed, so they must be excluded entirely rather
        // than counted as fractionally packable volume.
        Volume total_capacity = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            total_capacity += bin_type.volume() * bin_type.copies;
        }
        std::vector<ItemTypeId> sorted_item_types;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (instance.fits_some_bin(item_type_id))
                sorted_item_types.push_back(item_type_id);
        }
        std::sort(
                sorted_item_types.begin(),
                sorted_item_types.end(),
                [&instance](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2) -> bool
                {
                    const ItemType& item_type_1 = instance.item_type(item_type_id_1);
                    const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                    return item_type_1.profit * item_type_2.volume()
                        > item_type_2.profit * item_type_1.volume();
                });
        Profit bound = 0.0;
        Volume remaining_capacity = total_capacity;
        for (ItemTypeId item_type_id: sorted_item_types) {
            if (remaining_capacity <= 0)
                break;
            const ItemType& item_type = instance.item_type(item_type_id);
            if (item_type.volume() <= 0)
                continue;
            Volume item_total_volume = item_type.volume() * item_type.copies;
            if (item_total_volume <= remaining_capacity) {
                bound += item_type.profit * item_type.copies;
                remaining_capacity -= item_total_volume;
            } else {
                bound += item_type.profit
                    * ((double)remaining_capacity / item_type.volume());
                remaining_capacity = 0;
            }
        }
        // This bound ignores resources entirely. A 'penalize' resource with
        // a negative penalty *increases* the reported profit when
        // triggered (see 'Resource'), so add back the worst case - every
        // such resource triggering at once - to keep the bound valid.
        bound += negative_penalty_sum(instance);
        algorithm_formatter.update_knapsack_bound(bound);

    } else if (instance.objective() == Objective::BinPacking) {
        // Volume-based bound: fill bin types in the order they are
        // provided (as bins are used for this objective) until enough
        // volume is available to fit all the items. Cheap (linear in the
        // number of bin/item types), so useful when there are too many
        // (small) items for the more expensive dual feasible functions
        // bound to run.
        Volume remaining_item_volume = instance.item_volume();
        BinPos bound = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (remaining_item_volume <= 0)
                break;
            const BinType& bin_type = instance.bin_type(bin_type_id);
            if (bin_type.volume() <= 0)
                continue;
            BinPos bins_needed = (BinPos)((remaining_item_volume + bin_type.volume() - 1) / bin_type.volume());
            BinPos bins_used = std::min(bins_needed, bin_type.copies);
            bound += bins_used;
            remaining_item_volume -= bins_used * bin_type.volume();
        }
        algorithm_formatter.update_bin_packing_bound(bound);

    } else if (instance.objective() == Objective::OpenDimensionX
            || instance.objective() == Objective::OpenDimensionY
            || instance.objective() == Objective::OpenDimensionZ) {
        // Area-based bound: the open dimension cannot be smaller than what is
        // required to fit the total volume of the items in the fixed
        // cross-section of the bin.
        // Item-based bound: the open dimension cannot be smaller than the
        // smallest extent an item can have in that direction, over its allowed
        // rotations, for the item requiring the most space in that direction.
        const auto& bin_type = instance.bin_type(0);
        Volume cross_section = 0;
        if (instance.objective() == Objective::OpenDimensionX) {
            cross_section = bin_type.box.y * bin_type.box.z;
        } else if (instance.objective() == Objective::OpenDimensionY) {
            cross_section = bin_type.box.x * bin_type.box.z;
        } else {
            cross_section = bin_type.box.x * bin_type.box.y;
        }
        Length bound = (cross_section > 0)?
            (Length)((instance.item_volume() + cross_section - 1) / cross_section):
            0;

        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            Length item_min_extent = item_type.box.max();
            for (Rotation rotation: item_type.rotations) {
                Box rotated_box = item_type.box.rotate(rotation);
                Length extent
                    = (instance.objective() == Objective::OpenDimensionX)? rotated_box.x:
                    (instance.objective() == Objective::OpenDimensionY)? rotated_box.y:
                    rotated_box.z;
                item_min_extent = std::min(item_min_extent, extent);
            }
            bound = std::max(bound, item_min_extent);
        }

        if (instance.objective() == Objective::OpenDimensionX) {
            algorithm_formatter.update_open_dimension_x_bound(bound);
        } else if (instance.objective() == Objective::OpenDimensionY) {
            algorithm_formatter.update_open_dimension_y_bound(bound);
        } else {
            algorithm_formatter.update_open_dimension_z_bound(bound);
        }
    }

    algorithm_formatter.end();
    return output;
}

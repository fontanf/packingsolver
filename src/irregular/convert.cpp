#include "convert.hpp"
#include "packingsolver/irregular/instance_builder.hpp"

#include "shape/shape.hpp"

#include <algorithm>

using namespace packingsolver;
using namespace packingsolver::irregular;

Instance packingsolver::irregular::convert_song2014(const Instance& instance)
{
    InstanceBuilder builder;
    builder.set_objective(Objective::BinPacking);

    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        LengthDbl height = bin_type.aabb_orig.y_max - bin_type.aabb_orig.y_min;
        Shape square = shape::build_rectangle(height, height);
        builder.add_bin_type(square, bin_type.cost);
    }
    builder.set_bin_types_infinite_copies();

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemTypeId new_item_type_id = builder.add_item_type(
                item_type.shapes,
                item_type.profit,
                item_type.copies * 100);
        for (const AllowedRotation& rotation: item_type.allowed_rotations) {
            builder.add_item_type_allowed_rotation(
                    new_item_type_id,
                    rotation.start_angle,
                    rotation.end_angle,
                    rotation.mirror);
        }
    }

    return builder.build();
}

Instance packingsolver::irregular::convert_martinez2017(
        const Instance& instance,
        double factor)
{
    LengthDbl md = 0.0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        AxisAlignedBoundingBox aabb = item_type.compute_min_max(0.0, false, 0);
        LengthDbl width = aabb.x_max - aabb.x_min;
        LengthDbl height = aabb.y_max - aabb.y_min;
        md = std::max(md, std::max(width, height));
    }

    LengthDbl bin_size = factor * md;
    Shape square = shape::build_rectangle(bin_size, bin_size);

    InstanceBuilder builder;
    builder.set_objective(Objective::BinPacking);
    builder.add_bin_type(square);
    builder.set_bin_types_infinite_copies();

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemTypeId new_item_type_id = builder.add_item_type(
                item_type.shapes,
                item_type.profit,
                item_type.copies);
        for (const AllowedRotation& rotation: item_type.allowed_rotations) {
            builder.add_item_type_allowed_rotation(
                    new_item_type_id,
                    rotation.start_angle,
                    rotation.end_angle,
                    rotation.mirror);
        }
    }

    return builder.build();
}

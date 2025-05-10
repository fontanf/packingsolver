#include "box/instance_flipper.hpp"

#include "packingsolver/box/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

Point packingsolver::box::convert_point_back(
        const Point& point,
        Direction direction)
{
    if (direction == Direction::X) {
        return Point{point.x, point.y, point.z};
    } else if (direction == Direction::Y) {
        return Point{point.y, point.x, point.z};
    } else if (direction == Direction::Z) {
        return Point{point.z, point.y, point.x};
    }
    throw std::runtime_error("");
    return Point();
}

Instance InstanceFlipper::flip(
        const Instance& instance,
        Direction direction)
{
    InstanceBuilder flipped_instance_builder;
    if (instance.objective() == Objective::OpenDimensionY) {
        flipped_instance_builder.set_objective(Objective::OpenDimensionX);
    } else {
        flipped_instance_builder.set_objective(instance.objective());
    }
    box::Parameters flipped_instance_parameters = instance.parameters();
    flipped_instance_builder.set_parameters(flipped_instance_parameters);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinType bin_type_new = bin_type;
        if (direction_ == Direction::Y) {
            bin_type_new.box.x = bin_type.box.y;
            bin_type_new.box.y = bin_type.box.x;
        } else if (direction_ == Direction::Z) {
            bin_type_new.box.x = bin_type.box.z;
            bin_type_new.box.z = bin_type.box.x;
        }
        flipped_instance_builder.add_bin_type(
                bin_type_new,
                bin_type.copies);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemType item_type_new = item_type;
        if (direction_ == Direction::Y) {
            item_type_new.box.x = item_type.box.y;
            item_type_new.box.y = item_type.box.x;
        } else if (direction_ == Direction::Z) {
            item_type_new.box.x = item_type.box.z;
            item_type_new.box.z = item_type.box.x;
        }
        flipped_instance_builder.add_item_type(
                item_type_new,
                item_type.profit,
                item_type.copies);
    }
    return flipped_instance_builder.build();
}

Solution InstanceFlipper::unflip_solution(const Solution& flipped_solution) const
{
    Solution solution(instance_orig_);
    for (BinPos bin_pos = 0;
            bin_pos < flipped_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& flipped_bin = flipped_solution.bin(bin_pos);
        solution.add_bin(
                flipped_bin.bin_type_id,
                flipped_bin.copies);
        for (const SolutionItem& flipped_item: flipped_bin.items) {
            Point bl_corner = convert_point_back(
                    flipped_item.bl_corner,
                    direction_);
            solution.add_item(
                    bin_pos,
                    flipped_item.item_type_id,
                    bl_corner,
                    flipped_item.rotation);
        }
    }
    return solution;
}

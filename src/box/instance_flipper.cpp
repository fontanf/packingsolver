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
    throw std::runtime_error(FUNC_SIGNATURE);
    return Point();
}

static const int map_y[] = {1, 0, 3, 2, 5, 4};
static const int map_z[] = {2, 5, 0, 4, 3, 1};

Rotation packingsolver::box::get_flipped_rotation(
        Rotation r,
        Direction direction)
{
    if (direction == Direction::X) {
        return r;
    } else if (direction == Direction::Y) {
        return static_cast<Rotation>(map_y[(int)r]);
    } else if (direction == Direction::Z) {
        return static_cast<Rotation>(map_z[(int)r]);
    }
    throw std::runtime_error(FUNC_SIGNATURE);
    return r;
}

std::vector<Rotation> InstanceFlipper::flip_rotations(
        const std::vector<Rotation>& rotations) const
{
    std::vector<Rotation> flipped;
    for (Rotation r: rotations)
        flipped.push_back(get_flipped_rotation(r, direction_));
    return flipped;
}

Instance InstanceFlipper::flip(
        const Instance& instance)
{
    InstanceBuilder flipped_instance_builder;
    if (instance.objective() == Objective::OpenDimensionY
            || instance.objective() == Objective::OpenDimensionZ) {
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
        Length flipped_x = bin_type.box.x;
        Length flipped_y = bin_type.box.y;
        Length flipped_z = bin_type.box.z;
        if (direction_ == Direction::Y) {
            flipped_x = bin_type.box.y;
            flipped_y = bin_type.box.x;
        } else if (direction_ == Direction::Z) {
            flipped_x = bin_type.box.z;
            flipped_z = bin_type.box.x;
        }
        BinTypeId flipped_bin_type_id = flipped_instance_builder.add_bin_type(
                flipped_x,
                flipped_y,
                flipped_z,
                bin_type.cost,
                bin_type.copies);
        flipped_instance_builder.set_bin_type_maximum_weight(
                flipped_bin_type_id,
                bin_type.maximum_weight);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemTypeId flipped_item_type_id = flipped_instance_builder.add_item_type(
                item_type.box.x,
                item_type.box.y,
                item_type.box.z,
                item_type.profit,
                item_type.copies);
        for (Rotation r: flip_rotations(item_type.rotations))
            flipped_instance_builder.add_item_type_rotation(flipped_item_type_id, r);
        flipped_instance_builder.set_item_type_weight(
                flipped_item_type_id,
                item_type.weight);
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
            Point bl_corner = convert_point_back(flipped_item.bl_corner, direction_);
            Rotation original_rotation = get_flipped_rotation(flipped_item.rotation, direction_);
            solution.add_item(
                    bin_pos,
                    flipped_item.item_type_id,
                    bl_corner,
                    original_rotation);
        }
    }
    return solution;
}

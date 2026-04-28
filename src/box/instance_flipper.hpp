#pragma once

#include "packingsolver/box/solution.hpp"

namespace packingsolver
{
namespace box
{

Point convert_point_back(
        const Point& point,
        Direction direction);

Rotation get_flipped_rotation(
        Rotation r,
        Direction direction);

class InstanceFlipper
{
public:

    InstanceFlipper(
            const Instance& instance,
            Direction direction):
        instance_orig_(instance),
        direction_(direction),
        instance_flipped_(flip(instance)) { }

    const Instance& flipped_instance() const { return instance_flipped_; }

    Solution unflip_solution(const Solution& solution) const;

private:

    std::vector<Rotation> flip_rotations(const std::vector<Rotation>& rotations) const;

    Instance flip(const Instance& instance);

    const Instance& instance_orig_;

    Direction direction_;

    Instance instance_flipped_;

};

}
}

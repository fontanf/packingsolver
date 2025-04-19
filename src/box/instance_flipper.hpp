#pragma once

#include "packingsolver/box/solution.hpp"

namespace packingsolver
{
namespace box
{

class InstanceFlipper
{
public:

    InstanceFlipper(
            const Instance& instance,
            Direction direction):
        instance_orig_(instance),
        direction_(direction),
        instance_flipped_(flip(instance, direction)) { }

    const Instance& flipped_instance() const { return instance_flipped_; }

    Solution unflip_solution(const Solution& solution) const;

private:

    Instance flip(const Instance& instance, Direction direction);

    const Instance& instance_orig_;

    Direction direction_;

    Instance instance_flipped_;

};

}
}

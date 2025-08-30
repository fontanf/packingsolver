#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

struct SimplifiedShape
{
    ShapeWithHoles shape;

    ShapeWithHoles shape_inflated;
};

struct SimplifiedBinType
{
    std::vector<SimplifiedShape> borders;

    std::vector<SimplifiedShape> defects;
};

struct SimplifiedItemType
{
    std::vector<SimplifiedShape> shapes;
};

struct SimplifiedInstance
{
    std::vector<SimplifiedItemType> item_types;

    std::vector<SimplifiedBinType> bin_types;
};

SimplifiedInstance shape_simplification(
        const Instance& instance,
        double maximum_approximation_ratio);

}
}

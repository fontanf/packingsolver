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

/**
 * 'minimum_number_of_vertices' is forwarded as-is to shape::simplify (see
 * its own doc): every border, defect, and item shape (and their inflated
 * counterparts) already at or below it is left unapproximated, regardless
 * of how much of the area budget implied by maximum_approximation_ratio is
 * left unspent.
 */
SimplifiedInstance shape_simplification(
        const Instance& instance,
        double maximum_approximation_ratio,
        shape::ElementPos minimum_number_of_vertices = 4);

}
}

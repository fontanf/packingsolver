#pragma once

#include "packingsolver/irregular/shape.hpp"

namespace packingsolver
{
namespace irregular
{

std::pair<Shape, std::vector<Shape>> remove_self_intersections(
        const Shape& shape);

std::pair<Shape, std::vector<Shape>> compute_union(
        const Shape& shape_1,
        const Shape& shape_2);

std::vector<Shape> extract_all_holes_from_self_intersecting_hole(
        const Shape& shape);

}
}

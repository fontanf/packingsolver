#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

std::pair<Shape, std::vector<Shape>> remove_self_intersections(
        const Shape& shape);

std::vector<Shape> extract_all_holes_from_self_intersecting_hole(
        const Shape& shape);

}
}

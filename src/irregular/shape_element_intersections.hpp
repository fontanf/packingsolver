#pragma once

#include "packingsolver/irregular/shape.hpp"

namespace packingsolver
{
namespace irregular
{

std::vector<Point> compute_intersections(
        const ShapeElement& element_1,
        const ShapeElement& element_2,
        bool strict = false);

}
}

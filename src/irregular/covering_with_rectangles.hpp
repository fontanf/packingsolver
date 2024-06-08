#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

using RectanglePos = int64_t;

struct ShapeRectangle
{
    Point bottom_left;

    Point top_right;
};

std::vector<ShapeRectangle> compute_covering_with_rectangle(
        const Shape& shape,
        const std::vector<Shape>& holes);

}
}

#pragma once

#include "irregular/trapezoid.hpp"

namespace packingsolver
{
namespace irregular
{

std::vector<GeneralizedTrapezoid> polygon_trapezoidation(
        const Shape& shape,
        const std::vector<Shape>& holes = {});

}
}

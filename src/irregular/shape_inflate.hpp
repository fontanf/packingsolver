#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

Shape inflate_shape_without_holes(
        const Shape& shape,
        LengthDbl value);

std::pair<Shape, std::vector<Shape>> inflate(
        const Shape& shape,
        LengthDbl value,
        const std::vector<Shape>& holes = {});

ShapeElement offset_element(const ShapeElement& element, LengthDbl value);

bool is_degenerate_element(const ShapeElement& element);

bool is_arc_covered_by_adjacent_elements(
        const ShapeElement& prev_element,
        const ShapeElement& arc_element,
        const ShapeElement& next_element,
        LengthDbl value);

}
} 
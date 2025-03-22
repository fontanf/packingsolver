#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

Shape close_inflated_elements(const std::vector<ShapeElement>& inflated_elements, bool is_deflating = false);

LengthDbl euclidean_distance(const Point& p1, const Point& p2);

bool are_line_segments_collinear(
        const Point& line1_start, 
        const Point& line1_end, 
        const Point& line2_start, 
        const Point& line2_end);

bool is_line_segment_tangent_to_arc(
        const Point& line_start, 
        const Point& line_end, 
        const Point& arc_center, 
        const Point& arc_start);

bool are_arcs_tangent(const ShapeElement& arc1, const ShapeElement& arc2);

}
}

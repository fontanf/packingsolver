#pragma once

#include "packingsolver/irregular/shape.hpp"

namespace packingsolver
{
namespace irregular
{

std::pair<Shape, std::vector<Shape>> remove_self_intersections(
        const Shape& shape);

std::vector<Shape> extract_all_holes_from_self_intersecting_hole(
        const Shape& shape);

// Remove intersections between shape elements, returning a set of non-intersecting segments
std::vector<ShapeElement> remove_intersections_segments(
        const std::vector<ShapeElement>& elements,
        bool is_deflating = false);

// Helper function to check if a point is inside a box
bool is_point_inside_box(const Point& p, const Point& min_corner, const Point& max_corner);

}
}

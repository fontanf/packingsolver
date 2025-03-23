#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

// Check if a point is on a line segment
bool is_point_on_line_segment(
        const Point& p,
        const Point& start,
        const Point& end);

// Check if a point is strictly inside a shape (excluding the boundary)
bool is_point_strictly_inside_shape(
        const Point& point,
        const Shape& shape);

// Check if a point is inside a shape or on its boundary
bool is_point_inside_or_on_shape(
        const Point& point,
        const Shape& shape);

std::pair<bool, Shape> remove_redundant_vertices(
        const Shape& shape);

std::pair<bool, Shape> remove_aligned_vertices(
        const Shape& shape);

std::pair<bool, Shape> equalize_close_y(
        const Shape& shape);

Shape clean_shape(
        const Shape& shape);

std::vector<Shape> borders(
        const Shape& shape);

bool operator==(
        const ShapeElement& element_1,
        const ShapeElement& element_2);

bool operator==(
        const Shape& shape_1,
        const Shape& shape_2);

}
}

#include "irregular/covering_with_rectangles.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

std::vector<ShapeRectangle> packingsolver::irregular::compute_covering_with_rectangle(
        const Shape& shape,
        const std::vector<Shape>& holes)
{
    std::vector<ShapeRectangle> shape_rectangles;
    ShapeRectangle shape_rectangle;
    auto min_max = shape.compute_min_max();
    shape_rectangle.bottom_left = min_max.first;
    shape_rectangle.top_right = min_max.second;
    shape_rectangles.push_back(shape_rectangle);
    return shape_rectangles;
}

#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

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

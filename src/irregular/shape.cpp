#include "irregular/shape.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

Shape irregular::clean_shape(
        const Shape& shape)
{
    Shape shape_tmp_1 = shape;
    Shape shape_tmp_2;

    // Remove redondant vertices.
    shape_tmp_2.elements.clear();
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape_tmp_1.elements.size();
            ++element_pos) {
        const ShapeElement& element = shape_tmp_1.elements[element_pos];
        if (equal(element.start.x, element.end.x)
                && equal(element.start.y, element.end.y))
            continue;
        shape_tmp_2.elements.push_back(element);
    }
    std::swap(shape_tmp_1, shape_tmp_2);

    // Remove vertices aligned with their previous and next vertices.
    shape_tmp_2.elements.clear();
    std::vector<uint8_t> useless(shape.elements.size(), false);
    ElementPos element_prev_pos = shape_tmp_1.elements.size() - 1;
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape_tmp_1.elements.size();
            ++element_pos) {
        const ShapeElement& element = shape_tmp_1.elements[element_pos];
        ShapeElement& element_prev = shape_tmp_1.elements[element_prev_pos];
        if (element.type == ShapeElementType::LineSegment
                && element_prev.type == ShapeElementType::LineSegment) {
            double x1 = element_prev.start.x;
            double y1 = element_prev.start.y;
            double x2 = element.start.x;
            double y2 = element.start.y;
            double x3 = element.end.x;
            double y3 = element.end.y;
            double v = x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2);
            if (equal(v, 0)) {
                useless[v] = 1;
                element_prev.end = element.end;
            }
        }
        element_prev_pos = element_pos;
    }
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape_tmp_1.elements.size();
            ++element_pos) {
        const ShapeElement& element = shape_tmp_1.elements[element_pos];
        if (useless[element_pos])
            continue;
        shape_tmp_2.elements.push_back(element);
    }
    std::swap(shape_tmp_1, shape_tmp_2);

    return shape_tmp_1;
}

Shape irregular::clean_shape_y(const Shape& shape)
{
    Shape new_shape = shape;
    for (;;) {
        bool found = false;
        ElementPos element_prev_pos = new_shape.elements.size() - 1;
        for (ElementPos element_pos = 0;
                element_pos < (ElementPos)new_shape.elements.size();
                ++element_pos) {
            ShapeElement& element = new_shape.elements[element_pos];
            const ShapeElement& element_prev = new_shape.elements[element_prev_pos];
            if (equal(element.start.y, element_prev.start.y)
                    && element.start.y != element_prev.start.y) {
                element.start.y = element_prev.start.y;
                found = true;
            }
        }
        if (!found)
            break;
    }
    return new_shape;
}

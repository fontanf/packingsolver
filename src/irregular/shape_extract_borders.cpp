#include "irregular/shape_extract_borders.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

std::vector<Shape> packingsolver::irregular::extract_borders(
        const Shape& shape)
{
    std::vector<Shape> res;

    auto mm = shape.compute_min_max();
    //std::cout << "mm.first.x " << mm.first.x
    //    << " mm.first.y " << mm.first.y
    //    << " mm.second.x " << mm.second.x
    //    << " mm.second.y " << mm.second.y
    //    << std::endl;

    Shape shape_border;
    ElementPos element_0_pos = 0;
    for (ElementPos element_pos = 0;
            element_pos < shape.elements.size();
            ++element_pos) {
        const ShapeElement& shape_element = shape.elements[element_pos];
        if (shape_element.start.x == mm.first.x) {
            element_0_pos = element_pos;
            break;
        }
    }
    //std::cout << "element_0_pos " << element_0_pos << std::endl;
    // 0: left; 1: bottom; 2: right; 3: top.
    const ShapeElement& element_0 = shape.elements[element_0_pos];
    int start_border = (element_0.start.y == mm.first.y)? 1: 0;
    LengthDbl start_coordinate = element_0.start.y;
    for (ElementPos element_pos = 0;
            element_pos < shape.elements.size();
            ++element_pos) {
        const ShapeElement& element = shape.elements[(element_0_pos + element_pos) % shape.elements.size()];
        //std::cout << "element_pos " << ((element_0_pos + element_pos) % shape.elements.size())
        //    << " / " << shape.elements.size()
        //    << ": " << element.to_string()
        //    << "; start_border: " << start_border
        //    << std::endl;
        shape_border.elements.push_back(element);
        bool close = false;
        if (start_border == 0) {
            if (equal(element.end.x, mm.first.x)) {
                ShapeElement new_element;
                new_element.type = ShapeElementType::LineSegment;
                new_element.start = element.end;
                new_element.end = shape_border.elements[0].start;
                shape_border.elements.push_back(new_element);
                close = true;
                if (!equal(element.end.y, mm.first.y)) {
                    start_border = 0;
                } else {
                    start_border = 1;
                }
            } else if (equal(element.end.y, mm.first.y)) {
                if (element.end.x != shape_border.elements[0].start.x) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {mm.first.x, mm.first.y};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                }
                close = true;
                if (!equal(element.end.x, mm.second.x)) {
                    start_border = 1;
                } else {
                    start_border = 2;
                }
            }
        } else if (start_border == 1) {
            if (equal(element.end.y, mm.first.y)) {
                ShapeElement new_element;
                new_element.type = ShapeElementType::LineSegment;
                new_element.start = element.end;
                new_element.end = shape_border.elements[0].start;
                shape_border.elements.push_back(new_element);
                close = true;
                if (!equal(element.end.x, mm.second.x)) {
                    start_border = 1;
                } else {
                    start_border = 2;
                }
            } else if (equal(element.end.x, mm.second.x)) {
                if (element.end.y != shape_border.elements[0].start.y) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {mm.second.x, mm.first.y};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                }
                close = true;
                if (!equal(element.end.y, mm.second.y)) {
                    start_border = 2;
                } else {
                    start_border = 3;
                }
            }
        } else if (start_border == 2) {
            if (equal(element.end.x, mm.second.x)) {
                ShapeElement new_element;
                new_element.type = ShapeElementType::LineSegment;
                new_element.start = element.end;
                new_element.end = shape_border.elements[0].start;
                shape_border.elements.push_back(new_element);
                close = true;
                if (!equal(element.end.y, mm.second.y)) {
                    start_border = 2;
                } else {
                    start_border = 3;
                }
            } else if (equal(element.end.y, mm.second.y)) {
                if (element.end.y != shape_border.elements[0].start.y) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {mm.second.x, mm.second.y};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                }
                close = true;
                if (!equal(element.end.x, mm.second.x)) {
                    start_border = 3;
                } else {
                    start_border = 0;
                }
            }
        } else if (start_border == 3) {
            if (equal(element.end.y, mm.second.y)) {
                ShapeElement new_element;
                new_element.type = ShapeElementType::LineSegment;
                new_element.start = element.end;
                new_element.end = shape_border.elements[0].start;
                shape_border.elements.push_back(new_element);
                close = true;
                if (!equal(element.end.x, mm.first.x)) {
                    start_border = 3;
                } else {
                    start_border = 0;
                }
            } else if (equal(element.end.x, mm.first.x)) {
                if (element.end.x != shape_border.elements[0].start.x) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {mm.first.x, mm.second.y};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                }
                close = true;
                if (!equal(element.end.y, mm.second.y)) {
                    start_border = 0;
                } else {
                    start_border = 1;
                }
            }
        }
        //std::cout << "shape_border " << shape_border.to_string(0) << std::endl;
        // New shape.
        if (close) {
            //std::cout << "close " << shape_border.to_string(0) << std::endl;
            if (shape_border.elements.size() >= 3)
                res.push_back(shape_border.reverse());
            shape_border.elements.clear();
        }
    }

    return res;
}

#include "irregular/trapezoid_slider.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

bool packingsolver::irregular::slide_trapezoid(
        SlidePosition& position,
        const Shape& supporting_shape,
        const Shape& supported_shape,
        const GeneralizedTrapezoid& trapezoid_to_avoid,
        GeneralizedTrapezoid& current_trapezoid)
{
    bool updated = false;
    for (;;) {
        if (!current_trapezoid.intersect(trapezoid_to_avoid))
            return updated;

        double direction = 0.0;
        Point point_limit = {0.0, 0.0};
        bool update_supporting_element = false;
        bool update_supported_element = false;
        if (position.supported_part_element_pos == supported_shape.elements.size()) {
            const ShapeElement& supporting_element = supporting_shape.elements[position.supporting_part_element_pos];
            double supporting_slope
                = (supporting_element.end.y - supporting_element.start.y)
                / (supporting_element.end.x - supporting_element.start.x);
            direction = supporting_slope;
            point_limit = supporting_element.end - supported_shape.elements.back().end;
            update_supporting_element = true;
            update_supported_element = false;
        } else if (position.supporting_part_element_pos == supporting_shape.elements.size()) {
            const ShapeElement& supported_element = supported_shape.elements[position.supported_part_element_pos];
            double supported_slope
                = (supported_element.end.y - supported_element.start.y)
                / (supported_element.end.x - supported_element.start.x);
            direction = supported_slope;
            point_limit = supporting_shape.elements.back().end - supported_element.end;
            update_supporting_element = false;
            update_supported_element = true;
        } else {
            const ShapeElement& supporting_element = supporting_shape.elements[position.supporting_part_element_pos];
            const ShapeElement& supported_element = supported_shape.elements[position.supported_part_element_pos];
            ShapeElement supported_element_shifted = supported_element;
            supported_element_shifted.shift(position.x, position.y);
            //std::cout << "supporting_element " << supporting_element.to_string() << std::endl
            //    << " supported_element " << supported_element.to_string() << std::endl
            //    << " shifted " << supported_element_shifted.to_string() << std::endl;
            double supporting_slope
                = (supporting_element.end.y - supporting_element.start.y)
                / (supporting_element.end.x - supporting_element.start.x);
            double supported_slope
                = (supported_element.end.y - supported_element.start.y)
                / (supported_element.end.x - supported_element.start.x);
            //std::cout << "supporting_slope " << supporting_slope
            //    << " supported_slope " << supported_slope << std::endl;
            if (equal(supporting_slope, supported_slope)) {
                direction = supported_slope;
                point_limit = supporting_element.end - supported_element.end;
                update_supporting_element = true;
                update_supported_element = true;
            } else if (supporting_slope > supported_slope) {
                direction = supporting_slope;
                point_limit = supporting_element.end - supported_element.start;
                update_supporting_element = true;
                update_supported_element = false;
            } else {
                direction = supported_slope;
                point_limit = supporting_element.start - supported_element.end;
                update_supporting_element = false;
                update_supported_element = true;
            }
        }

        LengthDbl lx = (direction == 0.0)?
            current_trapezoid.compute_right_shift_if_intersects(
                trapezoid_to_avoid):
            current_trapezoid.compute_top_right_shift(
                trapezoid_to_avoid,
                direction);
        LengthDbl ly = lx * direction;
        //std::cout << "direction " << direction
        //    << " point_limit " << point_limit.to_string()
        //    << " lx " << lx << " ly " << ly << std::endl;
        if (equal(position.x + lx, position.x) && equal(position.y + ly, position.y)) {
            return updated;
        } else if (!strictly_greater(position.x + lx, point_limit.x)) {
            // Same element.
            current_trapezoid.shift_right(lx);
            current_trapezoid.shift_top(ly);
            position.x += lx;
            position.y += ly;
            return true;
        } else {
            // Change element.
            if (update_supporting_element)
                position.supporting_part_element_pos++;
            if (update_supported_element)
                position.supported_part_element_pos++;
            if (position.supporting_part_element_pos == supporting_shape.elements.size()
                    && position.supported_part_element_pos == supported_shape.elements.size()) {
                return updated;
            }
            current_trapezoid.shift_right(point_limit.x - position.x);
            current_trapezoid.shift_top(point_limit.y - position.y);
            position.x = point_limit.x;
            position.y = point_limit.y;
            updated = true;
        }
    }
    return true;
}

bool TrapezoidSetSlider::slide_right_of(
        SlidePosition& position,
        LengthDbl x_wall,
        LengthDbl x_far,
        LengthDbl y_min,
        LengthDbl y_max) const
{
    for (;;) {
        bool moved = false;
        for (const auto& item_shape_trapezoids: trapezoid_set_.shapes) {
            for (const GeneralizedTrapezoid& item_shape_trapezoid: item_shape_trapezoids) {
                GeneralizedTrapezoid current_trapezoid = item_shape_trapezoid;
                current_trapezoid.shift_right(position.x);
                current_trapezoid.shift_top(position.y);

                GeneralizedTrapezoid trapezoid_to_avoid(
                        std::min(y_min, current_trapezoid.y_bottom()),
                        std::max(y_max, current_trapezoid.y_top()),
                        std::min(x_far, current_trapezoid.x_min()),
                        x_wall,
                        std::min(x_far, current_trapezoid.x_min()),
                        x_wall);

                bool updated = slide_trapezoid(
                        position,
                        supporting_shape_,
                        supported_shape_,
                        trapezoid_to_avoid,
                        current_trapezoid);
                if (end_of_support(position))
                    return false;
                if (updated) {
                    moved = true;
                    if (out_of_bounds(position))
                        return false;
                }
            }
        }
        if (!moved)
            return true;
    }
}

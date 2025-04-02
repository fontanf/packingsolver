#include "irregular/shape_convex_hull.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

Shape irregular::convex_hull(
        const Shape& shape)
{
    //std::cout << "shape.elements.size() " << shape.elements.size() << std::endl;
    if (shape.elements.size() < 3)
        return shape;

    Point point_least_y = {
        std::numeric_limits<LengthDbl>::infinity(),
        std::numeric_limits<LengthDbl>::infinity()};
    for (const ShapeElement& shape_element: shape.elements) {
        if (point_least_y.y > shape_element.start.y
                || (point_least_y.y == shape_element.start.y
                    && point_least_y.x > shape_element.start.x)) {
            point_least_y = shape_element.start;
        }
    }

    // swap the pivot with the first point
    std::vector<Point> points;
    for (const ShapeElement& shape_element: shape.elements) {
        if (shape_element.start == point_least_y)
            continue;
        points.push_back(shape_element.start);
    }

    std::sort(
            points.begin(),
            points.end(),
            [&point_least_y](
                const Point& point_1,
                const Point& point_2)
            {
                int order = counter_clockwise(
                        point_least_y,
                        point_1,
                        point_2);
                if (order != 0)
                    return (order == -1);
                return squared_distance(point_least_y, point_1)
                    < squared_distance(point_least_y, point_2);
            });

    std::vector<Point> convex_hull;
    convex_hull.push_back(point_least_y);
    //std::cout << "push " << point_least_y.to_string() << std::endl;
    convex_hull.push_back(points[0]);
    //std::cout << "push " << points[0].to_string() << std::endl;
    convex_hull.push_back(points[1]);
    //std::cout << "push " << points[1].to_string() << std::endl;

    for (ElementPos pos = 2; pos < (ElementPos)points.size(); pos++) {
        //std::cout << "pos " << pos << " / " << points.size() << std::endl;
        //std::cout << "point " << points[pos].to_string() << std::endl;
        Point top = convex_hull.back();
        convex_hull.pop_back();
        //std::cout << "pop " << top.to_string() << std::endl;
        while (convex_hull.size() > 1
                && counter_clockwise(convex_hull.back(), top, points[pos]) != -1) {
            top = convex_hull.back();
            convex_hull.pop_back();
            //std::cout << "pop " << top.to_string() << std::endl;
        }
        convex_hull.push_back(top);
        //std::cout << "push " << top.to_string() << std::endl;
        convex_hull.push_back(points[pos]);
        //std::cout << "push " << points[pos].to_string() << std::endl;
    }

    Shape convex_hull_shape;
    for (ElementPos pos = 0;
            pos < convex_hull.size();
            ++pos) {
        ShapeElement element;
        element.type = ShapeElementType::LineSegment;
        element.start = convex_hull[pos];
        element.end = convex_hull[(pos + 1) % convex_hull.size()];
        convex_hull_shape.elements.push_back(element);
    }

    if (strictly_lesser(convex_hull_shape.compute_area(), shape.compute_area())) {
        throw std::runtime_error(
                "packingsolver::irregular::polygon_convex_hull; "
                "shape.compute_area(): " + std::to_string(shape.compute_area()) + "; "
                "convex_hull_shape.compute_area(): " + std::to_string(convex_hull_shape.compute_area()) + ".");

    }

    return convex_hull_shape;
}

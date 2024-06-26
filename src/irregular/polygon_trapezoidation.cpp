#include "irregular/polygon_trapezoidation.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

enum class VertexTypeFlag
{
    LocalMaximumConvex,
    LocalMinimumConvex,
    LocalMaximumConcave,
    LocalMinimumConcave,
    Inflection,
    HorizontalLocalMaximumConvex,
    HorizontalLocalMinimumConvex,
    HorizontalLocalMaximumConcave,
    HorizontalLocalMinimumConcave,
    StrictlyHorizontal,
};

struct Vertex
{
    /** Vertex type flag. */
    VertexTypeFlag flag;
};

struct OpenTrapezoid
{
    Point bottom_left;
    Point bottom_right;
    Point top_left;
    Point top_right;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const OpenTrapezoid& open_trapezoid)
{
    os << "bl " << open_trapezoid.bottom_left.x << " " << open_trapezoid.bottom_left.y
        << " br " << open_trapezoid.bottom_right.x << " " << open_trapezoid.bottom_right.y
        << " tl " << open_trapezoid.top_left.x << " " << open_trapezoid.top_left.y
        << " tr " << open_trapezoid.top_right.x << " " << open_trapezoid.top_right.y;
    return os;
}

LengthDbl x(
        const Point& bottom,
        const Point& top,
        LengthDbl y)
{
    if (y == bottom.y)
        return bottom.x;
    if (y == top.y)
        return top.x;
    double a = (top.x - bottom.x) / (top.y - bottom.y);
    return bottom.x + (y - bottom.y) * a;
}

std::pair<ElementPos, ElementPos> find_trapezoid_containing_vertex(
        const std::vector<OpenTrapezoid>& open_trapezoids,
        const Point& vertex)
{
    std::pair<ElementPos, ElementPos> res = {-1, -1};
    LengthDbl x_left_1 = 0;
    for (ElementPos open_trapezoid_pos = 0;
            open_trapezoid_pos < (TrapezoidPos)open_trapezoids.size();
            ++open_trapezoid_pos) {
        const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

        LengthDbl x_left = x(open_trapezoid.bottom_left, open_trapezoid.top_left, vertex.y);
        LengthDbl x_right = x(open_trapezoid.bottom_right, open_trapezoid.top_right, vertex.y);
        //std::cout << "x_left " << x_left << " x " << vertex.x << " x_right " << x_right << std::endl;
        if (!striclty_greater(x_left, vertex.x)
                && !striclty_lesser(x_right, vertex.x)) {
            if (std::get<0>(res) == -1) {
                std::get<0>(res) = open_trapezoid_pos;
                x_left_1 = x_left;
            } else {
                if (striclty_lesser(x_left_1, x_left)) {
                    std::get<1>(res) = open_trapezoid_pos;
                } else {
                    std::get<1>(res) = std::get<0>(res);
                    std::get<0>(res) = open_trapezoid_pos;
                }
                return res;
            }
        }
    }
    return res;
}

inline Point get_vertex(
        const Shape& shape,
        ElementPos element_pos)
{
    ElementPos n = shape.elements.size();
    return shape.elements[((element_pos % n) + n) % n].start;
}

}

std::vector<GeneralizedTrapezoid> packingsolver::irregular::polygon_trapezoidation(
        const Shape& shape)
{
    //std::cout << "polygon_trapezoidation" << std::endl;
    //std::cout << shape.to_string(0) << std::endl;
    std::vector<GeneralizedTrapezoid> trapezoids;

    // Sort vertices according to their y coordinate.
    std::vector<ElementPos> sorted_vertices(shape.elements.size());
    std::iota(sorted_vertices.begin(), sorted_vertices.end(), 0);
    std::sort(
            sorted_vertices.begin(),
            sorted_vertices.end(),
            [&shape](
                ElementPos element_pos_1,
                ElementPos element_pos_2)
            {
                if (shape.elements[element_pos_1].start.y
                        != shape.elements[element_pos_2].start.y) {
                    return shape.elements[element_pos_1].start.y
                        > shape.elements[element_pos_2].start.y;
                }
                return shape.elements[element_pos_1].start.x
                    < shape.elements[element_pos_2].start.x;
            });

    // Classify the vertices.
    std::vector<Vertex> vertices(shape.elements.size());
    ElementPos element_pos_prev = shape.elements.size() - 1;
    for (ElementPos element_pos = 0;
            element_pos < shape.elements.size();
            ++element_pos) {
        const ShapeElement& element = shape.elements[element_pos];
        const ShapeElement& element_prev = shape.elements[element_pos_prev];

        // Convexity.
        // The convexity can be determined easily by investigating the sign of the
        // cross product of the edges meeting at the considered vertex.
        double v = cross_product(
                element_prev.end - element_prev.start,
                element.end - element.start);
        bool is_convex = (v >= 0);

        // Local extreme of the vertices.
        if (element_prev.start.y < element.start.y
                && element.start.y < element.end.y) {
            vertices[element_pos].flag = VertexTypeFlag::Inflection;
        } else if (element_prev.start.y > element.start.y
                && element.start.y > element.end.y) {
            vertices[element_pos].flag = VertexTypeFlag::Inflection;
        } else if (element.start.y < element_prev.start.y
                && element.start.y < element.end.y) {
            vertices[element_pos].flag = (is_convex)?
                VertexTypeFlag::LocalMinimumConvex:
                VertexTypeFlag::LocalMinimumConcave;
        } else if (element.start.y > element_prev.start.y
                && element.start.y > element.end.y) {
            vertices[element_pos].flag = (is_convex)?
                VertexTypeFlag::LocalMaximumConvex:
                VertexTypeFlag::LocalMaximumConcave;
        } else if (element.start.y == element_prev.start.y
                && element.start.y < element.end.y) {
            vertices[element_pos].flag = (is_convex)?
                VertexTypeFlag::HorizontalLocalMinimumConvex:
                VertexTypeFlag::HorizontalLocalMinimumConcave;
        } else if (element.start.y < element_prev.start.y
                && element.start.y == element.end.y) {
            vertices[element_pos].flag = (is_convex)?
                VertexTypeFlag::HorizontalLocalMinimumConvex:
                VertexTypeFlag::HorizontalLocalMinimumConcave;
        } else if (element.start.y == element_prev.start.y
                && element.start.y > element.end.y) {
            vertices[element_pos].flag = (is_convex)?
                VertexTypeFlag::HorizontalLocalMaximumConvex:
                VertexTypeFlag::HorizontalLocalMaximumConcave;
        } else if (element.start.y > element_prev.start.y
                && element.start.y == element.end.y) {
            vertices[element_pos].flag = (is_convex)?
                VertexTypeFlag::HorizontalLocalMaximumConvex:
                VertexTypeFlag::HorizontalLocalMaximumConcave;
        } else if (element.start.y == element_prev.start.y
                && element.start.y == element.end.y) {
            vertices[element_pos].flag = VertexTypeFlag::StrictlyHorizontal;
        }

        element_pos_prev = element_pos;
    }

    // Sweep.
    std::vector<OpenTrapezoid> open_trapezoids;
    for (ElementPos vertex_pos = 0;
            vertex_pos < (ElementPos)shape.elements.size();
            ++vertex_pos) {
        ElementPos element_pos = sorted_vertices[vertex_pos];
        ElementPos element_pos_next = sorted_vertices[(vertex_pos + 1) % shape.elements.size()];
        const Point& vertex = shape.elements[element_pos].start;
        const Point& vertex_next = shape.elements[element_pos_next].start;
        //std::cout << "element_pos " << element_pos << " element_pos_next " << element_pos_next << std::endl;
        //std::cout << "vertex " << vertex.x << " " << vertex.y << std::endl;
        //std::cout << "vertex_next " << get_vertex(shape, element_pos_next).x << " " << get_vertex(shape, element_pos_next).y << std::endl;
        //std::cout << "open trapezoids:" << std::endl;
        //for (const OpenTrapezoid& open_trapezoid: open_trapezoids)
        //    std::cout << open_trapezoid << std::endl;
        if (vertices[element_pos].flag == VertexTypeFlag::LocalMaximumConvex) {
            // +1 open trapezoid.
            //std::cout << "LocalMaximumConvex" << std::endl;

            // Update open_trapezoids.
            OpenTrapezoid open_trapezoid;
            open_trapezoid.top_left = get_vertex(shape, element_pos);
            open_trapezoid.top_right = get_vertex(shape, element_pos);
            open_trapezoid.bottom_left = get_vertex(shape, element_pos + 1);
            open_trapezoid.bottom_right = get_vertex(shape, element_pos - 1);
            ElementPos open_trapezoid_pos = open_trapezoids.size();
            open_trapezoids.push_back(open_trapezoid);

        } else if (vertices[element_pos].flag == VertexTypeFlag::LocalMinimumConvex) {
            // -1 open trapezoid.
            //std::cout << "LocalMinimumConvex" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            GeneralizedTrapezoid trapezoid(
                    vertex.y,
                    open_trapezoid.top_left.y,
                    vertex.x,
                    vertex.x,
                    open_trapezoid.top_left.x,
                    open_trapezoid.top_right.x);
            trapezoids.push_back(trapezoid);

            // Update open_trapezoids.
            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

        } else if (vertices[element_pos].flag == VertexTypeFlag::LocalMaximumConcave) {
            // -1 open trapezoid.
            // +2 open trapezoids.
            //std::cout << "LocalMaximumConcave" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            LengthDbl x_left = x(open_trapezoid.bottom_left, open_trapezoid.top_left, vertex.y);
            LengthDbl x_right = x(open_trapezoid.bottom_right, open_trapezoid.top_right, vertex.y);
            if (vertex.y != open_trapezoid.top_left.y) {
                GeneralizedTrapezoid trapezoid(
                        vertex.y,
                        open_trapezoid.top_left.y,
                        x_left,
                        x_right,
                        open_trapezoid.top_left.x,
                        open_trapezoid.top_right.x);
                trapezoids.push_back(trapezoid);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid_1;
            new_open_trapezoid_1.top_left = {x_left, vertex.y};
            new_open_trapezoid_1.top_right = vertex;
            new_open_trapezoid_1.bottom_left = open_trapezoid.bottom_left;
            new_open_trapezoid_1.bottom_right = get_vertex(shape, element_pos - 1);
            open_trapezoids.push_back(new_open_trapezoid_1);

            OpenTrapezoid new_open_trapezoid_2;
            new_open_trapezoid_2.top_left = vertex;
            new_open_trapezoid_2.top_right = {x_right, vertex.y};
            new_open_trapezoid_2.bottom_left = get_vertex(shape, element_pos + 1);
            new_open_trapezoid_2.bottom_right = open_trapezoid.bottom_right;
            open_trapezoids.push_back(new_open_trapezoid_2);

            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

        } else if (vertices[element_pos].flag == VertexTypeFlag::LocalMinimumConcave) {
            // -2 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "LocalMinimumConcave" << std::endl;

            auto p = find_trapezoid_containing_vertex(open_trapezoids, vertex);
            const OpenTrapezoid& open_trapezoid_1 = open_trapezoids[p.first];
            const OpenTrapezoid& open_trapezoid_2 = open_trapezoids[p.second];

            // Update trapezoids.
            LengthDbl x_left = x(open_trapezoid_1.bottom_left, open_trapezoid_1.top_left, vertex.y);
            if (vertex.y != open_trapezoid_1.top_left.y) {
                GeneralizedTrapezoid trapezoid_1(
                        vertex.y,
                        open_trapezoid_1.top_left.y,
                        x_left,
                        vertex.x,
                        open_trapezoid_1.top_left.x,
                        open_trapezoid_1.top_right.x);
                trapezoids.push_back(trapezoid_1);
            }

            LengthDbl x_right = x(open_trapezoid_2.bottom_right, open_trapezoid_2.top_right, vertex.y);
            if (vertex.y != open_trapezoid_2.top_left.y) {
                GeneralizedTrapezoid trapezoid_2(
                        vertex.y,
                        open_trapezoid_2.top_left.y,
                        vertex.x,
                        x_right,
                        open_trapezoid_2.top_left.x,
                        open_trapezoid_2.top_right.x);
                trapezoids.push_back(trapezoid_2);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = {x_left, vertex.y};
            new_open_trapezoid.top_right = {x_right, vertex.y};
            new_open_trapezoid.bottom_left = open_trapezoid_1.bottom_left;
            new_open_trapezoid.bottom_right = open_trapezoid_2.bottom_right;
            open_trapezoids.push_back(new_open_trapezoid);

            open_trapezoids[p.first] = open_trapezoids.back();
            open_trapezoids.pop_back();
            open_trapezoids[p.second] = open_trapezoids.back();
            open_trapezoids.pop_back();

        } else if (vertices[element_pos].flag == VertexTypeFlag::Inflection) {
            // -1 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "Inflection" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            //std::cout << "open_trapezoid_pos " << open_trapezoid_pos << std::endl;
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];
            //std::cout << "open_trapezoid"
            //    << " bl " << open_trapezoid.bottom_left.x << " " << open_trapezoid.bottom_left.y
            //    << " br " << open_trapezoid.bottom_right.x << " " << open_trapezoid.bottom_right.y
            //    << " tl " << open_trapezoid.top_left.x << " " << open_trapezoid.top_left.y
            //    << " tr " << open_trapezoid.top_right.x << " " << open_trapezoid.top_right.y
            //    << std::endl;

            // Update trapezoids.
            LengthDbl x_left = x(open_trapezoid.bottom_left, open_trapezoid.top_left, vertex.y);
            LengthDbl x_right = x(open_trapezoid.bottom_right, open_trapezoid.top_right, vertex.y);
            if (vertex.y != open_trapezoid.top_left.y) {
                // Open trapezoids with null height might be created.
                // If that happens, don't add them to the trapezoids.
                GeneralizedTrapezoid trapezoid(
                        vertex.y,
                        open_trapezoid.top_left.y,
                        x_left,
                        x_right,
                        open_trapezoid.top_left.x,
                        open_trapezoid.top_right.x);
                trapezoids.push_back(trapezoid);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = {x_left, vertex.y};
            new_open_trapezoid.top_right = {x_right, vertex.y};
            if (vertex == open_trapezoid.bottom_left) {
                new_open_trapezoid.bottom_left = get_vertex(shape, element_pos + 1);
                new_open_trapezoid.bottom_right = open_trapezoid.bottom_right;
            } else {
                new_open_trapezoid.bottom_left = open_trapezoid.bottom_left;
                new_open_trapezoid.bottom_right = get_vertex(shape, element_pos - 1);
            }
            open_trapezoids.push_back(new_open_trapezoid);

            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();


        } else if (vertices[element_pos].flag == VertexTypeFlag::HorizontalLocalMaximumConvex
                && vertices[element_pos_next].flag == VertexTypeFlag::HorizontalLocalMaximumConvex) {
            // +1 open trapezoid.
            //std::cout << "HorizontalLocalMaximumConvex HorizontalLocalMaximumConvex" << std::endl;

            // Update open_trapezoids.
            OpenTrapezoid open_trapezoid;
            open_trapezoid.top_left = get_vertex(shape, element_pos);
            open_trapezoid.top_right = get_vertex(shape, element_pos - 1);
            open_trapezoid.bottom_left = get_vertex(shape, element_pos + 1);
            open_trapezoid.bottom_right = get_vertex(shape, element_pos - 2);
            open_trapezoids.push_back(open_trapezoid);

            vertex_pos++;

        } else if (vertices[element_pos].flag == VertexTypeFlag::HorizontalLocalMinimumConvex
                && vertices[element_pos_next].flag == VertexTypeFlag::HorizontalLocalMinimumConvex) {
            // -1 open trapezoid.
            //std::cout << "HorizontalLocalMinimumConvex HorizontalLocalMinimumConvex" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            GeneralizedTrapezoid trapezoid(
                    vertex.y,
                    open_trapezoid.top_left.y,
                    get_vertex(shape, element_pos).x,
                    get_vertex(shape, element_pos + 1).x,
                    open_trapezoid.top_left.x,
                    open_trapezoid.top_right.x);
            trapezoids.push_back(trapezoid);

            // Update open_trapezoids.
            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[element_pos].flag == VertexTypeFlag::HorizontalLocalMaximumConcave
                && vertices[element_pos_next].flag == VertexTypeFlag::HorizontalLocalMaximumConcave) {
            // -1 open trapezoid.
            // +2 open trapezoids.
            //std::cout << "HorizontalLocalMaximumConcave HorizontalLocalMaximumConcave" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];
            //std::cout << "open trapezoid " << open_trapezoid << std::endl;

            // Update trapezoids.
            LengthDbl x_left = x(open_trapezoid.bottom_left, open_trapezoid.top_left, vertex.y);
            LengthDbl x_right = x(open_trapezoid.bottom_right, open_trapezoid.top_right, vertex.y);
            if (vertex.y != open_trapezoid.top_left.y) {
                GeneralizedTrapezoid trapezoid(
                        vertex.y,
                        open_trapezoid.top_left.y,
                        x_left,
                        x_right,
                        open_trapezoid.top_left.x,
                        open_trapezoid.top_right.x);
                trapezoids.push_back(trapezoid);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid_1;
            new_open_trapezoid_1.top_left = {x_left, vertex.y};
            new_open_trapezoid_1.top_right = vertex;
            new_open_trapezoid_1.bottom_left = open_trapezoid.bottom_left;
            new_open_trapezoid_1.bottom_right = get_vertex(shape, element_pos - 1);
            open_trapezoids.push_back(new_open_trapezoid_1);

            OpenTrapezoid new_open_trapezoid_2;
            new_open_trapezoid_2.top_left = get_vertex(shape, element_pos + 1);
            new_open_trapezoid_2.top_right = {x_right, vertex.y};
            new_open_trapezoid_2.bottom_left = get_vertex(shape, element_pos + 2);
            new_open_trapezoid_2.bottom_right = open_trapezoid.bottom_right;
            open_trapezoids.push_back(new_open_trapezoid_2);

            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[element_pos].flag == VertexTypeFlag::HorizontalLocalMinimumConcave
                && vertices[element_pos_next].flag == VertexTypeFlag::HorizontalLocalMinimumConcave) {
            // -2 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "HorizontalLocalMinimumConcave HorizontalLocalMinimumConcave" << std::endl;

            ElementPos open_trapezoid_1_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            ElementPos open_trapezoid_2_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex_next).first;
            //std::cout << "open_trapezoid_1_pos " << open_trapezoid_1_pos << " open_trapezoid_2_pos " << open_trapezoid_2_pos << std::endl;
            const OpenTrapezoid& open_trapezoid_1 = open_trapezoids[open_trapezoid_1_pos];
            const OpenTrapezoid& open_trapezoid_2 = open_trapezoids[open_trapezoid_2_pos];

            // Update trapezoids.
            LengthDbl x_left = x(open_trapezoid_1.bottom_left, open_trapezoid_1.top_left, vertex.y);
            if (vertex.y != open_trapezoid_1.top_left.y) {
                GeneralizedTrapezoid trapezoid_1(
                        vertex.y,
                        open_trapezoid_1.top_left.y,
                        x_left,
                        vertex.x,
                        open_trapezoid_1.top_left.x,
                        open_trapezoid_1.top_right.x);
                trapezoids.push_back(trapezoid_1);
            }

            LengthDbl x_right = x(open_trapezoid_2.bottom_right, open_trapezoid_2.top_right, vertex.y);
            if (vertex.y != open_trapezoid_2.top_left.y) {
                GeneralizedTrapezoid trapezoid_2(
                        vertex.y,
                        open_trapezoid_2.top_left.y,
                        get_vertex(shape, element_pos - 1).x,
                        x_right,
                        open_trapezoid_2.top_left.x,
                        open_trapezoid_2.top_right.x);
                trapezoids.push_back(trapezoid_2);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = {x_left, vertex.y};
            new_open_trapezoid.top_right = {x_right, vertex.y};
            new_open_trapezoid.bottom_left = open_trapezoid_1.bottom_left;
            new_open_trapezoid.bottom_right = open_trapezoid_2.bottom_right;
            open_trapezoids.push_back(new_open_trapezoid);

            open_trapezoids[open_trapezoid_1_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();
            open_trapezoids[open_trapezoid_2_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[element_pos].flag == VertexTypeFlag::HorizontalLocalMaximumConvex
                && vertices[element_pos_next].flag == VertexTypeFlag::HorizontalLocalMinimumConcave) {
            // -1 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "HorizontalLocalMaximumConvex HorizontalLocalMinimumConcave" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex_next).first;
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            LengthDbl x_right = x(open_trapezoid.bottom_right, open_trapezoid.top_right, vertex.y);
            GeneralizedTrapezoid trapezoid(
                    vertex.y,
                    open_trapezoid.top_left.y,
                    get_vertex(shape, element_pos - 1).x,
                    x_right,
                    open_trapezoid.top_left.x,
                    open_trapezoid.top_right.x);
            trapezoids.push_back(trapezoid);

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = vertex;
            new_open_trapezoid.top_right = {x_right, vertex.y};
            new_open_trapezoid.bottom_left = get_vertex(shape, element_pos + 1);
            new_open_trapezoid.bottom_right = open_trapezoid.bottom_right;
            open_trapezoids.push_back(new_open_trapezoid);

            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[element_pos].flag == VertexTypeFlag::HorizontalLocalMinimumConvex
                && vertices[element_pos_next].flag == VertexTypeFlag::HorizontalLocalMaximumConcave) {
            // -1 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "HorizontalLocalMinimumConvex HorizontalLocalMaximumConcave" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            LengthDbl x_right = x(open_trapezoid.bottom_right, open_trapezoid.top_right, vertex.y);
            GeneralizedTrapezoid trapezoid(
                    vertex.y,
                    open_trapezoid.top_left.y,
                    vertex.x,
                    x_right,
                    open_trapezoid.top_left.x,
                    open_trapezoid.top_right.x);
            trapezoids.push_back(trapezoid);

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = get_vertex(shape, element_pos + 1);
            new_open_trapezoid.top_right = {x_right, vertex.y};
            new_open_trapezoid.bottom_left = get_vertex(shape, element_pos + 2);
            new_open_trapezoid.bottom_right = open_trapezoid.bottom_right;
            open_trapezoids.push_back(new_open_trapezoid);

            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[element_pos].flag == VertexTypeFlag::HorizontalLocalMaximumConcave
                && vertices[element_pos_next].flag == VertexTypeFlag::HorizontalLocalMinimumConvex) {
            // -1 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "HorizontalLocalMaximumConcave HorizontalLocalMinimumConvex" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex_next).first;
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            LengthDbl x_left = x(open_trapezoid.bottom_left, open_trapezoid.top_left, vertex.y);
            if (vertex.y != open_trapezoid.top_left.y) {
                GeneralizedTrapezoid trapezoid(
                        vertex.y,
                        open_trapezoid.top_left.y,
                        x_left,
                        get_vertex(shape, element_pos + 1).x,
                        open_trapezoid.top_left.x,
                        open_trapezoid.top_right.x);
                trapezoids.push_back(trapezoid);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = {x_left, vertex.y};
            new_open_trapezoid.top_right = vertex;
            new_open_trapezoid.bottom_left = open_trapezoid.bottom_left;
            new_open_trapezoid.bottom_right = get_vertex(shape, element_pos - 1);
            open_trapezoids.push_back(new_open_trapezoid);

            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[element_pos].flag == VertexTypeFlag::HorizontalLocalMinimumConcave
                && vertices[element_pos_next].flag == VertexTypeFlag::HorizontalLocalMaximumConvex) {
            // -1 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "HorizontalLocalMinimumConcave HorizontalLocalMaximumConvex" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            LengthDbl x_left = x(open_trapezoid.bottom_left, open_trapezoid.top_left, vertex.y);
            if (vertex.y != open_trapezoid.top_left.y) {
                GeneralizedTrapezoid trapezoid(
                        vertex.y,
                        open_trapezoid.top_left.y,
                        x_left,
                        vertex.x,
                        open_trapezoid.top_left.x,
                        open_trapezoid.top_right.x);
                trapezoids.push_back(trapezoid);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = {x_left, vertex.y};
            new_open_trapezoid.top_right = get_vertex(shape, element_pos - 1);
            new_open_trapezoid.bottom_left = open_trapezoid.bottom_left;
            new_open_trapezoid.bottom_right = get_vertex(shape, element_pos - 2);
            open_trapezoids.push_back(new_open_trapezoid);

            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else {
            throw std::runtime_error("polygon_trapezoidation");

        }
        //std::cout << std::endl;
    }

    //std::cout << "polygon_trapezoidation end" << std::endl;
    return trapezoids;
}

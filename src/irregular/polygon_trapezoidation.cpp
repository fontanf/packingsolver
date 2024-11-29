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
    if (bottom.y == top.y) {
        if (bottom.x != top.x) {
            throw std::runtime_error("x");
        }
        return bottom.x;
    }
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
        //std::cout << "open_trapezoid " << open_trapezoid << std::endl;

        LengthDbl x_left = x(open_trapezoid.bottom_left, open_trapezoid.top_left, vertex.y);
        LengthDbl x_right = x(open_trapezoid.bottom_right, open_trapezoid.top_right, vertex.y);
        //std::cout << "x_left " << x_left << " x " << vertex.x << " x_right " << x_right << std::endl;
        if (!strictly_greater(x_left, vertex.x)
                && !strictly_lesser(x_right, vertex.x)) {
            if (res.first == -1) {
                res.first = open_trapezoid_pos;
                x_left_1 = x_left;
            } else {
                if (strictly_lesser(x_left_1, x_left)) {
                    res.second = open_trapezoid_pos;
                } else {
                    res.second = res.first;
                    res.first = open_trapezoid_pos;
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
        const Shape& shape,
        const std::vector<Shape>& holes)
{
    //std::cout << "polygon_trapezoidation" << std::endl;
    //std::cout << shape.to_string(0) << std::endl;
    std::vector<GeneralizedTrapezoid> trapezoids;

    // Sort vertices according to their y coordinate.
    std::vector<std::pair<ShapePos, ElementPos>> sorted_vertices;
    // Add holes.
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)holes.size();
            ++hole_pos) {
        const Shape& hole = holes[hole_pos];
        for (ElementPos element_pos = 0;
                element_pos < (ElementPos)hole.elements.size();
                ++element_pos) {
            sorted_vertices.push_back({hole_pos, element_pos});
        }
    }
    // Add shape.
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape.elements.size();
            ++element_pos) {
        sorted_vertices.push_back({holes.size(), element_pos});
    }
    // Sort.
    std::sort(
            sorted_vertices.begin(),
            sorted_vertices.end(),
            [&shape, &holes](
                const std::pair<ShapePos, ElementPos>& p1,
                const std::pair<ShapePos, ElementPos>& p2)
            {
                const Shape& shape_1 = (p1.first == holes.size())? shape: holes[p1.first];
                const Shape& shape_2 = (p2.first == holes.size())? shape: holes[p2.first];
                ElementPos element_pos_1 = p1.second;
                ElementPos element_pos_2 = p2.second;
                if (shape_1.elements[element_pos_1].start.y
                        != shape_2.elements[element_pos_2].start.y) {
                    return shape_1.elements[element_pos_1].start.y
                        > shape_2.elements[element_pos_2].start.y;
                }
                return shape_1.elements[element_pos_1].start.x
                    < shape_2.elements[element_pos_2].start.x;
            });

    // Classify the vertices.
    std::vector<std::vector<Vertex>> vertices;
    for (ShapePos shape_pos = 0;
            shape_pos <= (ShapePos)holes.size();
            ++shape_pos) {
        const Shape& current_shape = (shape_pos == (ShapePos)holes.size())? shape: holes[shape_pos];
        vertices.push_back(std::vector<Vertex>(current_shape.elements.size()));

        ElementPos element_pos_prev = current_shape.elements.size() - 2;
        ElementPos element_pos_cur = current_shape.elements.size() - 1;
        for (ElementPos element_pos_next = 0;
                element_pos_next < current_shape.elements.size();
                ++element_pos_next) {
            const ShapeElement& element_prev = current_shape.elements[element_pos_prev];
            const ShapeElement& element_cur = current_shape.elements[element_pos_cur];
            const ShapeElement& element_next = current_shape.elements[element_pos_next];

            // Convexity.
            // The convexity can be determined easily by investigating the sign of the
            // cross product of the edges meeting at the considered vertex.
            double v = cross_product(
                    element_cur.start - element_prev.start,
                    element_next.start - element_cur.start);
            bool is_convex = (v >= 0);
            if (shape_pos != holes.size())
                is_convex = !is_convex;

            // Local extreme of the vertices.
            if (element_prev.start.y < element_cur.start.y
                    && element_cur.start.y < element_next.start.y) {
                vertices[shape_pos][element_pos_cur].flag = VertexTypeFlag::Inflection;
            } else if (element_prev.start.y > element_cur.start.y
                    && element_cur.start.y > element_next.start.y) {
                vertices[shape_pos][element_pos_cur].flag = VertexTypeFlag::Inflection;
            } else if (element_cur.start.y < element_prev.start.y
                    && element_cur.start.y < element_next.start.y) {
                vertices[shape_pos][element_pos_cur].flag = (is_convex)?
                    VertexTypeFlag::LocalMinimumConvex:
                    VertexTypeFlag::LocalMinimumConcave;
            } else if (element_cur.start.y > element_prev.start.y
                    && element_cur.start.y > element_next.start.y) {
                vertices[shape_pos][element_pos_cur].flag = (is_convex)?
                    VertexTypeFlag::LocalMaximumConvex:
                    VertexTypeFlag::LocalMaximumConcave;
            } else if (element_cur.start.y == element_prev.start.y
                    && element_cur.start.y < element_next.start.y) {
                vertices[shape_pos][element_pos_cur].flag = (is_convex)?
                    VertexTypeFlag::HorizontalLocalMinimumConvex:
                    VertexTypeFlag::HorizontalLocalMinimumConcave;
            } else if (element_cur.start.y < element_prev.start.y
                    && element_cur.start.y == element_next.start.y) {
                vertices[shape_pos][element_pos_cur].flag = (is_convex)?
                    VertexTypeFlag::HorizontalLocalMinimumConvex:
                    VertexTypeFlag::HorizontalLocalMinimumConcave;
            } else if (element_cur.start.y == element_prev.start.y
                    && element_cur.start.y > element_next.start.y) {
                vertices[shape_pos][element_pos_cur].flag = (is_convex)?
                    VertexTypeFlag::HorizontalLocalMaximumConvex:
                    VertexTypeFlag::HorizontalLocalMaximumConcave;
            } else if (element_cur.start.y > element_prev.start.y
                    && element_cur.start.y == element_next.start.y) {
                vertices[shape_pos][element_pos_cur].flag = (is_convex)?
                    VertexTypeFlag::HorizontalLocalMaximumConvex:
                    VertexTypeFlag::HorizontalLocalMaximumConcave;
            } else if (element_cur.start.y == element_prev.start.y
                    && element_cur.start.y == element_next.start.y) {
                vertices[shape_pos][element_pos_cur].flag = VertexTypeFlag::StrictlyHorizontal;
            }

            element_pos_prev = element_pos_cur;
            element_pos_cur = element_pos_next;
        }
    }

    // Sweep.
    std::vector<OpenTrapezoid> open_trapezoids;
    for (ElementPos vertex_pos = 0;
            vertex_pos < (ElementPos)sorted_vertices.size();
            ++vertex_pos) {

        ShapePos shape_pos = sorted_vertices[vertex_pos].first;
        ElementPos element_pos = sorted_vertices[vertex_pos].second;

        ShapePos shape_pos_next = sorted_vertices[(vertex_pos + 1) % sorted_vertices.size()].first;
        ElementPos element_pos_next = sorted_vertices[(vertex_pos + 1) % sorted_vertices.size()].second;

        const Shape& current_shape = (shape_pos == (ShapePos)holes.size())? shape: holes[shape_pos];
        const Shape& current_shape_next = (shape_pos_next == (ShapePos)holes.size())? shape: holes[shape_pos_next];

        const Point& vertex = current_shape.elements[element_pos].start;
        const Point& vertex_next = current_shape_next.elements[element_pos_next].start;

        //std::cout << "vertex_pos " << vertex_pos << std::endl;
        //std::cout << "shape_pos " << shape_pos << " element_pos " << element_pos << std::endl;
        //std::cout << "shape_pos_next " << shape_pos_next << " element_pos_next " << element_pos_next << std::endl;
        //std::cout << "vertex " << vertex.x << " " << vertex.y << std::endl;
        //std::cout << "vertex_next " << vertex_next.x << " " << vertex_next.y << std::endl;
        //std::cout << "open trapezoids:" << std::endl;
        //for (const OpenTrapezoid& open_trapezoid: open_trapezoids)
        //    std::cout << open_trapezoid << std::endl;
        if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::LocalMaximumConvex) {
            // +1 open trapezoid.
            //std::cout << "LocalMaximumConvex" << std::endl;

            // Update open_trapezoids.
            OpenTrapezoid open_trapezoid;
            open_trapezoid.top_left = vertex;
            open_trapezoid.top_right = vertex;
            open_trapezoid.bottom_left = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos + 1):
                get_vertex(current_shape, element_pos - 1);
            open_trapezoid.bottom_right = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos - 1):
                get_vertex(current_shape, element_pos + 1);
            ElementPos open_trapezoid_pos = open_trapezoids.size();

            open_trapezoids.push_back(open_trapezoid);

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::LocalMinimumConvex) {
            // -1 open trapezoid.
            //std::cout << "LocalMinimumConvex" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            if (open_trapezoid_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_pos' must be != -1.");
            }
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            GeneralizedTrapezoid trapezoid(
                    vertex.y,
                    open_trapezoid.top_left.y,
                    vertex.x,
                    vertex.x,
                    open_trapezoid.top_left.x,
                    open_trapezoid.top_right.x);
            //std::cout << "new trapezoid " << trapezoid << std::endl;
            trapezoids.push_back(trapezoid);

            // Update open_trapezoids.
            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::LocalMaximumConcave) {
            // -1 open trapezoid.
            // +2 open trapezoids.
            //std::cout << "LocalMaximumConcave" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            if (open_trapezoid_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_pos' must be != -1.");
            }
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
                //std::cout << "new trapezoid " << trapezoid << std::endl;
                trapezoids.push_back(trapezoid);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid_1;
            new_open_trapezoid_1.top_left = {x_left, vertex.y};
            new_open_trapezoid_1.top_right = vertex;
            new_open_trapezoid_1.bottom_left = open_trapezoid.bottom_left;
            new_open_trapezoid_1.bottom_right = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos - 1):
                get_vertex(current_shape, element_pos + 1);

            OpenTrapezoid new_open_trapezoid_2;
            new_open_trapezoid_2.top_left = vertex;
            new_open_trapezoid_2.top_right = {x_right, vertex.y};
            new_open_trapezoid_2.bottom_left = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos + 1):
                get_vertex(current_shape, element_pos - 1);
            new_open_trapezoid_2.bottom_right = open_trapezoid.bottom_right;

            open_trapezoids.push_back(new_open_trapezoid_1);
            open_trapezoids.push_back(new_open_trapezoid_2);
            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::LocalMinimumConcave) {
            // -2 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "LocalMinimumConcave" << std::endl;

            auto p = find_trapezoid_containing_vertex(open_trapezoids, vertex);
            if (p.first == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'p.first' must be != -1.");
            }
            if (p.second == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'p.second' must be != -1.");
            }
            const OpenTrapezoid& open_trapezoid_1 = open_trapezoids[p.first];
            const OpenTrapezoid& open_trapezoid_2 = open_trapezoids[p.second];

            // Update trapezoids.
            LengthDbl x_left = x(open_trapezoid_1.bottom_left, open_trapezoid_1.top_left, vertex.y);
            if (vertex.y != open_trapezoid_1.top_left.y) {
                GeneralizedTrapezoid trapezoid(
                        vertex.y,
                        open_trapezoid_1.top_left.y,
                        x_left,
                        vertex.x,
                        open_trapezoid_1.top_left.x,
                        open_trapezoid_1.top_right.x);
                //std::cout << "new trapezoid " << trapezoid << std::endl;
                trapezoids.push_back(trapezoid);
            }

            LengthDbl x_right = x(open_trapezoid_2.bottom_right, open_trapezoid_2.top_right, vertex.y);
            if (vertex.y != open_trapezoid_2.top_left.y) {
                GeneralizedTrapezoid trapezoid(
                        vertex.y,
                        open_trapezoid_2.top_left.y,
                        vertex.x,
                        x_right,
                        open_trapezoid_2.top_left.x,
                        open_trapezoid_2.top_right.x);
                //std::cout << "new trapezoid " << trapezoid << std::endl;
                trapezoids.push_back(trapezoid);
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

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::Inflection) {
            // -1 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "Inflection" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            if (open_trapezoid_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_pos' must be != -1.");
            }
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
                //std::cout << "new trapezoid " << trapezoid << std::endl;
                trapezoids.push_back(trapezoid);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = {x_left, vertex.y};
            new_open_trapezoid.top_right = {x_right, vertex.y};
            if (vertex == open_trapezoid.bottom_left) {
                new_open_trapezoid.bottom_left = (shape_pos == holes.size())?
                    get_vertex(current_shape, element_pos + 1):
                    get_vertex(current_shape, element_pos - 1);
                new_open_trapezoid.bottom_right = open_trapezoid.bottom_right;
            } else {
                new_open_trapezoid.bottom_left = open_trapezoid.bottom_left;
                new_open_trapezoid.bottom_right = (shape_pos == holes.size())?
                    get_vertex(current_shape, element_pos - 1):
                    get_vertex(current_shape, element_pos + 1);
            }
            open_trapezoids.push_back(new_open_trapezoid);

            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();


        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::HorizontalLocalMaximumConvex
                && vertices[shape_pos_next][element_pos_next].flag == VertexTypeFlag::HorizontalLocalMaximumConvex) {
            // +1 open trapezoid.
            //std::cout << "HorizontalLocalMaximumConvex HorizontalLocalMaximumConvex" << std::endl;

            // Update open_trapezoids.
            OpenTrapezoid open_trapezoid;
            open_trapezoid.top_left = vertex;
            open_trapezoid.top_right = vertex_next;
            open_trapezoid.bottom_left = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos + 1):
                get_vertex(current_shape, element_pos - 1);
            open_trapezoid.bottom_right = (shape_pos_next == holes.size())?
                get_vertex(current_shape_next, element_pos_next - 1):
                get_vertex(current_shape_next, element_pos_next + 1);

            open_trapezoids.push_back(open_trapezoid);

            vertex_pos++;

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::HorizontalLocalMinimumConvex
                && vertices[shape_pos_next][element_pos_next].flag == VertexTypeFlag::HorizontalLocalMinimumConvex) {
            // -1 open trapezoid.
            //std::cout << "HorizontalLocalMinimumConvex HorizontalLocalMinimumConvex" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            if (open_trapezoid_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_pos' must be != -1.");
            }
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            LengthDbl x_right = ((shape_pos == holes.size())?
                get_vertex(current_shape, element_pos + 1):
                get_vertex(current_shape, element_pos - 1)).x;
            GeneralizedTrapezoid trapezoid(
                    vertex.y,
                    open_trapezoid.top_left.y,
                    vertex.x,
                    x_right,
                    open_trapezoid.top_left.x,
                    open_trapezoid.top_right.x);
            //std::cout << "new trapezoid " << trapezoid << std::endl;
            trapezoids.push_back(trapezoid);

            // Update open_trapezoids.
            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::HorizontalLocalMaximumConcave
                && vertices[shape_pos_next][element_pos_next].flag == VertexTypeFlag::HorizontalLocalMaximumConcave) {
            // -1 open trapezoid.
            // +2 open trapezoids.
            //std::cout << "HorizontalLocalMaximumConcave HorizontalLocalMaximumConcave" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            if (open_trapezoid_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_pos' must be != -1.");
            }
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
                //std::cout << "new trapezoid " << trapezoid << std::endl;
                trapezoids.push_back(trapezoid);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid_1;
            new_open_trapezoid_1.top_left = {x_left, vertex.y};
            new_open_trapezoid_1.top_right = vertex;
            new_open_trapezoid_1.bottom_left = open_trapezoid.bottom_left;
            new_open_trapezoid_1.bottom_right = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos - 1):
                get_vertex(current_shape, element_pos + 1);

            OpenTrapezoid new_open_trapezoid_2;
            new_open_trapezoid_2.top_left = vertex_next;
            new_open_trapezoid_2.top_right = {x_right, vertex.y};
            new_open_trapezoid_2.bottom_left = (shape_pos_next == holes.size())?
                get_vertex(current_shape_next, element_pos_next + 1):
                get_vertex(current_shape_next, element_pos_next - 1);
            new_open_trapezoid_2.bottom_right = open_trapezoid.bottom_right;

            open_trapezoids.push_back(new_open_trapezoid_1);
            open_trapezoids.push_back(new_open_trapezoid_2);
            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::HorizontalLocalMinimumConcave
                && vertices[shape_pos_next][element_pos_next].flag == VertexTypeFlag::HorizontalLocalMinimumConcave) {
            // -2 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "HorizontalLocalMinimumConcave HorizontalLocalMinimumConcave" << std::endl;

            ElementPos open_trapezoid_1_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            if (open_trapezoid_1_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_1_pos' must be != -1.");
            }
            auto p = find_trapezoid_containing_vertex(open_trapezoids, vertex_next);
            ElementPos open_trapezoid_2_pos = (p.second != -1)? p.second: p.first;
            if (open_trapezoid_2_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_2_pos' must be != -1.");
            }
            //std::cout << "open_trapezoid_1_pos " << open_trapezoid_1_pos << " open_trapezoid_2_pos " << open_trapezoid_2_pos << std::endl;
            const OpenTrapezoid& open_trapezoid_1 = open_trapezoids[open_trapezoid_1_pos];
            const OpenTrapezoid& open_trapezoid_2 = open_trapezoids[open_trapezoid_2_pos];

            // Update trapezoids.
            LengthDbl x_left = x(open_trapezoid_1.bottom_left, open_trapezoid_1.top_left, vertex.y);
            if (vertex.y != open_trapezoid_1.top_left.y) {
                GeneralizedTrapezoid trapezoid(
                        vertex.y,
                        open_trapezoid_1.top_left.y,
                        x_left,
                        vertex.x,
                        open_trapezoid_1.top_left.x,
                        open_trapezoid_1.top_right.x);
                //std::cout << "new trapezoid " << trapezoid << std::endl;
                trapezoids.push_back(trapezoid);
            }

            LengthDbl x_right = x(open_trapezoid_2.bottom_right, open_trapezoid_2.top_right, vertex.y);
            if (vertex.y != open_trapezoid_2.top_left.y) {
                GeneralizedTrapezoid trapezoid(
                        vertex.y,
                        open_trapezoid_2.top_left.y,
                        vertex_next.x,
                        x_right,
                        open_trapezoid_2.top_left.x,
                        open_trapezoid_2.top_right.x);
                //std::cout << "new trapezoid " << trapezoid << std::endl;
                trapezoids.push_back(trapezoid);
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

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::HorizontalLocalMaximumConvex
                && vertices[shape_pos_next][element_pos_next].flag == VertexTypeFlag::HorizontalLocalMinimumConcave) {
            // -1 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "HorizontalLocalMaximumConvex HorizontalLocalMinimumConcave" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex_next).first;
            if (open_trapezoid_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_pos' must be != -1.");
            }
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            LengthDbl x_right = x(open_trapezoid.bottom_right, open_trapezoid.top_right, vertex.y);
            LengthDbl x_left = ((shape_pos == holes.size())?
                get_vertex(current_shape, element_pos - 1):
                get_vertex(current_shape, element_pos + 1)).x;
            GeneralizedTrapezoid trapezoid(
                    vertex.y,
                    open_trapezoid.top_left.y,
                    x_left,
                    x_right,
                    open_trapezoid.top_left.x,
                    open_trapezoid.top_right.x);
            //std::cout << "new trapezoid " << trapezoid << std::endl;
            trapezoids.push_back(trapezoid);

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = vertex;
            new_open_trapezoid.top_right = {x_right, vertex.y};
            new_open_trapezoid.bottom_left = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos + 1):
                get_vertex(current_shape, element_pos - 1);
            new_open_trapezoid.bottom_right = open_trapezoid.bottom_right;

            open_trapezoids.push_back(new_open_trapezoid);
            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::HorizontalLocalMinimumConvex
                && vertices[shape_pos_next][element_pos_next].flag == VertexTypeFlag::HorizontalLocalMaximumConcave) {
            // -1 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "HorizontalLocalMinimumConvex HorizontalLocalMaximumConcave" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            if (open_trapezoid_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_pos' must be != -1.");
            }
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
            //std::cout << "new trapezoid " << trapezoid << std::endl;
            trapezoids.push_back(trapezoid);

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos + 1):
                get_vertex(current_shape, element_pos - 1);
            new_open_trapezoid.top_right = {x_right, vertex.y};
            new_open_trapezoid.bottom_left = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos + 2):
                get_vertex(current_shape, element_pos - 2);
            new_open_trapezoid.bottom_right = open_trapezoid.bottom_right;

            open_trapezoids.push_back(new_open_trapezoid);
            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::HorizontalLocalMaximumConcave
                && vertices[shape_pos_next][element_pos_next].flag == VertexTypeFlag::HorizontalLocalMinimumConvex) {
            // -1 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "HorizontalLocalMaximumConcave HorizontalLocalMinimumConvex" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex_next).first;
            if (open_trapezoid_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_pos' must be != -1.");
            }
            const OpenTrapezoid& open_trapezoid = open_trapezoids[open_trapezoid_pos];

            // Update trapezoids.
            LengthDbl x_left = x(open_trapezoid.bottom_left, open_trapezoid.top_left, vertex.y);
            if (vertex.y != open_trapezoid.top_left.y) {
                LengthDbl x_right = ((shape_pos == holes.size())?
                        get_vertex(current_shape, element_pos + 1):
                        get_vertex(current_shape, element_pos - 1)).x;
                GeneralizedTrapezoid trapezoid(
                        vertex.y,
                        open_trapezoid.top_left.y,
                        x_left,
                        x_right,
                        open_trapezoid.top_left.x,
                        open_trapezoid.top_right.x);
                //std::cout << "new trapezoid " << trapezoid << std::endl;
                trapezoids.push_back(trapezoid);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = {x_left, vertex.y};
            new_open_trapezoid.top_right = vertex;
            new_open_trapezoid.bottom_left = open_trapezoid.bottom_left;
            new_open_trapezoid.bottom_right = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos - 1):
                get_vertex(current_shape, element_pos + 1);

            open_trapezoids.push_back(new_open_trapezoid);
            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else if (vertices[shape_pos][element_pos].flag == VertexTypeFlag::HorizontalLocalMinimumConcave
                && vertices[shape_pos_next][element_pos_next].flag == VertexTypeFlag::HorizontalLocalMaximumConvex) {
            // -1 open trapezoid.
            // +1 open trapezoids.
            //std::cout << "HorizontalLocalMinimumConcave HorizontalLocalMaximumConvex" << std::endl;

            ElementPos open_trapezoid_pos = find_trapezoid_containing_vertex(open_trapezoids, vertex).first;
            if (open_trapezoid_pos == -1) {
                throw std::runtime_error(
                        "irregular::polygon_trapezoidation."
                        " 'open_trapezoid_pos' must be != -1.");
            }
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
                //std::cout << "new trapezoid " << trapezoid << std::endl;
                trapezoids.push_back(trapezoid);
            }

            // Update open_trapezoids.

            OpenTrapezoid new_open_trapezoid;
            new_open_trapezoid.top_left = {x_left, vertex.y};
            new_open_trapezoid.top_right = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos - 1):
                get_vertex(current_shape, element_pos + 1);
            new_open_trapezoid.bottom_left = open_trapezoid.bottom_left;
            new_open_trapezoid.bottom_right = (shape_pos == holes.size())?
                get_vertex(current_shape, element_pos - 2):
                get_vertex(current_shape, element_pos + 2);

            open_trapezoids.push_back(new_open_trapezoid);
            open_trapezoids[open_trapezoid_pos] = open_trapezoids.back();
            open_trapezoids.pop_back();

            vertex_pos++;

        } else {
            throw std::runtime_error(
                    "irregular::polygon_trapezoidation."
                    " flag: " + std::to_string((int)vertices[shape_pos][element_pos].flag)
                    + "; flag_next: " + std::to_string((int)vertices[shape_pos_next][element_pos_next].flag)
                    + ".");

        }
        //std::cout << std::endl;
    }

    // Check area.
    AreaDbl shape_area = shape.compute_area();
    for (const Shape& hole: holes)
        shape_area -= hole.compute_area();
    AreaDbl trapezoidation_area = 0.0;
    for (const GeneralizedTrapezoid& trapezoid: trapezoids)
        trapezoidation_area += trapezoid.area();
    if (!equal(shape_area, trapezoidation_area)) {
        throw std::runtime_error(
                "polygon_trapezoidation."
                " shape_area: " + std::to_string(shape_area)
                + "; trapezoidation_area: " + std::to_string(trapezoidation_area)
                + ".");
    }

    //std::cout << "polygon_trapezoidation end" << std::endl;
    return trapezoids;
}

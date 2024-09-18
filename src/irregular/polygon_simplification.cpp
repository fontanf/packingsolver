#include "irregular/polygon_simplification.hpp"

#include "CDT.h"

using namespace packingsolver;
using namespace packingsolver::irregular;

AreaDbl area(
        const std::vector<CDT::V2d<LengthDbl>>& cdt_vertices,
        const CDT::Triangle& cdt_triangle)
{
    const CDT::V2d<LengthDbl> a = cdt_vertices[cdt_triangle.vertices[0]];
    const CDT::V2d<LengthDbl> b = cdt_vertices[cdt_triangle.vertices[1]];
    const CDT::V2d<LengthDbl> c = cdt_vertices[cdt_triangle.vertices[2]];
    return 0.5 * std::fabs(
            (a.x - c.x) * (b.y - a.y)
            - (a.x - b.x) - (c.y - a.y));
}

Shape irregular::polygon_simplification(
        const Shape& shape)
{
    // Build CDT input.
    CDT::Triangulation<double> cdt;
    std::vector<CDT::V2d<LengthDbl>> cdt_vertices;
    std::vector<CDT::Edge> cdt_edges;
    ElementPos element_prev_pos = shape.elements.size() - 1;
    for (ElementPos element_cur_pos = 0;
            element_cur_pos < (ElementPos)shape.elements.size();
            ++element_cur_pos) {
        const ShapeElement& element_prev = shape.elements[element_prev_pos];
        const ShapeElement& element_cur = shape.elements[element_cur_pos];

        CDT::V2d<LengthDbl> cdt_vertex_prev;
        cdt_vertex_prev.x = element_prev.start.x;
        cdt_vertex_prev.y = element_prev.start.y;

        CDT::V2d<LengthDbl> cdt_vertex_cur;
        cdt_vertex_cur.x = element_cur.start.x;
        cdt_vertex_cur.y = element_cur.start.y;

        cdt_vertices.push_back(cdt_vertex_cur);
        cdt_edges.push_back(CDT::Edge(
                    element_prev_pos,
                    element_cur_pos));

        element_prev_pos = element_cur_pos;
    }

    cdt.insertVertices(cdt_vertices);
    cdt.insertEdges(cdt_edges);
    //cdt.eraseOuterTrianglesAndHoles();

    // Get CDT triangulation.
    CDT::TriangleVec remaining_triangles = cdt.triangles;

    // Get the outer triangle and remove it from the remaining triangles.
    const CDT::Triangle outer_triangle = cdt.triangles.front();
    remaining_triangles[0] = remaining_triangles.back();
    remaining_triangles.pop_back();

    // Find convex hull edges.
    std::unordered_set<CDT::Edge> convex_hull_edges;
    for (auto it = remaining_triangles.begin();
            it != remaining_triangles.end();) {
        const CDT::Triangle& cdt_triangle = *it;
        bool ok = false;
        for (int i = 0; i < 3; ++ i) {
            if (std::find(
                        outer_triangle.vertices.begin(),
                        outer_triangle.vertices.end(),
                        cdt_triangle.vertices[i])) {
                convex_hull_edges.insert(CDT::Edge(
                            cdt_triangle.vertices[(i + 1) % 3],
                            cdt_triangle.vertices[(i + 2) % 3]));
                ok = true;
                break;
            }
        }
        if (ok) {
            *it = remaining_triangles.back();
            remaining_triangles.pop_back();
        } else {
            it++;
        }
    }

    // Find all triangles with one edge in the convex hull edges.
    auto cmp = [&cdt_vertices](
            const CDT::Triangle& cdt_triangle_1,
            const CDT::Triangle& cdt_triangle_2)
    {
        return area(cdt_vertices, cdt_triangle_1) > area(cdt_vertices, cdt_triangle_2);
    };
    std::set<CDT::Triangle, decltype(cmp)> triangle_heap(cmp);
    for (auto it = remaining_triangles.begin();
            it != remaining_triangles.end();) {
        const CDT::Triangle& cdt_triangle = *it;
        bool ok = false;
        for (int i = 0; i < 3; ++ i) {
            CDT::Edge edge(
                    cdt_triangle.vertices[i],
                    cdt_triangle.vertices[(i + 1) % 3]);
            if (convex_hull_edges.find(edge) != convex_hull_edges.end()) {
                triangle_heap.insert(cdt_triangle);
                ok = true;
                break;
            }
        }
        if (ok) {
            *it = remaining_triangles.back();
            remaining_triangles.pop_back();
        } else {
            it++;
        }
    }

    for (;;) {
        // Pop the best triangle to remove.
        CDT::Triangle cdt_triangle = *triangle_heap.begin();
        triangle_heap.erase(triangle_heap.begin());

        // Add its neighbors to the heap.
        for (auto it = remaining_triangles.begin();
                it != remaining_triangles.end();) {
            const CDT::Triangle& cdt_triangle = *it;
            bool ok = false;
            // TODO
            for (int i = 0; i < 3; ++ i) {
                CDT::Edge edge(
                        cdt_triangle.vertices[i],
                        cdt_triangle.vertices[(i + 1) % 3]);
                if (convex_hull_edges.find(edge) != convex_hull_edges.end()) {
                    triangle_heap.insert(cdt_triangle);
                    ok = true;
                    break;
                }
            }
            if (ok) {
                *it = remaining_triangles.back();
                remaining_triangles.pop_back();
            } else {
                it++;
            }
        }
    }

    // For each vertex, the triangles to which it belongs.
    std::vector<CDT::TriangleVec> vertex_triangles;
    // TODO

    // Retrieve shape.
    Shape shape_new;
    // TODO

    return shape_new;
}

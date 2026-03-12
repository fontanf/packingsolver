#include "irregular/linear_programming.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "shape/convex_partition.hpp"

#include "mathoptsolverscmake/milp.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

bool aabb_may_intersect(
        const AxisAlignedBoundingBox& aabb_1,
        const AxisAlignedBoundingBox& aabb_2)
{
    return aabb_1.x_max > aabb_2.x_min && aabb_2.x_max > aabb_1.x_min
        && aabb_1.y_max > aabb_2.y_min && aabb_2.y_max > aabb_1.y_min;
}

/**
 * Try each edge of `shape_from` as a potential separator from `shape_to`.
 *
 * (dx, dy) = world center of shape_to - world center of shape_from.
 *
 * For each edge, compute:
 *   a = Y_{l+1} - Y_l,  b = X_l - X_{l+1}  (outward normal)
 *   rhs = a * X_{from,l} + b * Y_{from,l} - min_{to_l}[a * X_{to,l} + b * Y_{to,l}]
 *   min_dist = a * dx + b * dy - rhs
 *
 * Updates best_* if a better edge is found. Returns true iff best was updated.
 */
bool try_edges_as_separator(
        const Shape& shape_from,
        const Shape& shape_to,
        LengthDbl dx,
        LengthDbl dy,
        LengthDbl& best_min_dist,
        LengthDbl& best_a,
        LengthDbl& best_b,
        LengthDbl& best_rhs)
{
    bool updated = false;
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape_from.elements.size();
            ++element_pos) {
        const Point& p_l = shape_from.elements[element_pos].start;
        const Point& p_l1 = shape_from.elements[
            (element_pos + 1) % (ElementPos)shape_from.elements.size()].start;
        LengthDbl a = p_l1.y - p_l.y;
        LengthDbl b = p_l.x - p_l1.x;

        LengthDbl min_proj = std::numeric_limits<LengthDbl>::infinity();
        for (const ShapeElement& element_to: shape_to.elements)
            min_proj = std::min(min_proj, a * element_to.start.x + b * element_to.start.y);
        LengthDbl rhs = a * p_l.x + b * p_l.y - min_proj;
        LengthDbl min_dist = a * dx + b * dy - rhs;

        if (min_dist > best_min_dist) {
            best_min_dist = min_dist;
            best_a = a;
            best_b = b;
            best_rhs = rhs;
            updated = true;
        }
    }
    return updated;
}

Solution linear_programming(
        const Instance& instance,
        const Solution& initial_solution,
        const LinearProgrammingParameters& parameters,
        BinPos bin_pos,
        const std::vector<std::vector<std::vector<Shape>>>& item_types_convex_decompositions)
{
    const LengthDbl sv = instance.parameters().scale_value;

    Solution solution(instance);
    solution.append(initial_solution, bin_pos, 1);
    double current_value = 0;
    for (const SolutionItem& solution_item: initial_solution.bin(bin_pos).items) {
        if (instance.parameters().anchor_corner == Corner::BottomLeft
                || instance.parameters().anchor_corner == Corner::TopLeft) {
            current_value += solution_item.bl_corner.x * sv;
        } else {
            current_value -= solution_item.bl_corner.x * sv;
        }
        if (instance.parameters().anchor_corner == Corner::BottomLeft
                || instance.parameters().anchor_corner == Corner::BottomRight) {
            current_value += solution_item.bl_corner.y * sv;
        } else {
            current_value -= solution_item.bl_corner.y * sv;
        }
    }

    LengthDbl movement_box_half_size = 10;

    for (Counter number_of_iterations = 0;; ++number_of_iterations) {

        ////////////////////
        // Setup LP model //
        ////////////////////

        // Build LP model.
        mathoptsolverscmake::MilpModel lp_model;

        /////////////////////////////
        // Objective and variables //
        /////////////////////////////

        // Set objective sense.
        lp_model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Minimize;

        const SolutionBin& solution_bin = solution.bin(0);
        std::vector<int> x(solution_bin.items.size());
        std::vector<int> y(solution_bin.items.size());
        for (ItemPos pos = 0; pos < (ItemPos)solution_bin.items.size(); ++pos) {
            x[pos] = lp_model.variables_lower_bounds.size();
            lp_model.variables_lower_bounds.push_back(-std::numeric_limits<LengthDbl>::infinity());
            lp_model.variables_upper_bounds.push_back(+std::numeric_limits<LengthDbl>::infinity());
            lp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            if (instance.parameters().anchor_corner == Corner::BottomLeft
                    || instance.parameters().anchor_corner == Corner::TopLeft) {
                lp_model.objective_coefficients.push_back(1);
            } else {
                lp_model.objective_coefficients.push_back(-1);
            }
            y[pos] = lp_model.variables_lower_bounds.size();
            lp_model.variables_lower_bounds.push_back(-std::numeric_limits<LengthDbl>::infinity());
            lp_model.variables_upper_bounds.push_back(+std::numeric_limits<LengthDbl>::infinity());
            lp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            if (instance.parameters().anchor_corner == Corner::BottomLeft
                    || instance.parameters().anchor_corner == Corner::BottomRight) {
                lp_model.objective_coefficients.push_back(1);
            } else {
                lp_model.objective_coefficients.push_back(-1);
            }
        }

        /////////////////
        // Constraints //
        /////////////////

        // Get bin type.
        const BinType& bin_type = instance.bin_type(solution_bin.bin_type_id);

        // Compute convex decompositions of borders and defects.
        std::vector<std::vector<Shape>> borders_convex_decompositions(
                bin_type.borders.size());
        for (Counter border_pos = 0;
                border_pos < (Counter)bin_type.borders.size();
                ++border_pos) {
            borders_convex_decompositions[border_pos] = shape::compute_convex_partition(
                    bin_type.borders[border_pos].shape_scaled);
        }
        std::vector<std::vector<Shape>> defects_convex_decompositions(
                bin_type.defects.size());
        for (Counter defect_pos = 0;
                defect_pos < (Counter)bin_type.defects.size();
                ++defect_pos) {
            defects_convex_decompositions[defect_pos] = shape::compute_convex_partition(
                    bin_type.defects[defect_pos].shape_scaled);
        }

        // Precompute AABBs for obstacle convex parts (fixed world coordinates).
        std::vector<std::vector<AxisAlignedBoundingBox>> border_part_aabbs(bin_type.borders.size());
        for (Counter border_pos = 0;
                border_pos < (Counter)bin_type.borders.size();
                ++border_pos) {
            for (const Shape& part: borders_convex_decompositions[border_pos])
                border_part_aabbs[border_pos].push_back(part.compute_min_max());
        }
        std::vector<std::vector<AxisAlignedBoundingBox>> defect_part_aabbs(bin_type.defects.size());
        for (Counter defect_pos = 0;
                defect_pos < (Counter)bin_type.defects.size();
                ++defect_pos) {
            for (const Shape& part: defects_convex_decompositions[defect_pos])
                defect_part_aabbs[defect_pos].push_back(part.compute_min_max());
        }

        // Precompute rotated convex decompositions for each solution item.
        // item_rotated_decompositions[item_pos][item_shape_pos] = list of convex parts
        // with the item's angle and mirror already applied.
        std::vector<std::vector<std::vector<Shape>>> item_rotated_decompositions(
                solution_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            const ItemType& item_type = instance.item_type(solution_item.item_type_id);
            item_rotated_decompositions[item_pos].resize(item_type.shapes.size());
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                for (const Shape& convex_part:
                        item_types_convex_decompositions[solution_item.item_type_id][item_shape_pos]) {
                    Shape rotated = convex_part;
                    if (solution_item.mirror)
                        rotated = rotated.axial_symmetry_y_axis();
                    rotated = rotated.rotate(solution_item.angle);
                    item_rotated_decompositions[item_pos][item_shape_pos].push_back(rotated);
                }
            }
        }

        // Precompute AABBs for rotated item convex parts (local coordinates,
        // before adding bl_corner).
        std::vector<std::vector<std::vector<AxisAlignedBoundingBox>>> item_part_aabbs(
                solution_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const ItemType& item_type = instance.item_type(
                    solution_bin.items[item_pos].item_type_id);
            item_part_aabbs[item_pos].resize(item_type.shapes.size());
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                for (const Shape& part:
                        item_rotated_decompositions[item_pos][item_shape_pos])
                    item_part_aabbs[item_pos][item_shape_pos].push_back(
                            part.compute_min_max());
            }
        }

        // Precompute per-item bin bounds (without movement restriction) and
        // current scaled positions. The local bounding box of each item is
        // read from the already-computed item_part_aabbs.
        const ItemPos n_items = (ItemPos)solution_bin.items.size();
        std::vector<LengthDbl> item_bin_lb_x(n_items), item_bin_ub_x(n_items);
        std::vector<LengthDbl> item_bin_lb_y(n_items), item_bin_ub_y(n_items);
        std::vector<LengthDbl> item_cx_scaled(n_items), item_cy_scaled(n_items);
        for (ItemPos item_pos = 0; item_pos < n_items; ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            const ItemType& item_type = instance.item_type(solution_item.item_type_id);

            LengthDbl x_min_local = std::numeric_limits<LengthDbl>::infinity();
            LengthDbl x_max_local = -std::numeric_limits<LengthDbl>::infinity();
            LengthDbl y_min_local = std::numeric_limits<LengthDbl>::infinity();
            LengthDbl y_max_local = -std::numeric_limits<LengthDbl>::infinity();
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                for (const AxisAlignedBoundingBox& aabb: item_part_aabbs[item_pos][item_shape_pos]) {
                    x_min_local = std::min(x_min_local, aabb.x_min);
                    x_max_local = std::max(x_max_local, aabb.x_max);
                    y_min_local = std::min(y_min_local, aabb.y_min);
                    y_max_local = std::max(y_max_local, aabb.y_max);
                }
            }

            item_bin_lb_x[item_pos] = bin_type.x_min * sv - x_min_local;
            item_bin_ub_x[item_pos] = bin_type.x_max * sv - x_max_local;
            item_bin_lb_y[item_pos] = bin_type.y_min * sv - y_min_local;
            item_bin_ub_y[item_pos] = bin_type.y_max * sv - y_max_local;
            item_cx_scaled[item_pos] = solution_item.bl_corner.x * sv;
            item_cy_scaled[item_pos] = solution_item.bl_corner.y * sv;
        }

        // Count total convex parts (items + borders + defects).
        int total_convex_parts = 0;
        for (ItemPos item_pos = 0; item_pos < n_items; ++item_pos) {
            const ItemType& item_type = instance.item_type(
                    solution_bin.items[item_pos].item_type_id);
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                total_convex_parts += (int)item_rotated_decompositions[item_pos][item_shape_pos].size();
            }
        }
        for (Counter border_pos = 0;
                border_pos < (Counter)bin_type.borders.size();
                ++border_pos) {
            total_convex_parts += (int)borders_convex_decompositions[border_pos].size();
        }
        for (Counter defect_pos = 0;
                defect_pos < (Counter)bin_type.defects.size();
                ++defect_pos) {
            total_convex_parts += (int)defects_convex_decompositions[defect_pos].size();
        }
        const int max_pairs = 16 * total_convex_parts;

        // Lambda: count AABB-passing constraint pairs for a given movement box
        // half-size (in original coordinates). An infinity half-size means no
        // movement restriction beyond the bin bounds.
        auto count_pairs = [&](LengthDbl box_half) -> int {
            LengthDbl box = box_half * sv;
            std::vector<LengthDbl> lbx(n_items), ubx(n_items), lby(n_items), uby(n_items);
            for (ItemPos item_pos = 0; item_pos < n_items; ++item_pos) {
                lbx[item_pos] = std::max(item_bin_lb_x[item_pos], item_cx_scaled[item_pos] - box);
                ubx[item_pos] = std::min(item_bin_ub_x[item_pos], item_cx_scaled[item_pos] + box);
                lby[item_pos] = std::max(item_bin_lb_y[item_pos], item_cy_scaled[item_pos] - box);
                uby[item_pos] = std::min(item_bin_ub_y[item_pos], item_cy_scaled[item_pos] + box);
            }
            int count = 0;
            for (ItemPos item_pos = 0; item_pos < n_items; ++item_pos) {
                const ItemType& item_type = instance.item_type(
                        solution_bin.items[item_pos].item_type_id);
                for (Counter border_pos = 0;
                        border_pos < (Counter)bin_type.borders.size(); ++border_pos) {
                    for (ItemShapePos item_shape_pos = 0;
                            item_shape_pos < (ItemShapePos)item_type.shapes.size();
                            ++item_shape_pos) {
                        for (Counter part_item_pos = 0;
                                part_item_pos < (Counter)item_part_aabbs[item_pos][item_shape_pos].size();
                                ++part_item_pos) {
                            const AxisAlignedBoundingBox& la = item_part_aabbs[item_pos][item_shape_pos][part_item_pos];
                            AxisAlignedBoundingBox w = {lbx[item_pos] + la.x_min, ubx[item_pos] + la.x_max,
                                      lby[item_pos] + la.y_min, uby[item_pos] + la.y_max};
                            for (const AxisAlignedBoundingBox& obs: border_part_aabbs[border_pos])
                                if (aabb_may_intersect(w, obs)) ++count;
                        }
                    }
                }
                for (Counter defect_pos = 0;
                        defect_pos < (Counter)bin_type.defects.size(); ++defect_pos) {
                    for (ItemShapePos item_shape_pos = 0;
                            item_shape_pos < (ItemShapePos)item_type.shapes.size();
                            ++item_shape_pos) {
                        for (Counter part_item_pos = 0;
                                part_item_pos < (Counter)item_part_aabbs[item_pos][item_shape_pos].size();
                                ++part_item_pos) {
                            const AxisAlignedBoundingBox& la = item_part_aabbs[item_pos][item_shape_pos][part_item_pos];
                            AxisAlignedBoundingBox w = {lbx[item_pos] + la.x_min, ubx[item_pos] + la.x_max,
                                      lby[item_pos] + la.y_min, uby[item_pos] + la.y_max};
                            for (const AxisAlignedBoundingBox& obs: defect_part_aabbs[defect_pos])
                                if (aabb_may_intersect(w, obs)) ++count;
                        }
                    }
                }
            }
            for (ItemPos item_1_pos = 0; item_1_pos < n_items; ++item_1_pos) {
                const ItemType& item_type_1 = instance.item_type(
                        solution_bin.items[item_1_pos].item_type_id);
                for (ItemPos item_2_pos = item_1_pos + 1; item_2_pos < n_items; ++item_2_pos) {
                    const ItemType& item_type_2 = instance.item_type(
                            solution_bin.items[item_2_pos].item_type_id);
                    for (ItemShapePos sp1 = 0;
                            sp1 < (ItemShapePos)item_type_1.shapes.size(); ++sp1) {
                        for (ItemShapePos sp2 = 0;
                                sp2 < (ItemShapePos)item_type_2.shapes.size(); ++sp2) {
                            for (Counter pp1 = 0;
                                    pp1 < (Counter)item_part_aabbs[item_1_pos][sp1].size(); ++pp1) {
                                const AxisAlignedBoundingBox& a1 = item_part_aabbs[item_1_pos][sp1][pp1];
                                AxisAlignedBoundingBox w1 = {lbx[item_1_pos] + a1.x_min, ubx[item_1_pos] + a1.x_max,
                                           lby[item_1_pos] + a1.y_min, uby[item_1_pos] + a1.y_max};
                                for (Counter pp2 = 0;
                                        pp2 < (Counter)item_part_aabbs[item_2_pos][sp2].size(); ++pp2) {
                                    const AxisAlignedBoundingBox& a2 = item_part_aabbs[item_2_pos][sp2][pp2];
                                    AxisAlignedBoundingBox w2 = {lbx[item_2_pos] + a2.x_min, ubx[item_2_pos] + a2.x_max,
                                               lby[item_2_pos] + a2.y_min, uby[item_2_pos] + a2.y_max};
                                    if (aabb_may_intersect(w1, w2)) ++count;
                                }
                            }
                        }
                    }
                }
            }
            return count;
        };

        // Adjust movement_box_half_size so that the pair count <= max_pairs.
        // If even the fully unrestricted case is within budget, use infinity.
        // Otherwise, halve while too many pairs, then double while we have room.
        if (count_pairs(std::numeric_limits<LengthDbl>::infinity()) <= max_pairs) {
            movement_box_half_size = std::numeric_limits<LengthDbl>::infinity();
        } else {
            while (count_pairs(movement_box_half_size) > max_pairs)
                movement_box_half_size /= 2;
            while (count_pairs(movement_box_half_size * 2) <= max_pairs)
                movement_box_half_size *= 2;
        }

        // Compute final variable bounds using the adjusted movement box.
        const LengthDbl box = movement_box_half_size * sv;
        std::vector<LengthDbl> lb_x(n_items), ub_x(n_items), lb_y(n_items), ub_y(n_items);
        for (ItemPos item_pos = 0; item_pos < n_items; ++item_pos) {
            lb_x[item_pos] = std::max(item_bin_lb_x[item_pos], item_cx_scaled[item_pos] - box);
            ub_x[item_pos] = std::min(item_bin_ub_x[item_pos], item_cx_scaled[item_pos] + box);
            lb_y[item_pos] = std::max(item_bin_lb_y[item_pos], item_cy_scaled[item_pos] - box);
            ub_y[item_pos] = std::min(item_bin_ub_y[item_pos], item_cy_scaled[item_pos] + box);
            lp_model.variables_lower_bounds[x[item_pos]] = lb_x[item_pos];
            lp_model.variables_upper_bounds[x[item_pos]] = ub_x[item_pos];
            lp_model.variables_lower_bounds[y[item_pos]] = lb_y[item_pos];
            lp_model.variables_upper_bounds[y[item_pos]] = ub_y[item_pos];
        }

        // Constraints for bin borders and defects.
        // For each item, for each border/defect convex part, and for each item
        // convex part, find the separating edge that maximizes the minimum
        // signed distance in the current solution and add the LP constraint.
        //
        // The obstacle (border/defect) is fixed; the item position is variable.
        // Obstacle vertices are already in world coordinates (treated as local
        // coords with offset 0).
        //
        // If the separator comes from convex_part_item (best_from_item = true):
        //   LP upper-bound: a*x[item_pos] + b*y[item_pos] <= -best_rhs
        //
        // If the separator comes from convex_part_obs (best_from_item = false):
        //   LP lower-bound: a*x[item_pos] + b*y[item_pos] >= best_rhs
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            const ItemType& item_type = instance.item_type(solution_item.item_type_id);
            LengthDbl cx_item = solution_item.bl_corner.x * sv;
            LengthDbl cy_item = solution_item.bl_corner.y * sv;

            for (Counter border_pos = 0;
                    border_pos < (Counter)bin_type.borders.size();
                    ++border_pos) {
                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type.shapes.size();
                        ++item_shape_pos) {
                    for (Counter part_item_pos = 0;
                            part_item_pos < (Counter)item_rotated_decompositions[item_pos][item_shape_pos].size();
                            ++part_item_pos) {
                        const Shape& convex_part_item = item_rotated_decompositions[item_pos][item_shape_pos][part_item_pos];
                        const AxisAlignedBoundingBox& local_aabb = item_part_aabbs[item_pos][item_shape_pos][part_item_pos];
                        AxisAlignedBoundingBox item_world_aabb = {
                            lb_x[item_pos] + local_aabb.x_min,
                            ub_x[item_pos] + local_aabb.x_max,
                            lb_y[item_pos] + local_aabb.y_min,
                            ub_y[item_pos] + local_aabb.y_max
                        };
                        for (Counter part_obs_pos = 0;
                                part_obs_pos < (Counter)borders_convex_decompositions[border_pos].size();
                                ++part_obs_pos) {
                            if (!aabb_may_intersect(item_world_aabb, border_part_aabbs[border_pos][part_obs_pos]))
                                continue;
                            const Shape& convex_part_obs = borders_convex_decompositions[border_pos][part_obs_pos];

                            LengthDbl best_min_dist = -std::numeric_limits<LengthDbl>::infinity();
                            LengthDbl best_a = 0, best_b = 0, best_rhs = 0;
                            bool best_from_item = false;

                            if (try_edges_as_separator(
                                    convex_part_item, convex_part_obs,
                                    -cx_item, -cy_item,
                                    best_min_dist, best_a, best_b, best_rhs))
                                best_from_item = true;
                            if (try_edges_as_separator(
                                    convex_part_obs, convex_part_item,
                                    cx_item, cy_item,
                                    best_min_dist, best_a, best_b, best_rhs))
                                best_from_item = false;

                            if (shape::equal(best_a, 0.0) && shape::equal(best_b, 0.0))
                                continue;

                            lp_model.constraints_starts.push_back(
                                    lp_model.elements_variables.size());
                            if (best_from_item) {
                                lp_model.constraints_lower_bounds.push_back(
                                        -std::numeric_limits<LengthDbl>::infinity());
                                lp_model.constraints_upper_bounds.push_back(-best_rhs);
                            } else {
                                lp_model.constraints_lower_bounds.push_back(best_rhs);
                                lp_model.constraints_upper_bounds.push_back(
                                        std::numeric_limits<LengthDbl>::infinity());
                            }
                            if (!shape::equal(best_a, 0.0)) {
                                lp_model.elements_variables.push_back(x[item_pos]);
                                lp_model.elements_coefficients.push_back(best_a);
                            }
                            if (!shape::equal(best_b, 0.0)) {
                                lp_model.elements_variables.push_back(y[item_pos]);
                                lp_model.elements_coefficients.push_back(best_b);
                            }
                        }
                    }
                }
            }

            for (Counter defect_pos = 0;
                    defect_pos < (Counter)bin_type.defects.size();
                    ++defect_pos) {
                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type.shapes.size();
                        ++item_shape_pos) {
                    for (Counter part_item_pos = 0;
                            part_item_pos < (Counter)item_rotated_decompositions[item_pos][item_shape_pos].size();
                            ++part_item_pos) {
                        const Shape& convex_part_item = item_rotated_decompositions[item_pos][item_shape_pos][part_item_pos];
                        const AxisAlignedBoundingBox& local_aabb = item_part_aabbs[item_pos][item_shape_pos][part_item_pos];
                        AxisAlignedBoundingBox item_world_aabb = {
                            lb_x[item_pos] + local_aabb.x_min,
                            ub_x[item_pos] + local_aabb.x_max,
                            lb_y[item_pos] + local_aabb.y_min,
                            ub_y[item_pos] + local_aabb.y_max
                        };
                        for (Counter part_obs_pos = 0;
                                part_obs_pos < (Counter)defects_convex_decompositions[defect_pos].size();
                                ++part_obs_pos) {
                            if (!aabb_may_intersect(item_world_aabb, defect_part_aabbs[defect_pos][part_obs_pos]))
                                continue;
                            const Shape& convex_part_obs = defects_convex_decompositions[defect_pos][part_obs_pos];

                            LengthDbl best_min_dist = -std::numeric_limits<LengthDbl>::infinity();
                            LengthDbl best_a = 0, best_b = 0, best_rhs = 0;
                            bool best_from_item = false;

                            if (try_edges_as_separator(
                                    convex_part_item, convex_part_obs,
                                    -cx_item, -cy_item,
                                    best_min_dist, best_a, best_b, best_rhs))
                                best_from_item = true;
                            if (try_edges_as_separator(
                                    convex_part_obs, convex_part_item,
                                    cx_item, cy_item,
                                    best_min_dist, best_a, best_b, best_rhs))
                                best_from_item = false;

                            if (shape::equal(best_a, 0.0) && shape::equal(best_b, 0.0))
                                continue;

                            lp_model.constraints_starts.push_back(
                                    lp_model.elements_variables.size());
                            if (best_from_item) {
                                lp_model.constraints_lower_bounds.push_back(
                                        -std::numeric_limits<LengthDbl>::infinity());
                                lp_model.constraints_upper_bounds.push_back(-best_rhs);
                            } else {
                                lp_model.constraints_lower_bounds.push_back(best_rhs);
                                lp_model.constraints_upper_bounds.push_back(
                                        std::numeric_limits<LengthDbl>::infinity());
                            }
                            if (!shape::equal(best_a, 0.0)) {
                                lp_model.elements_variables.push_back(x[item_pos]);
                                lp_model.elements_coefficients.push_back(best_a);
                            }
                            if (!shape::equal(best_b, 0.0)) {
                                lp_model.elements_variables.push_back(y[item_pos]);
                                lp_model.elements_coefficients.push_back(best_b);
                            }
                        }
                    }
                }
            }
        }

        // Each pair of items must not intersect.
        // For each pair of convex parts, find the separating edge that
        // maximizes the minimum signed distance in the current solution, and
        // add the corresponding linear constraint.
        //
        // If the separator comes from convex_part_1 (best_from_part_1 = true):
        //   a*x[item_2_pos] + b*y[item_2_pos] - a*x[item_1_pos] - b*y[item_1_pos] >= best_rhs
        //
        // If the separator comes from convex_part_2 (best_from_part_1 = false):
        //   a*x[item_1_pos] + b*y[item_1_pos] - a*x[item_2_pos] - b*y[item_2_pos] >= best_rhs
        for (ItemPos item_1_pos = 0;
                item_1_pos < (ItemPos)solution_bin.items.size();
                ++item_1_pos) {
            const SolutionItem& solution_item_1 = solution_bin.items[item_1_pos];
            const ItemType& item_type_1 = instance.item_type(solution_item_1.item_type_id);
            LengthDbl cx_1 = solution_item_1.bl_corner.x * sv;
            LengthDbl cy_1 = solution_item_1.bl_corner.y * sv;

            for (ItemPos item_2_pos = item_1_pos + 1;
                    item_2_pos < (ItemPos)solution_bin.items.size();
                    ++item_2_pos) {
                const SolutionItem& solution_item_2 = solution_bin.items[item_2_pos];
                const ItemType& item_type_2 = instance.item_type(solution_item_2.item_type_id);
                LengthDbl cx_2 = solution_item_2.bl_corner.x * sv;
                LengthDbl cy_2 = solution_item_2.bl_corner.y * sv;

                for (ItemShapePos item_shape_pos_1 = 0;
                        item_shape_pos_1 < (ItemShapePos)item_type_1.shapes.size();
                        ++item_shape_pos_1) {
                    for (ItemShapePos item_shape_pos_2 = 0;
                            item_shape_pos_2 < (ItemShapePos)item_type_2.shapes.size();
                            ++item_shape_pos_2) {
                        for (Counter part_1_pos = 0;
                                part_1_pos < (Counter)item_rotated_decompositions[item_1_pos][item_shape_pos_1].size();
                                ++part_1_pos) {
                            const Shape& convex_part_1 = item_rotated_decompositions[item_1_pos][item_shape_pos_1][part_1_pos];
                            const AxisAlignedBoundingBox& aabb_1_local = item_part_aabbs[item_1_pos][item_shape_pos_1][part_1_pos];
                            AxisAlignedBoundingBox world_aabb_1 = {
                                lb_x[item_1_pos] + aabb_1_local.x_min,
                                ub_x[item_1_pos] + aabb_1_local.x_max,
                                lb_y[item_1_pos] + aabb_1_local.y_min,
                                ub_y[item_1_pos] + aabb_1_local.y_max
                            };
                            for (Counter part_2_pos = 0;
                                    part_2_pos < (Counter)item_rotated_decompositions[item_2_pos][item_shape_pos_2].size();
                                    ++part_2_pos) {
                                const Shape& convex_part_2 = item_rotated_decompositions[item_2_pos][item_shape_pos_2][part_2_pos];
                                const AxisAlignedBoundingBox& aabb_2_local = item_part_aabbs[item_2_pos][item_shape_pos_2][part_2_pos];
                                AxisAlignedBoundingBox world_aabb_2 = {
                                    lb_x[item_2_pos] + aabb_2_local.x_min,
                                    ub_x[item_2_pos] + aabb_2_local.x_max,
                                    lb_y[item_2_pos] + aabb_2_local.y_min,
                                    ub_y[item_2_pos] + aabb_2_local.y_max
                                };
                                if (!aabb_may_intersect(world_aabb_1, world_aabb_2))
                                    continue;

                                LengthDbl best_min_dist = -std::numeric_limits<LengthDbl>::infinity();
                                LengthDbl best_a = 0, best_b = 0, best_rhs = 0;
                                bool best_from_part_1 = false;

                                if (try_edges_as_separator(
                                        convex_part_1, convex_part_2,
                                        cx_2 - cx_1, cy_2 - cy_1,
                                        best_min_dist, best_a, best_b, best_rhs))
                                    best_from_part_1 = true;
                                if (try_edges_as_separator(
                                        convex_part_2, convex_part_1,
                                        cx_1 - cx_2, cy_1 - cy_2,
                                        best_min_dist, best_a, best_b, best_rhs))
                                    best_from_part_1 = false;

                                if (shape::equal(best_a, 0.0) && shape::equal(best_b, 0.0))
                                    continue;

                                lp_model.constraints_starts.push_back(
                                        lp_model.elements_variables.size());
                                lp_model.constraints_lower_bounds.push_back(best_rhs);
                                lp_model.constraints_upper_bounds.push_back(
                                        std::numeric_limits<LengthDbl>::infinity());
                                ItemPos item_pos_near = best_from_part_1 ? item_2_pos : item_1_pos;
                                ItemPos item_pos_far  = best_from_part_1 ? item_1_pos : item_2_pos;
                                if (!shape::equal(best_a, 0.0)) {
                                    lp_model.elements_variables.push_back(x[item_pos_near]);
                                    lp_model.elements_coefficients.push_back(best_a);
                                    lp_model.elements_variables.push_back(x[item_pos_far]);
                                    lp_model.elements_coefficients.push_back(-best_a);
                                }
                                if (!shape::equal(best_b, 0.0)) {
                                    lp_model.elements_variables.push_back(y[item_pos_near]);
                                    lp_model.elements_coefficients.push_back(best_b);
                                    lp_model.elements_variables.push_back(y[item_pos_far]);
                                    lp_model.elements_coefficients.push_back(-best_b);
                                }
                            }
                        }
                    }
                }
            }
        }

        //lp_model.format(std::cout, 1);

        ///////////
        // Solve //
        ///////////

        std::vector<double> lp_solution;
        if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
            Highs highs;
            mathoptsolverscmake::reduce_printout(highs);
            mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
            highs.setOptionValue("parallel", "off");
            //mathoptsolverscmake::set_log_file(highs, "highs.log");
            mathoptsolverscmake::load(highs, lp_model);
            std::cout << "LP solve start" << std::endl;
            mathoptsolverscmake::solve(highs);
            std::cout << "LP solve end" << std::endl;
            lp_solution = mathoptsolverscmake::get_solution(highs);
#else
            throw std::invalid_argument(FUNC_SIGNATURE);
#endif

        } else {
            throw std::invalid_argument(FUNC_SIGNATURE);
        }

        double new_value = lp_model.evaluate_objective(lp_solution);

        // If the solution of the MILP model is not better, stop.
        if (!shape::strictly_greater(current_value, new_value))
            break;
        current_value = new_value;

        // Retrieve solution.
        Solution new_solution(instance);
        new_solution.add_bin(solution_bin.bin_type_id, 1);
        for (ItemPos pos = 0; pos < (ItemPos)solution_bin.items.size(); ++pos) {
            const SolutionItem& solution_item = solution_bin.items[pos];
            new_solution.add_item(
                    0,
                    solution_item.item_type_id,
                    {lp_solution[x[pos]] / sv, lp_solution[y[pos]] / sv},
                    solution_item.angle,
                    solution_item.mirror);
        }
        solution = new_solution;
    }

    return solution;
}

}

LinearProgrammingOutput packingsolver::irregular::linear_programming(
        const Instance& instance,
        const Solution& initial_solution,
        const LinearProgrammingParameters& parameters)
{
    std::cout << "linear_programming" << std::endl;
    LinearProgrammingOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Decompose each shape into convex polygons.
    // convex_decompositions[item_type_id][item_shape_pos][convex_part_pos]
    std::vector<std::vector<std::vector<Shape>>> item_types_convex_decompositions(
            instance.number_of_item_types());;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        item_types_convex_decompositions[item_type_id] = std::vector<std::vector<Shape>>(
                item_type.shapes.size());
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            const ItemShape& item_shape = item_type.shapes[item_shape_pos];
            item_types_convex_decompositions[item_type_id][item_shape_pos] = shape::compute_convex_partition(item_shape.shape_scaled);
        }
    }

    Solution solution(instance);
    for (BinPos bin_pos = 0;
            bin_pos < initial_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = initial_solution.bin(bin_pos);
        Solution new_solution = ::linear_programming(
                instance,
                initial_solution,
                parameters,
                bin_pos,
                item_types_convex_decompositions);
        solution.append(
                new_solution,
                0,
                solution_bin.copies);
    }
    //initial_solution.format(std::cout, 1);
    //solution.format(std::cout, 1);
    algorithm_formatter.update_solution(solution, "");

    //instance.write("lp_test.json");
    //initial_solution.write("lp_test_initial.json");
    //solution.write("lp_test_expected.json");
    //exit(1);
    std::cout << "linear_programming end" << std::endl;
    return output;
}

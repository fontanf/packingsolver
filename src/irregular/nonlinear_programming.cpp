#include "irregular/nonlinear_programming.hpp"

#ifdef KNITRO_FOUND
#include "mathoptsolverscmake/mathopt_knitro.hpp"
#endif

#include "irregular/utils.hpp"
#include "irregular/linear_programming.hpp"

#include <cmath>
#include <limits>

using namespace packingsolver;
using namespace packingsolver::irregular;

#ifdef KNITRO_FOUND

namespace
{

struct PointRef
{
    bool is_fixed = false;
    double const_x = 0.0;
    double const_y = 0.0;
    int xv_var_id = -1;
    int yv_var_id = -1;
};

/**
 * Add a separation constraint enforcing that point P is to the right of
 * directed edge E1->E2 (matching the LP convention):
 *   -(E2-E1) x (P-E1) >= 0
 * which expands to 6 bilinear terms:
 *   -E2x*Py + E1x*Py + E2y*Px - E1y*Px + E2x*E1y - E2y*E1x >= 0
 */
void add_cross_product_constraint(
        mathoptsolverscmake::MathOptModel& model,
        const PointRef& edge_start,
        const PointRef& edge_end,
        const PointRef& point)
{
    int constraint_id = (int)model.constraints_lower_bounds.size();
    model.constraints_lower_bounds.push_back(0.0);
    model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
    model.constraints_starts.push_back((int)model.elements_variables.size());
    model.quadratic_elements_constraints_starts.push_back(
        (int)model.quadratic_elements_variables_1.size());
    model.constraints_functions.emplace_back();
    double lower_bound = 0.0;

    // Term 1: -E2x * Py   (coeff -1.0)
    if (!edge_end.is_fixed && !point.is_fixed) {
        model.quadratic_elements_variables_1.push_back(edge_end.xv_var_id);
        model.quadratic_elements_variables_2.push_back(point.yv_var_id);
        model.quadratic_elements_coefficients.push_back(-1.0);
    } else if (!edge_end.is_fixed) {
        model.elements_variables.push_back(edge_end.xv_var_id);
        model.elements_coefficients.push_back(-point.const_y);
    } else if (!point.is_fixed) {
        model.elements_variables.push_back(point.yv_var_id);
        model.elements_coefficients.push_back(-edge_end.const_x);
    } else {
        lower_bound -= -edge_end.const_x * point.const_y;
    }

    // Term 2: +E1x * Py   (coeff +1.0)
    if (!edge_start.is_fixed && !point.is_fixed) {
        model.quadratic_elements_variables_1.push_back(edge_start.xv_var_id);
        model.quadratic_elements_variables_2.push_back(point.yv_var_id);
        model.quadratic_elements_coefficients.push_back(+1.0);
    } else if (!edge_start.is_fixed) {
        model.elements_variables.push_back(edge_start.xv_var_id);
        model.elements_coefficients.push_back(+point.const_y);
    } else if (!point.is_fixed) {
        model.elements_variables.push_back(point.yv_var_id);
        model.elements_coefficients.push_back(+edge_start.const_x);
    } else {
        lower_bound -= +edge_start.const_x * point.const_y;
    }

    // Term 3: +E2y * Px   (coeff +1.0)
    if (!edge_end.is_fixed && !point.is_fixed) {
        model.quadratic_elements_variables_1.push_back(edge_end.yv_var_id);
        model.quadratic_elements_variables_2.push_back(point.xv_var_id);
        model.quadratic_elements_coefficients.push_back(+1.0);
    } else if (!edge_end.is_fixed) {
        model.elements_variables.push_back(edge_end.yv_var_id);
        model.elements_coefficients.push_back(+point.const_x);
    } else if (!point.is_fixed) {
        model.elements_variables.push_back(point.xv_var_id);
        model.elements_coefficients.push_back(+edge_end.const_y);
    } else {
        lower_bound -= +edge_end.const_y * point.const_x;
    }

    // Term 4: -E1y * Px   (coeff -1.0)
    if (!edge_start.is_fixed && !point.is_fixed) {
        model.quadratic_elements_variables_1.push_back(edge_start.yv_var_id);
        model.quadratic_elements_variables_2.push_back(point.xv_var_id);
        model.quadratic_elements_coefficients.push_back(-1.0);
    } else if (!edge_start.is_fixed) {
        model.elements_variables.push_back(edge_start.yv_var_id);
        model.elements_coefficients.push_back(-point.const_x);
    } else if (!point.is_fixed) {
        model.elements_variables.push_back(point.xv_var_id);
        model.elements_coefficients.push_back(-edge_start.const_y);
    } else {
        lower_bound -= -edge_start.const_y * point.const_x;
    }

    // Term 5: +E2x * E1y   (coeff +1.0)
    if (!edge_end.is_fixed && !edge_start.is_fixed) {
        model.quadratic_elements_variables_1.push_back(edge_end.xv_var_id);
        model.quadratic_elements_variables_2.push_back(edge_start.yv_var_id);
        model.quadratic_elements_coefficients.push_back(+1.0);
    } else if (!edge_end.is_fixed) {
        model.elements_variables.push_back(edge_end.xv_var_id);
        model.elements_coefficients.push_back(+edge_start.const_y);
    } else if (!edge_start.is_fixed) {
        model.elements_variables.push_back(edge_start.yv_var_id);
        model.elements_coefficients.push_back(+edge_end.const_x);
    } else {
        lower_bound -= +edge_end.const_x * edge_start.const_y;
    }

    // Term 6: -E2y * E1x   (coeff -1.0)
    if (!edge_end.is_fixed && !edge_start.is_fixed) {
        model.quadratic_elements_variables_1.push_back(edge_end.yv_var_id);
        model.quadratic_elements_variables_2.push_back(edge_start.xv_var_id);
        model.quadratic_elements_coefficients.push_back(-1.0);
    } else if (!edge_end.is_fixed) {
        model.elements_variables.push_back(edge_end.yv_var_id);
        model.elements_coefficients.push_back(-edge_start.const_x);
    } else if (!edge_start.is_fixed) {
        model.elements_variables.push_back(edge_start.xv_var_id);
        model.elements_coefficients.push_back(-edge_end.const_y);
    } else {
        lower_bound -= -edge_end.const_y * edge_start.const_x;
    }

    model.constraints_lower_bounds[constraint_id] = lower_bound;
}

}  // namespace

#endif  // KNITRO_FOUND

NonlinearProgrammingMinimizeShrinkageOutput packingsolver::irregular::nonlinear_programming_minimize_shrinkage(
        const Solution& initial_solution,
        const std::vector<double>& initial_lambda,
        const std::vector<Profit>& item_penalties,
        const NonlinearProgrammingMinimizeShrinkageParameters& parameters)
{
    const Instance& instance = initial_solution.instance();
    NonlinearProgrammingMinimizeShrinkageOutput output(instance);

    Solution solution(instance);
    solution.append(initial_solution, 0, 1);

    std::vector<double> current_lambda = initial_lambda;

    std::vector<ItemPos> fixed_items;
    std::vector<ItemPos> unfixed_items;
    for (ItemPos item_pos = 0;
            item_pos < (ItemPos)solution.bin(0).items.size();
            ++item_pos) {
        if (solution.bin(0).items[item_pos].is_fixed) {
            fixed_items.push_back(item_pos);
        } else {
            unfixed_items.push_back(item_pos);
        }
    }

    std::vector<bool> current_items_shrunken(solution.bin(0).items.size(), false);
    for (ItemPos item_var_pos = 0;
            item_var_pos < (ItemPos)unfixed_items.size();
            ++item_var_pos) {
        current_items_shrunken[unfixed_items[item_var_pos]] = true;
    }

#ifndef KNITRO_FOUND
    output.solution = solution;
    output.items_shrunken = current_items_shrunken;
    output.final_lambda = current_lambda;
    output.feasible = std::none_of(
            current_items_shrunken.begin(),
            current_items_shrunken.end(),
            [](bool shrunken) { return shrunken; });
    return output;
#else

    const InstanceBasicShapeDecomposition icd = compute_instance_basic_shape_decomposition(instance);
    const BinTypeId bin_type_id = initial_solution.bin(0).bin_type_id;
    const BinType& bin_type = instance.bin_type(bin_type_id);

    std::vector<AxisAlignedBoundingBox> border_part_aabbs;
    for (const BasicShape& border_part: icd.bin_types_borders[bin_type_id])
        border_part_aabbs.push_back(border_part.shape.compute_min_max());
    std::vector<AxisAlignedBoundingBox> defect_part_aabbs;
    for (const BasicShape& defect_part: icd.bin_types_defects[bin_type_id])
        defect_part_aabbs.push_back(defect_part.shape.compute_min_max());

    const LengthDbl sv = instance.parameters().scale_value;
    const LengthDbl movement_box_half_size = 10;
    const double inf = std::numeric_limits<double>::infinity();

    std::vector<AxisAlignedBoundingBox> fixed_item_part_aabbs;
    for (ItemPos item_pos = 0;
            item_pos < (ItemPos)solution.bin(0).items.size();
            ++item_pos) {
        const SolutionItem& solution_item = solution.bin(0).items[item_pos];
        if (!solution_item.is_fixed)
            continue;
        const LengthDbl cx = solution_item.bl_corner.x * sv;
        const LengthDbl cy = solution_item.bl_corner.y * sv;
        const ItemTypeId item_type_id = solution_item.item_type_id;
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)instance.item_type(item_type_id).shapes.size();
                ++item_shape_pos) {
            for (const BasicShape& basic_shape: icd.item_types[item_type_id][item_shape_pos]) {
                const BasicShape rotated = apply_placement(basic_shape, solution_item.mirror, solution_item.angle);
                const AxisAlignedBoundingBox p = rotated.shape.compute_min_max();
                AxisAlignedBoundingBox ps;
                ps.x_min = cx + p.x_min;
                ps.x_max = cx + p.x_max;
                ps.y_min = cy + p.y_min;
                ps.y_max = cy + p.y_max;
                fixed_item_part_aabbs.push_back(ps);
            }
        }
    }

    double current_value = 0;

    for (Counter number_of_iterations = 0;; ++number_of_iterations) {
        output.number_of_iterations = number_of_iterations;
        const SolutionBin& solution_bin = solution.bin(0);

        // Precompute rotated basic shape decompositions.
        std::vector<std::vector<std::vector<BasicShape>>> item_rotated_decompositions(
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
                for (const BasicShape& basic_shape: icd.item_types[solution_item.item_type_id][item_shape_pos]) {
                    item_rotated_decompositions[item_pos][item_shape_pos].push_back(
                            apply_placement(basic_shape, solution_item.mirror, solution_item.angle));
                }
            }
        }

        std::vector<ItemAxisAlignedBoundingBoxes> unfixed_item_aabbs = compute_unfixed_item_aabbs(
                instance,
                solution_bin,
                movement_box_half_size,
                item_rotated_decompositions,
                current_lambda);

        IntersectingParts intersecting_parts = compute_potentially_intersecting_parts(
                unfixed_item_aabbs,
                fixed_item_part_aabbs,
                border_part_aabbs,
                defect_part_aabbs);

        ///////////////////
        // Build NLP data //
        ///////////////////

        mathoptsolverscmake::MathOptModel model;
        model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;

        std::vector<ItemPos> item_to_var_pos(solution_bin.items.size(), -1);
        for (ItemPos item_var_pos = 0;
                item_var_pos < (ItemPos)unfixed_items.size();
                ++item_var_pos) {
            item_to_var_pos[unfixed_items[item_var_pos]] = item_var_pos;
        }

        /////////////////////////////
        // Objective and variables //
        /////////////////////////////

        std::vector<int> x_vars(unfixed_items.size());
        std::vector<int> y_vars(unfixed_items.size());
        std::vector<int> cos_vars(unfixed_items.size());
        std::vector<int> sin_vars(unfixed_items.size());
        std::vector<int> lambda_vars(unfixed_items.size());

        for (ItemPos item_var_pos = 0;
                item_var_pos < (ItemPos)unfixed_items.size();
                ++item_var_pos) {
            const ItemPos item_pos = unfixed_items[item_var_pos];
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            const double current_x = solution_item.bl_corner.x * sv;
            const double current_y = solution_item.bl_corner.y * sv;
            const double angle_rad = solution_item.angle * M_PI / 180.0;

            x_vars[item_var_pos] = (int)model.variables_lower_bounds.size();
            model.variables_lower_bounds.push_back(-inf);
            model.variables_upper_bounds.push_back(+inf);
            model.variables_initial_values.push_back(current_x);
            model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            model.objective_coefficients.push_back(0.0);

            y_vars[item_var_pos] = (int)model.variables_lower_bounds.size();
            model.variables_lower_bounds.push_back(-inf);
            model.variables_upper_bounds.push_back(+inf);
            model.variables_initial_values.push_back(current_y);
            model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            model.objective_coefficients.push_back(0.0);

            cos_vars[item_var_pos] = (int)model.variables_lower_bounds.size();
            model.variables_lower_bounds.push_back(-1.0);
            model.variables_upper_bounds.push_back(+1.0);
            model.variables_initial_values.push_back(std::cos(angle_rad));
            model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            model.objective_coefficients.push_back(0.0);

            sin_vars[item_var_pos] = (int)model.variables_lower_bounds.size();
            model.variables_lower_bounds.push_back(-1.0);
            model.variables_upper_bounds.push_back(+1.0);
            model.variables_initial_values.push_back(std::sin(angle_rad));
            model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            model.objective_coefficients.push_back(0.0);

            lambda_vars[item_var_pos] = (int)model.variables_lower_bounds.size();
            model.variables_lower_bounds.push_back(0.0);
            model.variables_upper_bounds.push_back(1.0);
            model.variables_initial_values.push_back(current_lambda[item_pos]);
            model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            model.objective_coefficients.push_back(item_penalties[item_pos]);
        }

        // Vertex variables: xv_vars[item_var_pos][shape_pos][part_pos][vertex_pos]
        // vertex_pos k corresponds to element k of the rotated shape.
        std::vector<std::vector<std::vector<std::vector<int>>>> xv_vars(unfixed_items.size());
        std::vector<std::vector<std::vector<std::vector<int>>>> yv_vars(unfixed_items.size());

        for (ItemPos item_var_pos = 0;
                item_var_pos < (ItemPos)unfixed_items.size();
                ++item_var_pos) {
            const ItemPos item_pos = unfixed_items[item_var_pos];
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            const ItemTypeId item_type_id = solution_item.item_type_id;
            const double current_x = solution_item.bl_corner.x * sv;
            const double current_y = solution_item.bl_corner.y * sv;
            const double angle_rad = solution_item.angle * M_PI / 180.0;
            const double current_cos = std::cos(angle_rad);
            const double current_sin = std::sin(angle_rad);
            const double current_lam = current_lambda[item_pos];

            const Counter n_shapes = (Counter)item_rotated_decompositions[item_pos].size();
            xv_vars[item_var_pos].resize(n_shapes);
            yv_vars[item_var_pos].resize(n_shapes);
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < n_shapes;
                    ++item_shape_pos) {
                const Counter n_parts =
                        (Counter)item_rotated_decompositions[item_pos][item_shape_pos].size();
                xv_vars[item_var_pos][item_shape_pos].resize(n_parts);
                yv_vars[item_var_pos][item_shape_pos].resize(n_parts);
                for (Counter part_pos = 0; part_pos < n_parts; ++part_pos) {
                    const Shape& icd_part =
                            icd.item_types[item_type_id][item_shape_pos][part_pos].shape;
                    const Counter n_vertices = (Counter)icd_part.elements.size();
                    xv_vars[item_var_pos][item_shape_pos][part_pos].resize(n_vertices);
                    yv_vars[item_var_pos][item_shape_pos][part_pos].resize(n_vertices);

                    for (Counter vertex_pos = 0; vertex_pos < n_vertices; ++vertex_pos) {
                        // Map rotated vertex index to ICD vertex index.
                        // mirror=false: icd_idx = vertex_pos
                        // mirror=true:  icd_idx = (N - vertex_pos) % N
                        // (axial_symmetry_y_axis reverses element order)
                        const Counter icd_idx = solution_item.mirror
                                ? (n_vertices - vertex_pos) % n_vertices
                                : vertex_pos;
                        const Point& icd_vertex = icd_part.elements[icd_idx].start;
                        const double xm = solution_item.mirror ? -icd_vertex.x : icd_vertex.x;
                        const double ym = icd_vertex.y;

                        const double world_xv =
                                current_x + current_lam * (xm * current_cos - ym * current_sin);
                        const double world_yv =
                                current_y + current_lam * (xm * current_sin + ym * current_cos);

                        xv_vars[item_var_pos][item_shape_pos][part_pos][vertex_pos] =
                                (int)model.variables_lower_bounds.size();
                        model.variables_lower_bounds.push_back(-inf);
                        model.variables_upper_bounds.push_back(+inf);
                        model.variables_initial_values.push_back(world_xv);
                        model.variables_types.push_back(
                                mathoptsolverscmake::VariableType::Continuous);
                        model.objective_coefficients.push_back(0.0);

                        yv_vars[item_var_pos][item_shape_pos][part_pos][vertex_pos] =
                                (int)model.variables_lower_bounds.size();
                        model.variables_lower_bounds.push_back(-inf);
                        model.variables_upper_bounds.push_back(+inf);
                        model.variables_initial_values.push_back(world_yv);
                        model.variables_types.push_back(
                                mathoptsolverscmake::VariableType::Continuous);
                        model.objective_coefficients.push_back(0.0);
                    }
                }
            }
        }

        /////////////////
        // Constraints //
        /////////////////

        // Unit circle: cos^2 + sin^2 = 1
        for (ItemPos item_var_pos = 0;
                item_var_pos < (ItemPos)unfixed_items.size();
                ++item_var_pos) {
            model.constraints_lower_bounds.push_back(1.0);
            model.constraints_upper_bounds.push_back(1.0);
            model.constraints_starts.push_back((int)model.elements_variables.size());
            model.quadratic_elements_constraints_starts.push_back(
                (int)model.quadratic_elements_variables_1.size());
            model.constraints_functions.emplace_back();
            model.quadratic_elements_variables_1.push_back(cos_vars[item_var_pos]);
            model.quadratic_elements_variables_2.push_back(cos_vars[item_var_pos]);
            model.quadratic_elements_coefficients.push_back(1.0);
            model.quadratic_elements_variables_1.push_back(sin_vars[item_var_pos]);
            model.quadratic_elements_variables_2.push_back(sin_vars[item_var_pos]);
            model.quadratic_elements_coefficients.push_back(1.0);
        }

        // Vertex definitions: xv = x + lambda*(xm*cos - ym*sin)
        //                     yv = y + lambda*(xm*sin + ym*cos)
        for (ItemPos item_var_pos = 0;
                item_var_pos < (ItemPos)unfixed_items.size();
                ++item_var_pos) {
            const ItemPos item_pos = unfixed_items[item_var_pos];
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            const ItemTypeId item_type_id = solution_item.item_type_id;

            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)xv_vars[item_var_pos].size();
                    ++item_shape_pos) {
                for (Counter part_pos = 0;
                        part_pos < (Counter)xv_vars[item_var_pos][item_shape_pos].size();
                        ++part_pos) {
                    const Shape& icd_part =
                            icd.item_types[item_type_id][item_shape_pos][part_pos].shape;
                    const Counter n_vertices = (Counter)icd_part.elements.size();

                    for (Counter vertex_pos = 0; vertex_pos < n_vertices; ++vertex_pos) {
                        const Counter icd_idx = solution_item.mirror
                                ? (n_vertices - vertex_pos) % n_vertices
                                : vertex_pos;
                        const Point& icd_vertex = icd_part.elements[icd_idx].start;
                        const double xm = solution_item.mirror ? -icd_vertex.x : icd_vertex.x;
                        const double ym = icd_vertex.y;

                        // xv - x - xm*lambda*cos + ym*lambda*sin = 0
                        model.constraints_lower_bounds.push_back(0.0);
                        model.constraints_upper_bounds.push_back(0.0);
                        model.constraints_starts.push_back((int)model.elements_variables.size());
                        model.quadratic_elements_constraints_starts.push_back(
                            (int)model.quadratic_elements_variables_1.size());
                        model.constraints_functions.emplace_back();
                        model.elements_variables.push_back(
                                xv_vars[item_var_pos][item_shape_pos][part_pos][vertex_pos]);
                        model.elements_coefficients.push_back(+1.0);
                        model.elements_variables.push_back(x_vars[item_var_pos]);
                        model.elements_coefficients.push_back(-1.0);
                        if (!shape::equal(xm, 0.0)) {
                            model.quadratic_elements_variables_1.push_back(
                                    lambda_vars[item_var_pos]);
                            model.quadratic_elements_variables_2.push_back(
                                    cos_vars[item_var_pos]);
                            model.quadratic_elements_coefficients.push_back(-xm);
                        }
                        if (!shape::equal(ym, 0.0)) {
                            model.quadratic_elements_variables_1.push_back(
                                    lambda_vars[item_var_pos]);
                            model.quadratic_elements_variables_2.push_back(
                                    sin_vars[item_var_pos]);
                            model.quadratic_elements_coefficients.push_back(+ym);
                        }

                        // yv - y - xm*lambda*sin - ym*lambda*cos = 0
                        model.constraints_lower_bounds.push_back(0.0);
                        model.constraints_upper_bounds.push_back(0.0);
                        model.constraints_starts.push_back((int)model.elements_variables.size());
                        model.quadratic_elements_constraints_starts.push_back(
                            (int)model.quadratic_elements_variables_1.size());
                        model.constraints_functions.emplace_back();
                        model.elements_variables.push_back(
                                yv_vars[item_var_pos][item_shape_pos][part_pos][vertex_pos]);
                        model.elements_coefficients.push_back(+1.0);
                        model.elements_variables.push_back(y_vars[item_var_pos]);
                        model.elements_coefficients.push_back(-1.0);
                        if (!shape::equal(xm, 0.0)) {
                            model.quadratic_elements_variables_1.push_back(
                                    lambda_vars[item_var_pos]);
                            model.quadratic_elements_variables_2.push_back(
                                    sin_vars[item_var_pos]);
                            model.quadratic_elements_coefficients.push_back(-xm);
                        }
                        if (!shape::equal(ym, 0.0)) {
                            model.quadratic_elements_variables_1.push_back(
                                    lambda_vars[item_var_pos]);
                            model.quadratic_elements_variables_2.push_back(
                                    cos_vars[item_var_pos]);
                            model.quadratic_elements_coefficients.push_back(-ym);
                        }
                    }
                }
            }
        }

        // Bin containment constraints: linear in x, y, lambda, using current rotation's AABBs.
        for (ItemPos item_var_pos = 0;
                item_var_pos < (ItemPos)unfixed_items.size();
                ++item_var_pos) {
            for (ItemShapePos shape_pos = 0;
                    shape_pos < (ItemShapePos)unfixed_item_aabbs[item_var_pos].part_aabbs.size();
                    ++shape_pos) {
                for (Counter part_pos = 0;
                        part_pos < (Counter)unfixed_item_aabbs[item_var_pos].part_aabbs[shape_pos].size();
                        ++part_pos) {
                    const AxisAlignedBoundingBox& pa =
                            unfixed_item_aabbs[item_var_pos].part_aabbs[shape_pos][part_pos];
                    const AxisAlignedBoundingBox& pm =
                            unfixed_item_aabbs[item_var_pos].part_movement_aabbs[shape_pos][part_pos];

                    const LengthDbl x_min_clamped =
                            (std::max)(pm.x_min, bin_type.aabb.x_min * sv);
                    const LengthDbl x_max_clamped =
                            (std::min)(pm.x_max, bin_type.aabb.x_max * sv);
                    const LengthDbl y_min_clamped =
                            (std::max)(pm.y_min, bin_type.aabb.y_min * sv);
                    const LengthDbl y_max_clamped =
                            (std::min)(pm.y_max, bin_type.aabb.y_max * sv);

                    // x_j + pa.x_max * lambda_j <= x_max_clamped
                    model.constraints_lower_bounds.push_back(-inf);
                    model.constraints_upper_bounds.push_back(x_max_clamped);
                    model.constraints_starts.push_back((int)model.elements_variables.size());
                    model.quadratic_elements_constraints_starts.push_back(
                        (int)model.quadratic_elements_variables_1.size());
                    model.constraints_functions.emplace_back();
                    model.elements_variables.push_back(x_vars[item_var_pos]);
                    model.elements_coefficients.push_back(1.0);
                    model.elements_variables.push_back(lambda_vars[item_var_pos]);
                    model.elements_coefficients.push_back(pa.x_max);

                    // x_j + pa.x_min * lambda_j >= x_min_clamped
                    model.constraints_lower_bounds.push_back(x_min_clamped);
                    model.constraints_upper_bounds.push_back(+inf);
                    model.constraints_starts.push_back((int)model.elements_variables.size());
                    model.quadratic_elements_constraints_starts.push_back(
                        (int)model.quadratic_elements_variables_1.size());
                    model.constraints_functions.emplace_back();
                    model.elements_variables.push_back(x_vars[item_var_pos]);
                    model.elements_coefficients.push_back(1.0);
                    model.elements_variables.push_back(lambda_vars[item_var_pos]);
                    model.elements_coefficients.push_back(pa.x_min);

                    // y_j + pa.y_max * lambda_j <= y_max_clamped
                    model.constraints_lower_bounds.push_back(-inf);
                    model.constraints_upper_bounds.push_back(y_max_clamped);
                    model.constraints_starts.push_back((int)model.elements_variables.size());
                    model.quadratic_elements_constraints_starts.push_back(
                        (int)model.quadratic_elements_variables_1.size());
                    model.constraints_functions.emplace_back();
                    model.elements_variables.push_back(y_vars[item_var_pos]);
                    model.elements_coefficients.push_back(1.0);
                    model.elements_variables.push_back(lambda_vars[item_var_pos]);
                    model.elements_coefficients.push_back(pa.y_max);

                    // y_j + pa.y_min * lambda_j >= y_min_clamped
                    model.constraints_lower_bounds.push_back(y_min_clamped);
                    model.constraints_upper_bounds.push_back(+inf);
                    model.constraints_starts.push_back((int)model.elements_variables.size());
                    model.quadratic_elements_constraints_starts.push_back(
                        (int)model.quadratic_elements_variables_1.size());
                    model.constraints_functions.emplace_back();
                    model.elements_variables.push_back(y_vars[item_var_pos]);
                    model.elements_coefficients.push_back(1.0);
                    model.elements_variables.push_back(lambda_vars[item_var_pos]);
                    model.elements_coefficients.push_back(pa.y_min);
                }
            }
        }

        // Separation constraints for item-border pairs.
        for (const IntersectingItemPartDefectPart& entry:
                intersecting_parts.intersecting_item_part_border_part) {
            const ItemPos item_pos = unfixed_items[entry.item_var_pos];
            const Shape& border_shape =
                    icd.bin_types_borders[bin_type_id][entry.defect_pos].shape;
            const Shape& item_part =
                    item_rotated_decompositions[item_pos][entry.item_shape_pos][entry.item_part_pos].shape;
            const EdgeSeparationConstraintParameters sep = find_best_edge_separator(
                    border_shape,
                    {0.0, 0.0},
                    1.0,
                    item_part,
                    {solution_bin.items[item_pos].bl_corner.x * sv,
                     solution_bin.items[item_pos].bl_corner.y * sv},
                    current_lambda[item_pos]);

            const Counter n_item_vertices = (Counter)item_part.elements.size();
            PointRef edge_start_ref;
            PointRef edge_end_ref;
            PointRef point_ref;

            if (sep.edge_shape_pos == 0) {
                // Edge from border (fixed), point from item (variable).
                edge_start_ref.is_fixed = true;
                edge_start_ref.const_x = border_shape.elements[sep.edge_element_pos].start.x;
                edge_start_ref.const_y = border_shape.elements[sep.edge_element_pos].start.y;
                edge_end_ref.is_fixed = true;
                edge_end_ref.const_x = border_shape.elements[sep.edge_element_pos].end.x;
                edge_end_ref.const_y = border_shape.elements[sep.edge_element_pos].end.y;
                point_ref.is_fixed = false;
                point_ref.xv_var_id = xv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][sep.point_element_pos];
                point_ref.yv_var_id = yv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][sep.point_element_pos];
            } else {
                // Edge from item (variable), point from border (fixed).
                const Counter edge_end_vertex_pos =
                        (sep.edge_element_pos + 1) % n_item_vertices;
                edge_start_ref.is_fixed = false;
                edge_start_ref.xv_var_id = xv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][sep.edge_element_pos];
                edge_start_ref.yv_var_id = yv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][sep.edge_element_pos];
                edge_end_ref.is_fixed = false;
                edge_end_ref.xv_var_id = xv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][edge_end_vertex_pos];
                edge_end_ref.yv_var_id = yv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][edge_end_vertex_pos];
                point_ref.is_fixed = true;
                point_ref.const_x = border_shape.elements[sep.point_element_pos].start.x;
                point_ref.const_y = border_shape.elements[sep.point_element_pos].start.y;
            }
            add_cross_product_constraint(model,
                    edge_start_ref, edge_end_ref, point_ref);
        }

        // Separation constraints for item-defect pairs.
        for (const IntersectingItemPartDefectPart& entry:
                intersecting_parts.intersecting_item_part_defect_part) {
            const ItemPos item_pos = unfixed_items[entry.item_var_pos];
            const Shape& defect_shape =
                    icd.bin_types_defects[bin_type_id][entry.defect_pos].shape;
            const Shape& item_part =
                    item_rotated_decompositions[item_pos][entry.item_shape_pos][entry.item_part_pos].shape;
            const EdgeSeparationConstraintParameters sep = find_best_edge_separator(
                    defect_shape,
                    {0.0, 0.0},
                    1.0,
                    item_part,
                    {solution_bin.items[item_pos].bl_corner.x * sv,
                     solution_bin.items[item_pos].bl_corner.y * sv},
                    current_lambda[item_pos]);

            const Counter n_item_vertices = (Counter)item_part.elements.size();
            PointRef edge_start_ref;
            PointRef edge_end_ref;
            PointRef point_ref;

            if (sep.edge_shape_pos == 0) {
                edge_start_ref.is_fixed = true;
                edge_start_ref.const_x = defect_shape.elements[sep.edge_element_pos].start.x;
                edge_start_ref.const_y = defect_shape.elements[sep.edge_element_pos].start.y;
                edge_end_ref.is_fixed = true;
                edge_end_ref.const_x = defect_shape.elements[sep.edge_element_pos].end.x;
                edge_end_ref.const_y = defect_shape.elements[sep.edge_element_pos].end.y;
                point_ref.is_fixed = false;
                point_ref.xv_var_id = xv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][sep.point_element_pos];
                point_ref.yv_var_id = yv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][sep.point_element_pos];
            } else {
                const Counter edge_end_vertex_pos =
                        (sep.edge_element_pos + 1) % n_item_vertices;
                edge_start_ref.is_fixed = false;
                edge_start_ref.xv_var_id = xv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][sep.edge_element_pos];
                edge_start_ref.yv_var_id = yv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][sep.edge_element_pos];
                edge_end_ref.is_fixed = false;
                edge_end_ref.xv_var_id = xv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][edge_end_vertex_pos];
                edge_end_ref.yv_var_id = yv_vars[entry.item_var_pos][entry.item_shape_pos][entry.item_part_pos][edge_end_vertex_pos];
                point_ref.is_fixed = true;
                point_ref.const_x = defect_shape.elements[sep.point_element_pos].start.x;
                point_ref.const_y = defect_shape.elements[sep.point_element_pos].start.y;
            }
            add_cross_product_constraint(model,
                    edge_start_ref, edge_end_ref, point_ref);
        }

        // Separation constraints for item-item pairs.
        for (const IntersectingItemParts& entry: intersecting_parts.intersecting_item_parts) {
            const ItemPos item_1_pos = unfixed_items[entry.item_1_var_pos];
            const ItemPos item_2_pos = unfixed_items[entry.item_2_var_pos];
            const Shape& item_part_1 =
                    item_rotated_decompositions[item_1_pos][entry.item_shape_1_pos][entry.item_part_1_pos].shape;
            const Shape& item_part_2 =
                    item_rotated_decompositions[item_2_pos][entry.item_shape_2_pos][entry.item_part_2_pos].shape;
            const SolutionItem& sol_item_1 = solution_bin.items[item_1_pos];
            const SolutionItem& sol_item_2 = solution_bin.items[item_2_pos];
            const EdgeSeparationConstraintParameters sep = find_best_edge_separator(
                    item_part_1,
                    {sol_item_1.bl_corner.x * sv, sol_item_1.bl_corner.y * sv},
                    current_lambda[item_1_pos],
                    item_part_2,
                    {sol_item_2.bl_corner.x * sv, sol_item_2.bl_corner.y * sv},
                    current_lambda[item_2_pos]);

            PointRef edge_start_ref;
            PointRef edge_end_ref;
            PointRef point_ref;

            if (sep.edge_shape_pos == 0) {
                // Edge from item_1, point from item_2.
                const Counter n_vertices_1 = (Counter)item_part_1.elements.size();
                const Counter edge_end_vertex_pos =
                        (sep.edge_element_pos + 1) % n_vertices_1;
                edge_start_ref.is_fixed = false;
                edge_start_ref.xv_var_id = xv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][sep.edge_element_pos];
                edge_start_ref.yv_var_id = yv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][sep.edge_element_pos];
                edge_end_ref.is_fixed = false;
                edge_end_ref.xv_var_id = xv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][edge_end_vertex_pos];
                edge_end_ref.yv_var_id = yv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][edge_end_vertex_pos];
                point_ref.is_fixed = false;
                point_ref.xv_var_id = xv_vars[entry.item_2_var_pos][entry.item_shape_2_pos][entry.item_part_2_pos][sep.point_element_pos];
                point_ref.yv_var_id = yv_vars[entry.item_2_var_pos][entry.item_shape_2_pos][entry.item_part_2_pos][sep.point_element_pos];
            } else {
                // Edge from item_2, point from item_1.
                const Counter n_vertices_2 = (Counter)item_part_2.elements.size();
                const Counter edge_end_vertex_pos =
                        (sep.edge_element_pos + 1) % n_vertices_2;
                edge_start_ref.is_fixed = false;
                edge_start_ref.xv_var_id = xv_vars[entry.item_2_var_pos][entry.item_shape_2_pos][entry.item_part_2_pos][sep.edge_element_pos];
                edge_start_ref.yv_var_id = yv_vars[entry.item_2_var_pos][entry.item_shape_2_pos][entry.item_part_2_pos][sep.edge_element_pos];
                edge_end_ref.is_fixed = false;
                edge_end_ref.xv_var_id = xv_vars[entry.item_2_var_pos][entry.item_shape_2_pos][entry.item_part_2_pos][edge_end_vertex_pos];
                edge_end_ref.yv_var_id = yv_vars[entry.item_2_var_pos][entry.item_shape_2_pos][entry.item_part_2_pos][edge_end_vertex_pos];
                point_ref.is_fixed = false;
                point_ref.xv_var_id = xv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][sep.point_element_pos];
                point_ref.yv_var_id = yv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][sep.point_element_pos];
            }
            add_cross_product_constraint(model,
                    edge_start_ref, edge_end_ref, point_ref);
        }

        // Separation constraints for item-fixed-item pairs.
        for (const IntersectingItemParts& entry:
                intersecting_parts.intersecting_item_part_fixed_item_part) {
            const ItemPos item_1_pos = unfixed_items[entry.item_1_var_pos];
            const ItemPos item_2_pos = fixed_items[entry.item_2_var_pos];
            const Shape& item_part =
                    item_rotated_decompositions[item_1_pos][entry.item_shape_1_pos][entry.item_part_1_pos].shape;
            const Shape& fixed_item_part =
                    item_rotated_decompositions[item_2_pos][entry.item_shape_2_pos][entry.item_part_2_pos].shape;
            const SolutionItem& sol_item_1 = solution_bin.items[item_1_pos];
            const SolutionItem& sol_item_2 = solution_bin.items[item_2_pos];
            const EdgeSeparationConstraintParameters sep = find_best_edge_separator(
                    item_part,
                    {sol_item_1.bl_corner.x * sv, sol_item_1.bl_corner.y * sv},
                    current_lambda[item_1_pos],
                    fixed_item_part,
                    {sol_item_2.bl_corner.x * sv, sol_item_2.bl_corner.y * sv},
                    1.0);

            const Counter n_item_vertices = (Counter)item_part.elements.size();
            const Counter n_fixed_vertices = (Counter)fixed_item_part.elements.size();
            PointRef edge_start_ref;
            PointRef edge_end_ref;
            PointRef point_ref;

            if (sep.edge_shape_pos == 0) {
                // Edge from unfixed item (variable), point from fixed item (constant).
                const Counter edge_end_vertex_pos =
                        (sep.edge_element_pos + 1) % n_item_vertices;
                edge_start_ref.is_fixed = false;
                edge_start_ref.xv_var_id = xv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][sep.edge_element_pos];
                edge_start_ref.yv_var_id = yv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][sep.edge_element_pos];
                edge_end_ref.is_fixed = false;
                edge_end_ref.xv_var_id = xv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][edge_end_vertex_pos];
                edge_end_ref.yv_var_id = yv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][edge_end_vertex_pos];
                const Point& fixed_vertex = fixed_item_part.elements[sep.point_element_pos].start;
                point_ref.is_fixed = true;
                point_ref.const_x = sol_item_2.bl_corner.x * sv + fixed_vertex.x;
                point_ref.const_y = sol_item_2.bl_corner.y * sv + fixed_vertex.y;
            } else {
                // Edge from fixed item (constant), point from unfixed item (variable).
                const Counter edge_end_vertex_pos =
                        (sep.edge_element_pos + 1) % n_fixed_vertices;
                edge_start_ref.is_fixed = true;
                edge_start_ref.const_x = sol_item_2.bl_corner.x * sv
                        + fixed_item_part.elements[sep.edge_element_pos].start.x;
                edge_start_ref.const_y = sol_item_2.bl_corner.y * sv
                        + fixed_item_part.elements[sep.edge_element_pos].start.y;
                edge_end_ref.is_fixed = true;
                edge_end_ref.const_x = sol_item_2.bl_corner.x * sv
                        + fixed_item_part.elements[sep.edge_element_pos].end.x;
                edge_end_ref.const_y = sol_item_2.bl_corner.y * sv
                        + fixed_item_part.elements[sep.edge_element_pos].end.y;
                point_ref.is_fixed = false;
                point_ref.xv_var_id = xv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][sep.point_element_pos];
                point_ref.yv_var_id = yv_vars[entry.item_1_var_pos][entry.item_shape_1_pos][entry.item_part_1_pos][sep.point_element_pos];
            }
            add_cross_product_constraint(model,
                    edge_start_ref, edge_end_ref, point_ref);
        }

        ///////////
        // Solve //
        ///////////

        knitrocpp::Context knitro_context;
        knitro_context.set_int_param_by_name("outlev", 0);
        mathoptsolverscmake::solve(model, knitro_context);
        double new_value = mathoptsolverscmake::get_solution_value(knitro_context);

        if (!shape::strictly_greater(new_value, current_value))
            break;

        // Retrieve solution.
        const std::vector<double> solution_values =
                mathoptsolverscmake::get_solution(knitro_context);

        std::vector<double> new_lambda(solution_bin.items.size(), 1.0);
        std::vector<bool> new_items_shrunken(solution_bin.items.size(), false);
        for (ItemPos item_var_pos = 0;
                item_var_pos < (ItemPos)unfixed_items.size();
                ++item_var_pos) {
            const ItemPos item_pos = unfixed_items[item_var_pos];
            new_lambda[item_pos] = solution_values[lambda_vars[item_var_pos]];
            new_items_shrunken[item_pos] = new_lambda[item_pos] < 1.0 - 1e-6;
        }

        Solution new_solution(instance);
        new_solution.add_bin(solution_bin.bin_type_id, 1);
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            if (solution_item.is_fixed) {
                new_solution.add_item(
                        0,
                        solution_item.item_type_id,
                        solution_item.bl_corner,
                        solution_item.angle,
                        solution_item.mirror,
                        true);
            } else {
                const ItemPos item_var_pos = item_to_var_pos[item_pos];
                const double new_x = solution_values[x_vars[item_var_pos]] / sv;
                const double new_y = solution_values[y_vars[item_var_pos]] / sv;
                const double new_cos = solution_values[cos_vars[item_var_pos]];
                const double new_sin = solution_values[sin_vars[item_var_pos]];
                const double new_angle = std::atan2(new_sin, new_cos) * 180.0 / M_PI;
                new_solution.add_item(
                        0,
                        solution_item.item_type_id,
                        {new_x, new_y},
                        new_angle,
                        solution_item.mirror);
            }
        }

        solution = new_solution;
        current_value = new_value;
        current_items_shrunken = new_items_shrunken;
        current_lambda = new_lambda;
    }

    output.solution = solution;
    output.items_shrunken = current_items_shrunken;
    output.final_lambda = current_lambda;
    output.feasible = std::none_of(
            current_items_shrunken.begin(),
            current_items_shrunken.end(),
            [](bool shrunken) { return shrunken; });

    return output;
#endif  // KNITRO_FOUND
}

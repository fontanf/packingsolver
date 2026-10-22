#include "irregular/linear_programming/anchor.hpp"

#include "irregular/utils.hpp"
#include "irregular/solution_builder.hpp"

#ifdef CBC_FOUND
#include "mathoptsolverscmake/mathopt_cbc.hpp"
#endif
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif
#ifdef XPRESS_FOUND
#include "mathoptsolverscmake/mathopt_xpress.hpp"
#endif

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

Solution linear_programming_anchor(
        const Instance& instance,
        const Solution& initial_solution,
        double x_weight,
        double y_weight,
        const LinearProgrammingAnchorParameters& parameters,
        const InstanceBasicShapeDecomposition& icd)
{
    // Check that the solution has a single bin.
    if (initial_solution.number_of_bins() != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE);
    }

    // Get bin type.
    const BinPos bin_pos = 0;
    const BinTypeId bin_type_id = initial_solution.bin(bin_pos).bin_type_id;
    const BinType& bin_type = instance.bin_type(bin_type_id);

    // Compute fixed and unfixed items.
    std::vector<ItemPos> fixed_items;
    std::vector<ItemPos> unfixed_items;
    for (ItemPos item_pos = 0;
            item_pos < (ItemPos)initial_solution.bin(bin_pos).items.size();
            ++item_pos) {
        const SolutionItem& solution_item = initial_solution.bin(bin_pos).items[item_pos];
        if (solution_item.is_fixed) {
            fixed_items.push_back(item_pos);
        } else {
            unfixed_items.push_back(item_pos);
        }
    }

    // Precompute rotated convex decompositions for each solution item.
    // item_rotated_decompositions[item_pos][item_shape_pos] = list of convex parts
    // with the item's angle and mirror already applied.
    std::vector<std::vector<std::vector<BasicShape>>> item_rotated_decompositions(initial_solution.bin(bin_pos).items.size());
    for (ItemPos item_pos = 0;
            item_pos < (ItemPos)initial_solution.bin(bin_pos).items.size();
            ++item_pos) {
        const SolutionItem& solution_item = initial_solution.bin(bin_pos).items[item_pos];
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

    // Precompute AABBs for fixed items, border and defect convex parts (fixed world coordinates).
    std::vector<AxisAlignedBoundingBox> fixed_item_part_aabbs;
    for (ItemPos item_pos = 0;
            item_pos < (ItemPos)initial_solution.bin(bin_pos).items.size();
            ++item_pos) {
        const SolutionItem& solution_item = initial_solution.bin(bin_pos).items[item_pos];
        if (!solution_item.is_fixed)
            continue;
        const LengthDbl cx = solution_item.bl_corner.x * instance.parameters().scale_value;
        const LengthDbl cy = solution_item.bl_corner.y * instance.parameters().scale_value;
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_rotated_decompositions[item_pos].size();
                ++item_shape_pos) {
            const auto& parts = item_rotated_decompositions[item_pos][item_shape_pos];
            for (Counter part_pos = 0; part_pos < (Counter)parts.size(); ++part_pos) {
                const AxisAlignedBoundingBox p = parts[part_pos].shape.compute_min_max();
                AxisAlignedBoundingBox ps;
                ps.x_min = cx + p.x_min;
                ps.x_max = cx + p.x_max;
                ps.y_min = cy + p.y_min;
                ps.y_max = cy + p.y_max;
                fixed_item_part_aabbs.push_back(ps);
            }
        }
    }
    std::vector<AxisAlignedBoundingBox> border_part_aabbs;
    for (const BasicShape& part: icd.bin_types_borders[bin_type_id])
        border_part_aabbs.push_back(part.shape.compute_min_max());
    std::vector<AxisAlignedBoundingBox> defect_part_aabbs;
    for (const BasicShape& part: icd.bin_types_defects[bin_type_id])
        defect_part_aabbs.push_back(part.shape.compute_min_max());

    LengthDbl movement_box_half_size = 10;

    Solution solution = initial_solution;
    double current_value = 0;
    for (const SolutionItem& solution_item: initial_solution.bin(bin_pos).items) {
        current_value += x_weight * solution_item.bl_corner.x * instance.parameters().scale_value;
        current_value += y_weight * solution_item.bl_corner.y * instance.parameters().scale_value;
    }
    // Lambda is fixed at 1 for anchor_to_corner (no shrinkage variables).
    const std::vector<double> anchor_lambda(initial_solution.bin(bin_pos).items.size(), 1.0);

    for (Counter number_of_iterations = 0;; ++number_of_iterations) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);

        const std::vector<ItemAxisAlignedBoundingBoxes> unfixed_item_aabbs = compute_unfixed_item_aabbs(
                instance,
                solution_bin,
                movement_box_half_size,
                item_rotated_decompositions,
                anchor_lambda);

        // Precompute per-item bin bounds (without movement restriction).
        // The local bounding box of each item is read from item_aabbs.
        std::vector<AxisAlignedBoundingBox> item_bin_bounds(unfixed_items.size());
        for (ItemPos item_var_pos = 0;
                item_var_pos < (ItemPos)unfixed_items.size();
                ++item_var_pos) {
            const AxisAlignedBoundingBox& ia = unfixed_item_aabbs[item_var_pos].item_aabb;
            LengthDbl x_min = (x_weight < 0)? solution.x_min(): bin_type.aabb_orig.x_min;
            LengthDbl x_max = (x_weight > 0)? solution.x_max(): bin_type.aabb_orig.x_max;
            LengthDbl y_min = (y_weight < 0)? solution.y_min(): bin_type.aabb_orig.y_min;
            LengthDbl y_max = (y_weight > 0)? solution.y_max(): bin_type.aabb_orig.y_max;
            item_bin_bounds[item_var_pos].x_min = x_min * instance.parameters().scale_value - ia.x_min;
            item_bin_bounds[item_var_pos].x_max = x_max * instance.parameters().scale_value - ia.x_max;
            item_bin_bounds[item_var_pos].y_min = y_min * instance.parameters().scale_value - ia.y_min;
            item_bin_bounds[item_var_pos].y_max = y_max * instance.parameters().scale_value - ia.y_max;
        }

        IntersectingParts intersecting_parts = compute_potentially_intersecting_parts(
                unfixed_item_aabbs,
                fixed_item_part_aabbs,
                border_part_aabbs,
                defect_part_aabbs);

        ////////////////////
        // Setup LP model //
        ////////////////////

        // Build LP model.
        mathoptsolverscmake::MathOptModel lp_model;

        /////////////////////////////
        // Objective and variables //
        /////////////////////////////

        // Set objective sense.
        lp_model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Minimize;

        std::vector<int> x(solution_bin.items.size(), -1);
        std::vector<int> y(solution_bin.items.size(), -1);
        for (ItemPos item_var_pos = 0;
                item_var_pos < (ItemPos)unfixed_items.size();
                ++item_var_pos) {
            ItemPos item_pos = unfixed_items[item_var_pos];
            const SolutionItem& solution_item = solution_bin.items[item_pos];

            x[item_pos] = lp_model.variables_lower_bounds.size();
            lp_model.variables_lower_bounds.push_back(std::max(
                    item_bin_bounds[item_var_pos].x_min,
                    solution_item.bl_corner.x * instance.parameters().scale_value - movement_box_half_size));
            lp_model.variables_upper_bounds.push_back(std::min(
                    item_bin_bounds[item_var_pos].x_max,
                    solution_item.bl_corner.x * instance.parameters().scale_value +movement_box_half_size));
            lp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            lp_model.objective_coefficients.push_back(x_weight);
            lp_model.variables_names.push_back("x_" + std::to_string(item_pos));

            y[item_pos] = lp_model.variables_lower_bounds.size();
            lp_model.variables_lower_bounds.push_back(std::max(
                    item_bin_bounds[item_var_pos].y_min,
                    solution_item.bl_corner.y * instance.parameters().scale_value - movement_box_half_size));
            lp_model.variables_upper_bounds.push_back(std::min(
                    item_bin_bounds[item_var_pos].y_max,
                    solution_item.bl_corner.y * instance.parameters().scale_value + movement_box_half_size));
            lp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            lp_model.objective_coefficients.push_back(y_weight);
            lp_model.variables_names.push_back("y_" + std::to_string(item_pos));
        }

        /////////////////
        // Constraints //
        /////////////////

        // Obstacle (shape_1) is at world origin with scale=1 (fixed).
        // Item (shape_2) has LP variables x, y at scale=1 (no lambda in atc).
        // Full constraint: coef_x1*0 + coef_y1*0 + coef_lambda1 + coef_x2*x + coef_y2*y + coef_lambda2 >= 0
        //  => coef_x2*x + coef_y2*y >= -(coef_lambda1 + coef_lambda2)

        const LengthDbl sv = instance.parameters().scale_value;

        // Intersections between an unifxed item and a fixed item.
        for (const IntersectingItemParts& e: intersecting_parts.intersecting_item_part_fixed_item_part) {
            ItemPos item_1_pos = unfixed_items[e.item_1_var_pos];
            ItemPos item_2_pos = fixed_items[e.item_2_var_pos];
            const SolutionItem& item_1 = solution_bin.items[item_1_pos];
            const SolutionItem& item_2 = solution_bin.items[item_2_pos];
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    item_rotated_decompositions[item_1_pos][e.item_shape_1_pos][e.item_part_1_pos].shape,
                    sv * item_1.bl_corner,
                    1.0,
                    item_rotated_decompositions[item_2_pos][e.item_shape_2_pos][e.item_part_2_pos].shape,
                    sv * item_2.bl_corner,
                    1.0);
            if (shape::equal(p.coef_x1, 0.0) && shape::equal(p.coef_y1, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_names.push_back(
                    "sep_fixed_i_" + std::to_string(item_1_pos)
                    + "_s" + std::to_string(e.item_shape_1_pos)
                    + "_p" + std::to_string(e.item_part_1_pos)
                    + "_i" + std::to_string(item_2_pos)
                    + "_s" + std::to_string(e.item_shape_2_pos)
                    + "_p" + std::to_string(e.item_part_2_pos));
            lp_model.constraints_lower_bounds.push_back(
                    -p.coef_x2 * item_2.bl_corner.x * sv
                    -p.coef_y2 * item_2.bl_corner.y * sv
                    -p.coef_lambda2
                    -p.coef_lambda1);
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.coef_x1, 0.0)) {
                lp_model.elements_variables.push_back(x[item_1_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x1);
            }
            if (!shape::equal(p.coef_y1, 0.0)) {
                lp_model.elements_variables.push_back(y[item_1_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y1);
            }
        }

        // Intersections between items and bin borders.
        for (const IntersectingItemPartDefectPart& e: intersecting_parts.intersecting_item_part_border_part) {
            ItemPos item_pos = unfixed_items[e.item_var_pos];
            const SolutionItem& item = solution_bin.items[item_pos];
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    item_rotated_decompositions[item_pos][e.item_shape_pos][e.item_part_pos].shape,
                    sv * item.bl_corner,
                    1.0,
                    icd.bin_types_borders[bin_type_id][e.defect_pos].shape,
                    Point{0, 0},
                    1.0);
            if (shape::equal(p.coef_x1, 0.0) && shape::equal(p.coef_y1, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_names.push_back(
                    "border_i" + std::to_string(item_pos)
                    + "_s" + std::to_string(e.item_shape_pos)
                    + "_p" + std::to_string(e.item_part_pos)
                    + "_bp" + std::to_string(e.defect_pos));
            lp_model.constraints_lower_bounds.push_back(-p.coef_lambda2 - p.coef_lambda1);
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.coef_x1, 0.0)) {
                lp_model.elements_variables.push_back(x[item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x1);
            }
            if (!shape::equal(p.coef_y1, 0.0)) {
                lp_model.elements_variables.push_back(y[item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y1);
            }
        }

        // Intersections between items and bin defects.
        for (const IntersectingItemPartDefectPart& e: intersecting_parts.intersecting_item_part_defect_part) {
            ItemPos item_pos = unfixed_items[e.item_var_pos];
            const SolutionItem& item = solution_bin.items[item_pos];
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    item_rotated_decompositions[item_pos][e.item_shape_pos][e.item_part_pos].shape,
                    sv * item.bl_corner,
                    1.0,
                    icd.bin_types_defects[bin_type_id][e.defect_pos].shape,
                    Point{0, 0},
                    1.0);
            if (shape::equal(p.coef_x1, 0.0) && shape::equal(p.coef_y1, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_names.push_back(
                    "defect_i" + std::to_string(e.item_var_pos)
                    + "_s" + std::to_string(e.item_shape_pos)
                    + "_p" + std::to_string(e.item_part_pos)
                    + "_dp" + std::to_string(e.defect_pos));
            lp_model.constraints_lower_bounds.push_back(-p.coef_lambda2 - p.coef_lambda1);
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.coef_x1, 0.0)) {
                lp_model.elements_variables.push_back(x[item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x1);
            }
            if (!shape::equal(p.coef_y1, 0.0)) {
                lp_model.elements_variables.push_back(y[item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y1);
            }
        }

        // Intersections between pairs of items.
        // Full constraint: coef_x1*x1 + coef_y1*y1 + coef_lambda1 + coef_x2*x2 + coef_y2*y2 + coef_lambda2 >= 0
        //  => coef_x1*x1 + coef_y1*y1 + coef_x2*x2 + coef_y2*y2 >= -(coef_lambda1 + coef_lambda2)
        for (const IntersectingItemParts& e: intersecting_parts.intersecting_item_parts) {
            ItemPos item_1_pos = unfixed_items[e.item_1_var_pos];
            ItemPos item_2_pos = unfixed_items[e.item_2_var_pos];
            const SolutionItem& item_1 = solution_bin.items[item_1_pos];
            const SolutionItem& item_2 = solution_bin.items[item_2_pos];
            const Shape& convex_part_1 = item_rotated_decompositions[item_1_pos][e.item_shape_1_pos][e.item_part_1_pos].shape;
            const Shape& convex_part_2 = item_rotated_decompositions[item_2_pos][e.item_shape_2_pos][e.item_part_2_pos].shape;
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    convex_part_1,
                    sv * item_1.bl_corner,
                    1.0,  // scale
                    convex_part_2,
                    sv * item_2.bl_corner,
                    1.0);  // scale
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_names.push_back(
                    "sep_i" + std::to_string(e.item_1_var_pos)
                    + "_s" + std::to_string(e.item_shape_1_pos)
                    + "_p" + std::to_string(e.item_part_1_pos)
                    + "_i" + std::to_string(e.item_2_var_pos)
                    + "_s" + std::to_string(e.item_shape_2_pos)
                    + "_p" + std::to_string(e.item_part_2_pos));
            lp_model.constraints_lower_bounds.push_back(-(p.coef_lambda1 + p.coef_lambda2));
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.coef_x1, 0.0)) {
                lp_model.elements_variables.push_back(x[item_1_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x1);
            }
            if (!shape::equal(p.coef_y1, 0.0)) {
                lp_model.elements_variables.push_back(y[item_1_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y1);
            }
            if (!shape::equal(p.coef_x2, 0.0)) {
                lp_model.elements_variables.push_back(x[item_2_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x2);
            }
            if (!shape::equal(p.coef_y2, 0.0)) {
                lp_model.elements_variables.push_back(y[item_2_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y2);
            }
        }

        //lp_model.feasiblity_tolerance = 1e-3;
        //lp_model.check(1);
        //lp_model.format(std::cout, 4);

        std::vector<double> lp_initial_solution(lp_model.variables_lower_bounds.size());
        for (ItemPos item_var_pos = 0;
                item_var_pos < (ItemPos)unfixed_items.size();
                ++item_var_pos) {
            ItemPos item_pos = unfixed_items[item_var_pos];
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            lp_initial_solution[x[item_pos]] = solution_item.bl_corner.x * sv;
            lp_initial_solution[y[item_pos]] = solution_item.bl_corner.y * sv;
        }
        lp_model.variables_initial_values = lp_initial_solution;
        //lp_model.format_solution(std::cout, lp_initial_solution, 4);
        //lp_model.check_solution(lp_initial_solution, 1);

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
            //std::cout << "LP solve start" << std::endl;
            mathoptsolverscmake::solve(highs);
            //std::cout << "LP solve end" << std::endl;
            if (highs.getModelStatus() != HighsModelStatus::kOptimal) {
                // If the time limit is reached, keep the current solution.
                if (parameters.timer.needs_to_end())
                    break;
                throw std::runtime_error(
                        FUNC_SIGNATURE + ": LP not solved to optimality; "
                        "model status: " + highs.modelStatusToString(highs.getModelStatus()) + ".");
            }
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
        SolutionBuilder new_solution_builder(instance);
        new_solution_builder.add_bin(solution_bin.bin_type_id, 1);
        for (ItemPos pos = 0; pos < (ItemPos)solution_bin.items.size(); ++pos) {
            const SolutionItem& solution_item = solution_bin.items[pos];
            if (solution_item.is_fixed) {
                new_solution_builder.add_item(
                        0,
                        solution_item.item_type_id,
                        solution_item.bl_corner,
                        solution_item.angle,
                        solution_item.mirror,
                        true);
            } else {
                new_solution_builder.add_item(
                        0,
                        solution_item.item_type_id,
                        {lp_solution[x[pos]] / instance.parameters().scale_value, lp_solution[y[pos]] / instance.parameters().scale_value},
                        solution_item.angle,
                        solution_item.mirror);
            }
        }
        solution = new_solution_builder.build();
    }

    return solution;
}

}

LinearProgrammingAnchorOutput packingsolver::irregular::linear_programming_anchor(
        const Solution& initial_solution,
        double x_weight,
        double y_weight,
        const LinearProgrammingAnchorParameters& parameters)
{
    //std::cout << "linear_programming" << std::endl;
    const Instance& instance = initial_solution.instance();
    LinearProgrammingAnchorOutput output(instance);

    const InstanceBasicShapeDecomposition icd = compute_instance_basic_shape_decomposition(instance);

    Solution solution(instance);
    for (BinPos bin_pos = 0;
            bin_pos < initial_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = initial_solution.bin(bin_pos);
        Solution initial_solution_cur(instance);
        initial_solution_cur.append_bin(initial_solution, bin_pos, 1);
        Solution new_solution_cur = ::linear_programming_anchor(
                instance,
                initial_solution_cur,
                x_weight,
                y_weight,
                parameters,
                icd);
        solution.append_bin(
                new_solution_cur,
                0,
                solution_bin.copies);
    }
    //initial_solution.format(std::cout, 1);
    //solution.format(std::cout, 1);
    output.solution = solution;

    //instance.write("lp_test.json");
    //initial_solution.write("lp_test_initial.json");
    //solution.write("lp_test_expected.json");
    //exit(1);
    //std::cout << "linear_programming end" << std::endl;
    return output;
}

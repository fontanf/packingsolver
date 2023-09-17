#include "packingsolver/irregular/nlp_circle.hpp"

#if KNITRO_FOUND
#include "knitrocpp/knitro.hpp"
#endif

using namespace packingsolver;
using namespace packingsolver::irregular;

NlpCircleOutput irregular::nlp_circle(
        const Instance& instance,
        NlpCircleOptionalParameters parameters)
{
    NlpCircleOutput output(instance);

#if KNITRO_FOUND

    // Create a new Knitro context.
    knitrocpp::Context knitro_context;

    // Variables.
    struct Variables
    {
        std::vector<knitrocpp::VariableId> x;
        std::vector<knitrocpp::VariableId> y;
        std::vector<knitrocpp::VariableId> lambda;
    };
    Variables variables;

    const BinType& bin_type = instance.bin_type(0);
    if (!bin_type.shape.is_rectangle()) {
        throw std::invalid_argument(
                "NlpCircle algorithm requires bin types to"
                " have a shape of type 'Rectangle'.");
    }

    std::mt19937_64 generator;
    LengthDbl width = bin_type.shape.elements[0].length();
    LengthDbl height = bin_type.shape.elements[1].length();
    std::uniform_real_distribution<LengthDbl> d_x(0, width);
    std::uniform_real_distribution<LengthDbl> d_y(0, height);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.shape_type() != ShapeType::Circle) {
            throw std::invalid_argument(
                    "NlpCircle algorithm requires bin types to"
                    " have a shape of type 'Circle'.");
        }

        knitrocpp::VariableId variable_id = knitro_context.add_var();
        variables.x.push_back(variable_id);

        knitro_context.set_var_primal_init_value(
                variable_id,
                d_x(generator));
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {

        knitrocpp::VariableId variable_id = knitro_context.add_var();
        variables.y.push_back(variable_id);

        knitro_context.set_var_primal_init_value(
                variable_id,
                d_y(generator));
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        knitrocpp::VariableId variable_id = knitro_context.add_var();
        variables.lambda.push_back(variable_id);
        knitro_context.set_var_lobnd(
                variable_id,
                0.0);
        knitro_context.set_var_upbnd(
                variable_id,
                1.0);
        knitro_context.set_var_primal_init_value(
                variable_id,
                0);
    }

    // Objective: minimize x_max.
    knitro_context.set_obj_goal(KN_OBJGOAL_MAXIMIZE);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        knitro_context.add_obj_linear_term(
                variables.lambda[item_type_id],
                1);
    }

    // Constraints: intersections between items.
    // For each items j1, j2 = 1..N, j1 != j2
    //        (xc[j2] - xc[j1])^2 + (yc[j2] - yc[j1])^2
    //        >= (lambda_j1 rc_j1 + lambda_j2 rc_j2)^2
    //   <=>  xc[j1]^2 + xc[j2]^2 - 2 xc[j1] xc[j2]
    //        + yc[j1]^2 + yc[j2]^2 - 2 yc[j1] yc[j2]
    //        - rc_j1^2 lambda_j1^2
    //        - rc_j2^2 lambda_j2^2
    //        - rc_j1 rc_j2 lambda_j1 lambda_j2
    //        >= 0
    for (ItemTypeId item_type_id_1 = 0;
            item_type_id_1 < instance.number_of_item_types();
            ++item_type_id_1) {
        const ItemType& item_type_1 = instance.item_type(item_type_id_1);
        LengthDbl radius_1 = distance(
                item_type_1.shapes.front().shape.elements.front().center,
                item_type_1.shapes.front().shape.elements.front().start);
        for (ItemTypeId item_type_id_2 = item_type_id_1 + 1;
                item_type_id_2 < instance.number_of_item_types();
                ++item_type_id_2) {
            const ItemType& item_type_2 = instance.item_type(item_type_id_2);
            LengthDbl radius_2 = distance(
                    item_type_2.shapes.front().shape.elements.front().center,
                    item_type_2.shapes.front().shape.elements.front().start);

            knitrocpp::ConstraintId constraint_id = knitro_context.add_con();

            knitro_context.add_con_quadratic_term(
                    constraint_id,
                    variables.x[item_type_id_1],
                    variables.x[item_type_id_1],
                    1);
            knitro_context.add_con_quadratic_term(
                    constraint_id,
                    variables.x[item_type_id_2],
                    variables.x[item_type_id_2],
                    1);
            knitro_context.add_con_quadratic_term(
                    constraint_id,
                    variables.x[item_type_id_1],
                    variables.x[item_type_id_2],
                    -2);

            knitro_context.add_con_quadratic_term(
                    constraint_id,
                    variables.y[item_type_id_1],
                    variables.y[item_type_id_1],
                    1);
            knitro_context.add_con_quadratic_term(
                    constraint_id,
                    variables.y[item_type_id_2],
                    variables.y[item_type_id_2],
                    1);
            knitro_context.add_con_quadratic_term(
                    constraint_id,
                    variables.y[item_type_id_1],
                    variables.y[item_type_id_2],
                    -2);

            knitro_context.add_con_quadratic_term(
                    constraint_id,
                    variables.lambda[item_type_id_1],
                    variables.lambda[item_type_id_1],
                    -radius_1 * radius_1);
            knitro_context.add_con_quadratic_term(
                    constraint_id,
                    variables.lambda[item_type_id_2],
                    variables.lambda[item_type_id_2],
                    -radius_2 * radius_2);
            knitro_context.add_con_quadratic_term(
                    constraint_id,
                    variables.lambda[item_type_id_1],
                    variables.lambda[item_type_id_2],
                    -2 * radius_1 * radius_2);

            knitro_context.set_con_lobnd(
                    constraint_id,
                    0);
        }
    }

    // Constraints: intersections between items and bin.
    // For each items j = 1..N  x[j] >= lambda[j] * rc_j
    //                          y[j] >= lambda[j] * rc_j
    //                          x[j] <= w - lambda[j] * rc_j
    //                          y[j] <= h - lambda[j] * rc_j
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        LengthDbl radius = distance(
                item_type.shapes.front().shape.elements.front().center,
                item_type.shapes.front().shape.elements.front().start);

        knitrocpp::ConstraintId constraint_id = knitro_context.add_con();
        knitro_context.add_con_linear_term(
                constraint_id,
                variables.x[item_type_id],
                1);
        knitro_context.add_con_linear_term(
                constraint_id,
                variables.lambda[item_type_id],
                -radius);
        knitro_context.set_con_lobnd(
                constraint_id,
                0.0);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        LengthDbl radius = distance(
                item_type.shapes.front().shape.elements.front().center,
                item_type.shapes.front().shape.elements.front().start);

        knitrocpp::ConstraintId constraint_id = knitro_context.add_con();
        knitro_context.add_con_linear_term(
                constraint_id,
                variables.y[item_type_id],
                1);
        knitro_context.add_con_linear_term(
                constraint_id,
                variables.lambda[item_type_id],
                -radius);
        knitro_context.set_con_lobnd(
                constraint_id,
                0.0);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        LengthDbl radius = distance(
                item_type.shapes.front().shape.elements.front().center,
                item_type.shapes.front().shape.elements.front().start);

        knitrocpp::ConstraintId constraint_id = knitro_context.add_con();
        knitro_context.add_con_linear_term(
                constraint_id,
                variables.x[item_type_id],
                1);
        knitro_context.add_con_linear_term(
                constraint_id,
                variables.lambda[item_type_id],
                +radius);
        knitro_context.set_con_upbnd(
                constraint_id,
                width);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        LengthDbl radius = distance(
                item_type.shapes.front().shape.elements.front().center,
                item_type.shapes.front().shape.elements.front().start);

        knitrocpp::ConstraintId constraint_id = knitro_context.add_con();
        knitro_context.add_con_linear_term(
                constraint_id,
                variables.y[item_type_id],
                1);
        knitro_context.add_con_linear_term(
                constraint_id,
                variables.lambda[item_type_id],
                +radius);
        knitro_context.set_con_upbnd(
                constraint_id,
                height);
    }

    // Solve.
    int knitro_return_status = knitro_context.solve();

    // If the solution is infeasible, stop.
    if (knitro_return_status <= -200)
        return output;

    // Retrieve solution.
    Solution solution(instance);
    BinPos bin_pos = solution.add_bin(0, 1);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        LengthDbl x = knitro_context.get_var_primal_value(
                variables.x[item_type_id]);
        LengthDbl y = knitro_context.get_var_primal_value(
                variables.y[item_type_id]);
        solution.add_item(
                bin_pos,
                item_type_id,
                {x, y},
                0.0);
    }
    std::stringstream ss;
    output.solution_pool.add(solution, ss, parameters.info);

#endif

    return output;
}

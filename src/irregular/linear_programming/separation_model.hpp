/**
 * Separation model of the items of a bin, solved by linear programming.
 *
 * The items are decomposed into convex parts (convex polygons, circular
 * segments and circles), and may be shrunk: in an item with scale 'lambda',
 * each convex part is scaled by 'lambda' around its center (the average of
 * the vertices of its approximation, see 'Part'). A scaled part is in the
 * part, so a shrunk item is in the item.
 *
 * For each pair of convex parts close to each other, the direction 'normal'
 * of a separating line is selected; the constraint is that the gap between
 * the two parts along 'normal' is at least the minimum spacing. The gap is
 * the smallest value of 'normal . point' over the points of one part minus
 * the largest value over the points of the other part; these extreme points
 * are vertices, or points of circular arcs whose outward normal is 'normal'.
 * For a fixed direction, they don't depend on the translations and the
 * scales of the parts, so the constraint is linear in the positions and the
 * scales, and it proves that the two parts don't overlap. This is the
 * separating line of the nonlinear separation model (see
 * 'nonlinear_programming/separation_model.hpp') with its direction fixed
 * during a step.
 *
 * The candidate directions are the outward normals of the edges (line
 * segments) of the two parts and, if one of the parts has circular arcs, the
 * direction between the centers of the parts and the direction of the
 * separating line of the approximations of the parts which maximizes the gap
 * between them.
 *
 * A step solves the linear program maximizing the weighted sum of the scales,
 * the items staying in a small box around their current positions.
 *
 * As in the nonlinear separation model, the linear program is always
 * feasible: the current positions with null scales are a solution. With null
 * scales, the parts are reduced to their centers, which stay in the bin; and
 * the edge of each pair is selected, among the edges which separate the
 * centers of the two parts (by at least the spacing), as the one which
 * separates the two parts the best in the current state. Since the centers
 * are in the parts, the edge which separates two parts the best is always
 * selected when the parts don't overlap. The exceptions are a center inside
 * an obstacle (or outside the bin), and two centers closer than the spacing
 * in every direction of the edges: then, the edge which separates the two
 * parts the best is selected, and the linear program may be infeasible.
 */

#pragma once

#include "packingsolver/irregular/instance.hpp"

#include "optimizationtools/utils/timer.hpp"

namespace packingsolver
{
namespace irregular
{

struct InstanceBasicShapeDecomposition;

/** Item of a bin in the separation model. */
struct SeparationModelItem
{
    /** Item type. */
    ItemTypeId item_type_id = -1;

    /** Angle. */
    Angle angle = 0.0;

    /** Mirror. */
    bool mirror = false;

    /** True iff the item is fixed: it doesn't move and isn't shrunk. */
    bool is_fixed = false;

    /** Position of the reference point, in scaled coordinates. */
    Point position = {0.0, 0.0};

    /** Scale. */
    double lambda = 1.0;

    /** Weight of the scale in the objective. */
    double weight = 1.0;
};

class BinSeparationModel
{

public:

    /** Parameters of a step. */
    struct StepParameters
    {
        /**
         * Maximum move of an item, in each direction, relatively to the
         * largest side of its bounding box.
         */
        double movement_ratio = 0.05;
    };

    /**
     * Constructor.
     *
     * 'items' gives the item types, the orientations, the fixed items and
     * their positions; the positions of the other items and the scales
     * aren't used.
     */
    BinSeparationModel(
            const Instance& instance,
            const InstanceBasicShapeDecomposition& decomposition,
            BinTypeId bin_type_id,
            const std::vector<SeparationModelItem>& items);

    /**
     * Return true iff the model supports the items: the shapes of the borders
     * are made of line segments.
     */
    bool supported() const { return supported_; }

    /**
     * Get the smallest separation of the items in a state, in scaled
     * coordinates (negative: the largest penetration).
     */
    LengthDbl separation(const std::vector<SeparationModelItem>& items) const;

    /**
     * Solve the linear program of a step, and update the positions and the
     * scales of the items.
     *
     * Return false if the linear program hasn't been solved.
     */
    bool step(
            std::vector<SeparationModelItem>& items,
            const StepParameters& parameters,
            const optimizationtools::Timer& timer) const;

    /**
     * Solve the linear program of an anchor step, and update the positions
     * of the items.
     *
     * The items keep their scales. The linear program minimizes
     * 'x_weight * x + y_weight * y' summed over the items, the parts of the
     * items staying in 'container' (in scaled coordinates, the bin spacing
     * included).
     *
     * The edge of each pair of parts is the one which separates them the best
     * in the current state. The current state is kept feasible: a constraint
     * which it doesn't satisfy (for example, two parts whose corners are at
     * the spacing from each other, but whose edges are not) is relaxed to its
     * current value.
     *
     * Return false if the linear program hasn't been solved.
     */
    bool anchor_step(
            std::vector<SeparationModelItem>& items,
            double x_weight,
            double y_weight,
            const AxisAlignedBoundingBox& container,
            const StepParameters& parameters,
            const optimizationtools::Timer& timer) const;

private:

    /** Convex circular arc of a part, anticlockwise. */
    struct Arc
    {
        /** Center. */
        Point center = {0, 0};

        /** Radius. */
        LengthDbl radius = 0.0;

        /** Direction from the center to the start of the arc. */
        Point start_direction = {1, 0};

        /**
         * Angle of the arc, anticlockwise from its start; 2 * pi for a full
         * circle.
         */
        Angle angle = 0.0;
    };

    /** Convex part of an item or of an obstacle. */
    struct Part
    {
        /**
         * Position of the item in the bin, or -1 for the parts of the
         * obstacles (fixed items, borders and defects).
         */
        ItemPos item_pos = -1;

        /**
         * Vertices (starts of the elements), anticlockwise: relatively to the
         * reference point of the item, at full scale, for the parts of the
         * items; in the frame of the bin for the parts of the obstacles.
         */
        std::vector<Point> vertices;

        /** Edges (line segments): start and end, in the same frame. */
        std::vector<std::pair<Point, Point>> edges;

        /** Circular arcs, in the same frame. */
        std::vector<Arc> arcs;

        /**
         * Vertices of a convex polygon inscribed in the part, anticlockwise,
         * in the same frame: the vertices and points sampled on the arcs.
         */
        std::vector<Point> approximation;

        /** Bounding box, in the same frame as the vertices. */
        AxisAlignedBoundingBox aabb;

        /**
         * Center of the scaling of the part (average of the vertices of its
         * approximation), in the same frame as the vertices; {0, 0} for the
         * parts of the obstacles.
         */
        Point center = {0, 0};

        /**
         * Minimum spacing between this part and the parts of the items (for
         * an obstacle).
         */
        LengthDbl spacing = 0.0;
    };

    /**
     * Separation constraint of two parts:
     * normal . (p_support - p_edge)
     * + support_constant + support_coefficient * lambda_support
     * - edge_constant - edge_coefficient * lambda_edge
     * >= spacing.
     */
    struct Constraint
    {
        Counter edge_part_pos = -1;

        Counter support_part_pos = -1;

        Point normal = {0, 0};

        LengthDbl edge_constant = 0.0;

        LengthDbl edge_coefficient = 0.0;

        LengthDbl support_constant = 0.0;

        LengthDbl support_coefficient = 0.0;

        LengthDbl spacing = 0.0;
    };

    /**
     * Largest value of 'normal . point' over the points of a part, in the
     * frame of the part, at full scale.
     */
    static LengthDbl support_value(
            const Part& part,
            const Point& normal);

    /**
     * Build the constraint of two parts for a unit direction, oriented from
     * the edge part to the support part.
     *
     * 'edge_support_value' is 'support_value(edge_part, normal)'.
     */
    Constraint build_constraint(
            Counter edge_part_pos,
            Counter support_part_pos,
            const Point& normal,
            LengthDbl edge_support_value) const;

    /** Vertices of the approximation of a part in a state. */
    std::vector<Point> state_approximation(
            const std::vector<SeparationModelItem>& items,
            const Part& part) const;

    /** Bounding box of a part in a state. */
    AxisAlignedBoundingBox part_aabb(
            const std::vector<SeparationModelItem>& items,
            Counter part_pos) const;

    /**
     * Constraint of the edge selected for two parts in a state: among the
     * edges whose constraints are satisfied with null scales (see the
     * description of the model), the one which separates the two parts the
     * best; if there is none, the one which separates them the best.
     * 'best_value' is set to the largest value of the constraints of all
     * the edges, so that it measures the separation of the two parts.
     *
     * If 'fixed_scales' is true, the scales don't change during the step:
     * the selected edge is the one which separates the two parts the best.
     */
    Constraint best_constraint(
            const std::vector<SeparationModelItem>& items,
            Counter part_pos_1,
            Counter part_pos_2,
            bool fixed_scales,
            LengthDbl& best_value) const;

    /** Value of a constraint in a state (minus its spacing). */
    LengthDbl value(
            const std::vector<SeparationModelItem>& items,
            const Constraint& constraint) const;

    /**
     * Value of a constraint in a state where the scales of the items are
     * null (minus its spacing).
     */
    LengthDbl null_scales_value(
            const std::vector<SeparationModelItem>& items,
            const Constraint& constraint) const;

    /**
     * Compute the constraints of the pairs of parts whose boxes, expanded by
     * the maximum moves of their items, intersect; return the smallest
     * separation of the state.
     */
    LengthDbl compute_constraints(
            const std::vector<SeparationModelItem>& items,
            const std::vector<LengthDbl>& movements,
            bool fixed_scales,
            std::vector<Constraint>& constraints) const;

    /**
     * Solve the linear program of a step (see 'step') or, if 'fixed_scales'
     * is true, of an anchor step (see 'anchor_step').
     */
    bool solve_step(
            std::vector<SeparationModelItem>& items,
            const StepParameters& parameters,
            bool fixed_scales,
            double x_weight,
            double y_weight,
            const AxisAlignedBoundingBox& container,
            const optimizationtools::Timer& timer) const;

    /** Instance. */
    const Instance& instance_;

    /** Bin type. */
    BinTypeId bin_type_id_;

    /** Parts of the items and of the obstacles. */
    std::vector<Part> parts_;

    /** Bounding boxes of the items, at full scale, relatively to their reference points. */
    std::vector<AxisAlignedBoundingBox> items_aabbs_;

    /**
     * Bounding boxes of the centers of the parts of the items, relatively to
     * their reference points.
     */
    std::vector<AxisAlignedBoundingBox> items_centers_aabbs_;

    /** Item-item minimum spacing. */
    LengthDbl item_spacing_ = 0.0;

    /** Item-bin minimum spacing. */
    LengthDbl bin_spacing_ = 0.0;

    /** Largest minimum spacing. */
    LengthDbl maximum_spacing_ = 0.0;

    /** True iff the model supports the items. */
    bool supported_ = true;

};

}
}

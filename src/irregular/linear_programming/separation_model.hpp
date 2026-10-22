/**
 * Separation model of the items of a bin, solved by linear programming.
 *
 * The items are decomposed into convex parts, and may be shrunk: an item
 * with scale 'lambda' is its shape scaled by 'lambda' around its reference
 * point.
 *
 * For each pair of convex parts close to each other, the edge of one of them
 * which separates them the best in the current state is selected; the
 * constraint is that the signed distance from the line through this edge to
 * the vertex of the other part the closest to it is at least eta (plus the
 * minimum spacing). For a fixed edge, this vertex doesn't depend on the
 * translations and the scales of the parts, so the constraint is linear in
 * the positions and the scales, and a non-negative value proves that the two
 * parts don't overlap.
 *
 * A step solves the linear program maximizing eta first, then the weighted
 * sum of the scales (if the items may be shrunk), the items staying in a
 * small box around their current positions.
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

        /** True iff the scales of the items are variables. */
        bool shrink = false;

        /**
         * Upper bound of eta, in scaled coordinates: a small positive value,
         * so that the solution stays feasible despite the tolerances of the
         * solver.
         */
        LengthDbl target_separation = 1e-5;

        /** Weight of eta in the objective. */
        double eta_weight = 1.0;
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
     * Return true iff the model supports the items: their shapes, the shapes
     * of the borders and of the defects are made of line segments.
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

private:

    /** Convex part of an item or of an obstacle. */
    struct Part
    {
        /**
         * Position of the item in the bin, or -1 for the parts of the
         * obstacles (fixed items, borders and defects).
         */
        ItemPos item_pos = -1;

        /**
         * Vertices, anticlockwise: relatively to the reference point of the
         * item, at full scale, for the parts of the items; in the frame of
         * the bin for the parts of the obstacles.
         */
        std::vector<Point> vertices;

        /** Bounding box, in the same frame as the vertices. */
        AxisAlignedBoundingBox aabb;

        /**
         * Minimum spacing between this part and the parts of the items (for
         * an obstacle).
         */
        LengthDbl spacing = 0.0;
    };

    /**
     * Separation constraint of two parts:
     * normal . (p_support - p_edge)
     * + support_coefficient * lambda_support - edge_coefficient * lambda_edge
     * >= spacing.
     */
    struct Constraint
    {
        Counter edge_part_pos = -1;

        Counter support_part_pos = -1;

        Point normal = {0, 0};

        LengthDbl edge_coefficient = 0.0;

        LengthDbl support_coefficient = 0.0;

        LengthDbl spacing = 0.0;
    };

    /** Bounding box of a part in a state. */
    AxisAlignedBoundingBox part_aabb(
            const std::vector<SeparationModelItem>& items,
            Counter part_pos) const;

    /**
     * Constraint of the edge which separates two parts the best in a state;
     * 'best_value' is set to its value.
     */
    Constraint best_constraint(
            const std::vector<SeparationModelItem>& items,
            Counter part_pos_1,
            Counter part_pos_2,
            LengthDbl& best_value) const;

    /** Value of a constraint in a state (minus its spacing). */
    LengthDbl value(
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
            std::vector<Constraint>& constraints) const;

    /** Instance. */
    const Instance& instance_;

    /** Bin type. */
    BinTypeId bin_type_id_;

    /** Parts of the items and of the obstacles. */
    std::vector<Part> parts_;

    /** Bounding boxes of the items, at full scale, relatively to their reference points. */
    std::vector<AxisAlignedBoundingBox> items_aabbs_;

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

/**
 * Separation model of the items of a bin, solved by nonlinear programming.
 *
 * The items are decomposed into convex parts, and may be shrunk (scale
 * 'lambda' around their reference point) and rotated (angle 'theta' around
 * their reference point, relatively to their current angle).
 *
 * For each pair of convex parts close to each other, a separating line, of
 * normal (cos alpha, sin alpha) and offset c, is a variable of the model: the
 * vertices of the first part must be on one side of the line, and the
 * vertices of the second part on the other side (quasi-phi-functions, see
 * Stoyan, Pankratov and Romanova, 2016). Two convex parts don't overlap iff
 * such a line exists, so no separating edge needs to be selected. The lines
 * are initialized with the separating lines maximizing the gaps (see
 * 'compute_separating_line').
 *
 * A step solves the nonlinear program maximizing the weighted sum of the
 * scales, the items staying in a small box around their current positions
 * and in their rotation ranges. The reference points of the items stay in
 * the bin, so that the scales can always reach 0: the program is always
 * feasible, except when a reference point is inside an obstacle.
 *
 * The program is solved with Knitro.
 */

#pragma once

#include "irregular/linear_programming/separation_model.hpp"

namespace packingsolver
{
namespace irregular
{

class BinNonlinearSeparationModel
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
    BinNonlinearSeparationModel(
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
     * Solve the nonlinear program of a step, and update the positions, the
     * angles and the scales of the items. The rotation range of each item is
     * given by its fields 'minimum_rotation' and 'maximum_rotation'.
     *
     * Return false if the nonlinear program hasn't been solved.
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
         * item, at full scale and at the current angle, for the parts of the
         * items; in the frame of the bin for the parts of the obstacles.
         */
        std::vector<Point> vertices;

        /**
         * Minimum spacing between this part and the parts of the items (for
         * an obstacle).
         */
        LengthDbl spacing = 0.0;
    };

    /** Vertices of a part in a state, in the frame of the bin. */
    std::vector<Point> world_vertices(
            const std::vector<SeparationModelItem>& items,
            Counter part_pos) const;

    /** Instance. */
    const Instance& instance_;

    /** Bin type. */
    BinTypeId bin_type_id_;

    /** Parts of the items and of the obstacles. */
    std::vector<Part> parts_;

    /**
     * Vertices of the convex hulls of the items, relatively to their
     * reference points, at full scale and at their current angles.
     */
    std::vector<std::vector<Point>> items_hulls_;

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

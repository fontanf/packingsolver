/**
 * Separator of the local search with the nonlinear programming engine
 *
 * As for the linear programming engine (see
 * 'linear_programming/separator.hpp'), the items may be shrunk instead of
 * overlapping, and a separation maximizes their weighted scales, the weights
 * of the items which stay shrunk increasing as in guided local search. The
 * steps solve nonlinear programs (see 'BinNonlinearSeparationModel'), in which
 * the items may also rotate within their allowed rotation ranges.
 *
 * Requires Knitro.
 */

#pragma once

#include "irregular/nonlinear_programming/separation_model.hpp"

#include "packingsolver/irregular/solution.hpp"

#include <memory>
#include <random>

namespace packingsolver
{
namespace irregular
{

struct InstanceBasicShapeDecomposition;

struct NonlinearProgrammingSeparatorParameters
{
    /**
     * Maximum move of an item, in each direction, in each nonlinear program,
     * relatively to the largest side of its bounding box.
     */
    double movement_ratio = 0.05;

    /**
     * Smallest movement ratio: after a nonlinear program which doesn't
     * improve the loss, the movement ratio is halved, down to this value.
     */
    double minimum_movement_ratio = 1e-4;

    /**
     * Maximum rotation of an item, in each direction, in each nonlinear
     * program, in degrees, for the item types with continuous rotations; it
     * decreases with the movement ratio.
     */
    Angle maximum_rotation = 5.0;

    /**
     * Maximum number of nonlinear programs without improvement of the loss of
     * a separation.
     */
    Counter maximum_number_of_iterations_without_improvement = 4;

    /**
     * Same as 'maximum_number_of_iterations_without_improvement', while the
     * loss isn't smaller than the reference loss (see 'separate').
     */
    Counter maximum_number_of_iterations_without_improvement_above_reference = 2;

    /**
     * Same as 'maximum_number_of_iterations_without_improvement', for the
     * separation of the initial layout.
     */
    Counter initial_maximum_number_of_iterations_without_improvement = 24;

    /**
     * Penetration below which two shapes are considered touching, in scaled
     * coordinates.
     */
    LengthDbl tolerance = 1e-7;
};

class NonlinearProgrammingSeparator
{

public:

    /** State of an item. */
    struct ItemState
    {
        /** Bin, or -1 if the item isn't packed. */
        BinPos bin_pos = -1;

        /** Angle. */
        Angle angle = 0.0;

        /** Mirror. */
        bool mirror = false;

        /** Position of the reference point, in scaled coordinates. */
        Point position = {0.0, 0.0};

        /** Scale. */
        double lambda = 1.0;
    };

    /** Layout: the states of the items. */
    using Layout = std::vector<ItemState>;

    /** Constructor. */
    NonlinearProgrammingSeparator(
            const Instance& instance,
            const NonlinearProgrammingSeparatorParameters& parameters,
            std::mt19937_64& generator);

    /**
     * Load a solution of the instance.
     *
     * The items missing from the solution are inserted at random positions,
     * with a null scale.
     */
    void set_solution(
            const Solution& solution,
            const optimizationtools::Timer& timer);

    /**
     * Try to make all the items reach their full size without overlap.
     *
     * Return true iff successful. In any case, the current layout is the one
     * with the smallest loss found.
     *
     * While the smallest loss found isn't smaller than 'reference_loss', the
     * separation stops after
     * 'maximum_number_of_iterations_without_improvement_above_reference'
     * nonlinear programs without improvement instead of
     * 'maximum_number_of_iterations_without_improvement'.
     */
    bool separate(
            const optimizationtools::Timer& timer,
            double reference_loss = std::numeric_limits<double>::infinity());

    /** Same as 'separate', with more effort, for the initial layout. */
    bool separate_initial(const optimizationtools::Timer& timer);

    /** Get the current layout. */
    const Layout& layout() const { return items_state_; }

    /** Set the current layout; the weights are kept. */
    void set_layout(const Layout& layout) { items_state_ = layout; }

    /**
     * Disrupt the current layout: take the most shrunk item out, and insert
     * it again in a random bin, at a random position, with a random allowed
     * orientation and a null scale.
     */
    void disrupt();

    /** Distance between two layouts, in [0, 1]. */
    double distance(
            const Layout& layout_1,
            const Layout& layout_2) const;

    /**
     * Get the loss of the current layout: the area missing from the shrunk
     * items, plus the largest penetration for an infeasible layout.
     */
    double total_loss() const;

    /** Get the solution of the current layout. */
    Solution solution() const;

private:

    /** Run the nonlinear programs of a separation. */
    bool separate(
            const optimizationtools::Timer& timer,
            double reference_loss,
            Counter maximum_number_of_iterations_without_improvement);

    /** Run a nonlinear program on each bin; return false if one fails. */
    bool step(
            const optimizationtools::Timer& timer,
            double movement_ratio);

    /**
     * Compute the smallest separation of the current layout, over all the
     * bins.
     */
    LengthDbl separation() const;

    /** Items of the separation model of a bin, from the current layout. */
    std::vector<NonlinearSeparationModelItem> bin_items(
            BinPos bin_pos,
            std::vector<ItemPos>& item_positions) const;

    /**
     * Range of the rotation of an item allowed in a step, in degrees,
     * relatively to its angle: the intersection of the allowed rotation range
     * containing its angle and of [-maximum_rotation, maximum_rotation].
     */
    void rotation_range(
            ItemTypeId item_type_id,
            Angle angle,
            bool mirror,
            Angle maximum_rotation,
            Angle& minimum_rotation_output,
            Angle& maximum_rotation_output) const;

    /** Angle in [start, start + 360[ if it is in a full rotation range. */
    Angle normalize_angle(
            ItemTypeId item_type_id,
            Angle angle,
            bool mirror) const;

    /**
     * Random placement of an item: random bin, allowed orientation and
     * position, null scale.
     */
    void random_placement(
            ItemPos item_pos,
            ItemState& item_state);

    /** Random allowed orientation of an item type. */
    void random_orientation(
            ItemTypeId item_type_id,
            Angle& angle,
            bool& mirror);

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    NonlinearProgrammingSeparatorParameters parameters_;

    /** Random number generator. */
    std::mt19937_64 generator_;

    /** Convex decomposition of the instance. */
    std::shared_ptr<const InstanceBasicShapeDecomposition> decomposition_;

    /** Item type of each item. */
    std::vector<ItemTypeId> items_item_type_id_;

    /** Area of each item (scaled). */
    std::vector<AreaDbl> items_area_;

    /** Current layout. */
    Layout items_state_;

    /** Weights of the scales of the items. */
    std::vector<double> items_weight_;

};

}
}

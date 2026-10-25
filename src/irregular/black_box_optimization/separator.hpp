/**
 * Separator of the local search with the black-box optimization engine
 *
 * Overlap minimization following the separation algorithm of:
 * "An iterated local search algorithm based on nonlinear programming for the
 * irregular strip packing problem" (Imamichi et al., 2009), also used by
 * "A General Heuristic Approach for Maximum Polygon Packing" (Luo et al.,
 * 2024). "Solving the irregular strip packing problem via guided local search
 * for overlap minimization" (Umetani et al., 2009) uses directional
 * penetration depths instead.
 *
 * The items may overlap each other and the obstacles of their bin (borders,
 * defects and fixed items). The overlap of two items is measured by their
 * penetration depth, computed exactly from their no-fit polygon (see
 * 'no_fit_polygon.hpp'). A step minimizes, for each bin with overlap, the
 * weighted sum of the squared penetration depths over the positions of its
 * items, as a black-box (unconstrained, with bounds) nonlinear program solved
 * by Knitro with a quasi-Newton method; the orientations of the items are
 * fixed. The positions are bounded so that the items are in the bounding box
 * of their bin.
 *
 * The guided local search weights, the disruption, the loading of a solution
 * and the distance between layouts are the ones of the direct trigonometry
 * engine (see 'direct_trigonometry/separator.hpp'), whose separator is used
 * for them.
 *
 * Requires Knitro.
 */

#pragma once

#include "irregular/black_box_optimization/no_fit_polygon.hpp"
#include "irregular/direct_trigonometry/separator.hpp"

#include <memory>
#include <random>
#include <unordered_map>

namespace packingsolver
{
namespace irregular
{

/**
 * No-fit polygons of the pairs of item types in their orientations of the
 * direct trigonometry engine (see 'Separator::build_item_types'), computed
 * lazily.
 *
 * They depend only on the item types, so they can be shared by the
 * separators of instances with the same item types (the sub-instances of the
 * sequential feasibility algorithm). Not thread-safe.
 */
class ItemNoFitPolygons
{

public:

    /** Constructor. */
    ItemNoFitPolygons(
            const Instance& instance,
            const std::shared_ptr<const Separator::ItemTypes>& item_types,
            double maximum_approximation_ratio);

    /**
     * Get the no-fit polygon of two items in given orientations.
     *
     * It is the no-fit polygon of one of them inflated by the item spacing
     * (the fixed item) and of the other one (the orbiting item). If
     * 'reversed' is false, the fixed item is the first one; otherwise, it is
     * the second one.
     */
    const NoFitPolygon& no_fit_polygon(
            ItemTypeId item_type_id_1,
            Counter orientation_id_1,
            ItemTypeId item_type_id_2,
            Counter orientation_id_2,
            bool& reversed);

    /** Get the shapes of an item type in an orientation. */
    const std::vector<ShapeWithHoles>& shapes(
            ItemTypeId item_type_id,
            Counter orientation_id) const
    {
        return orientations_shapes_[orientation_index(item_type_id, orientation_id)];
    }

    /**
     * Get the shapes of an item type in an orientation, inflated by the item
     * spacing.
     */
    const std::vector<ShapeWithHoles>& shapes_inflated(
            ItemTypeId item_type_id,
            Counter orientation_id) const
    {
        return orientations_shapes_inflated_[orientation_index(item_type_id, orientation_id)];
    }

    /** Get the number of no-fit polygons computed. */
    Counter number_of_no_fit_polygons() const { return no_fit_polygons_.size(); }

private:

    /** Index of an item type in an orientation. */
    Counter orientation_index(
            ItemTypeId item_type_id,
            Counter orientation_id) const
    {
        return item_types_first_orientation_index_[item_type_id] + orientation_id;
    }

    /** For each item type, the index of its first orientation. */
    std::vector<Counter> item_types_first_orientation_index_;

    /** Shapes of each item type in each orientation. */
    std::vector<std::vector<ShapeWithHoles>> orientations_shapes_;

    /** Inflated shapes of each item type in each orientation. */
    std::vector<std::vector<ShapeWithHoles>> orientations_shapes_inflated_;

    /** No-fit polygons computed, by pair of orientation indices. */
    std::unordered_map<int64_t, NoFitPolygon> no_fit_polygons_;

};

struct BlackBoxOptimizationSeparatorParameters
{
    /**
     * Parameters of the direct trigonometry separator, used to load the
     * solutions and to disrupt the layouts.
     */
    SeparatorParameters direct_trigonometry_parameters;

    /**
     * Maximum number of steps without improvement of a separation attempt.
     */
    Counter maximum_number_of_iterations_without_improvement = 20;

    /**
     * Maximum number of consecutive separation attempts without substantial
     * improvement.
     */
    Counter maximum_number_of_strikes = 3;

    /**
     * Maximum number of consecutive separation attempts without substantial
     * improvement while the loss isn't smaller than the reference loss (see
     * 'separate').
     */
    Counter maximum_number_of_strikes_above_reference = 1;

    /**
     * Margin, relatively to the mean smallest dimension of the items: the
     * nonlinear programs minimize the squared penetration depths increased by
     * the margin, so that their solutions are separated despite the
     * tolerances of the solver.
     */
    double margin_ratio = 1e-4;

    /** Maximum number of iterations of the solver for a nonlinear program. */
    Counter maximum_number_of_solver_iterations = 1000;

    /** Weighting scheme of the guided local search weights. */
    LocalSearchWeightingScheme weighting_scheme = LocalSearchWeightingScheme::Multiplicative;
};

class BlackBoxOptimizationSeparator
{

public:

    /** State of an item. */
    using ItemState = Separator::ItemState;

    /** Layout: the states of the items. */
    using Layout = Separator::Layout;

    /**
     * Constructor.
     *
     * If 'item_types' is 'nullptr', the item types of the direct trigonometry
     * separator are built from the instance. If 'no_fit_polygons' is
     * 'nullptr', they are built for the item types.
     */
    BlackBoxOptimizationSeparator(
            const Instance& instance,
            const BlackBoxOptimizationSeparatorParameters& parameters,
            std::mt19937_64& generator,
            const std::shared_ptr<const Separator::ItemTypes>& item_types = nullptr,
            const std::shared_ptr<ItemNoFitPolygons>& no_fit_polygons = nullptr);

    /**
     * Load a solution of the instance, as the direct trigonometry separator
     * (see 'Separator::set_solution').
     */
    void set_solution(
            const Solution& solution,
            const optimizationtools::Timer& timer);

    /**
     * Try to remove all overlaps.
     *
     * Return true iff successful. In any case, the current layout is the one
     * with the smallest loss found.
     *
     * While the smallest loss found isn't smaller than 'reference_loss', the
     * separation stops after
     * 'maximum_number_of_strikes_above_reference' strikes instead of
     * 'maximum_number_of_strikes'.
     */
    bool separate(
            const optimizationtools::Timer& timer,
            double reference_loss = std::numeric_limits<double>::infinity());

    /**
     * Set the stopping criteria of the separation of the initial layout
     * ('separate_initial'); -1 means the one of the parameters.
     */
    void set_initial_stopping_criteria(
            Counter maximum_number_of_iterations_without_improvement,
            Counter maximum_number_of_strikes)
    {
        initial_maximum_number_of_iterations_without_improvement_
            = maximum_number_of_iterations_without_improvement;
        initial_maximum_number_of_strikes_ = maximum_number_of_strikes;
    }

    /**
     * Same as 'separate', with the stopping criteria of the separation of the
     * initial layout.
     */
    bool separate_initial(const optimizationtools::Timer& timer);

    /** Get the current layout. */
    const Layout& layout() const { return items_state_; }

    /**
     * Set the current layout; the weights are reset, as for the direct
     * trigonometry separator.
     */
    void set_layout(const Layout& layout);

    /**
     * Disrupt the current layout, as the direct trigonometry separator (see
     * 'Separator::disrupt').
     */
    void disrupt();

    /**
     * Distance between two layouts, as for the direct trigonometry separator
     * (see 'Separator::distance').
     */
    Counter distance(
            const Layout& layout_1,
            const Layout& layout_2) const;

    /**
     * Get the loss of the current layout: the sum of the squared penetration
     * depths, and of the squared distances of the items sticking out of the
     * bounding box of their bin.
     */
    double total_loss() const;

    /** Get the number of nonlinear programs solved. */
    Counter number_of_nonlinear_programs() const { return number_of_nonlinear_programs_; }

    /** Get the solution of the current layout. */
    Solution solution() const;

private:

    /** Obstacle of a bin: a border, a defect or a fixed item. */
    struct Obstacle
    {
        /**
         * Shapes: inflated by the spacing for a border or a defect; not
         * inflated for a fixed item.
         */
        std::vector<ShapeWithHoles> shapes;

        /**
         * True iff the shapes must be compared with the inflated shapes of the
         * items (fixed items).
         */
        bool against_inflated = false;

        /** Bounding box. */
        AxisAlignedBoundingBox aabb;

        /** No-fit polygons, by orientation index of the items, computed lazily. */
        std::unordered_map<int64_t, NoFitPolygon> no_fit_polygons;
    };

    struct BinTypeData
    {
        /** Box in which the bounding boxes of the items must be. */
        AxisAlignedBoundingBox container;

        /** Obstacles. */
        std::vector<Obstacle> obstacles;
    };

    /** Penetration loss and guided local search weight. */
    struct Entry
    {
        double loss = 0.0;

        double weight = 1.0;
    };

    /** Run the separation, with given stopping criteria. */
    bool separate(
            const optimizationtools::Timer& timer,
            double reference_loss,
            Counter maximum_number_of_iterations_without_improvement,
            Counter maximum_number_of_strikes,
            Counter maximum_number_of_strikes_above_reference);

    /** Get the entry of a pair of items. */
    Entry& pair_entry(ItemPos item_pos_1, ItemPos item_pos_2);
    const Entry& pair_entry(ItemPos item_pos_1, ItemPos item_pos_2) const;

    /** Get the entry of an item and an obstacle ('0' is the exterior of the bin). */
    Entry& hazard_entry(ItemPos item_pos, Counter hazard_pos) { return hazard_entries_[item_pos * number_of_hazards_ + hazard_pos]; }
    const Entry& hazard_entry(ItemPos item_pos, Counter hazard_pos) const { return hazard_entries_[item_pos * number_of_hazards_ + hazard_pos]; }

    /**
     * Compute the signed distance of the penetration of two items; return
     * false if they are farther from each other than the margin.
     *
     * 'gradient' is the gradient with respect to the position of the first
     * item.
     */
    bool pair_signed_distance(
            ItemPos item_pos_1,
            const Point& position_1,
            ItemPos item_pos_2,
            const Point& position_2,
            SignedDistance& output);

    /**
     * Compute the signed distance of the penetration of an item into an
     * obstacle; return false if they are farther from each other than the
     * margin.
     */
    bool obstacle_signed_distance(
            ItemPos item_pos,
            const Point& position,
            Obstacle& obstacle,
            SignedDistance& output);

    /**
     * Bounds of the position of an item: the positions for which its
     * bounding box is in the container of its bin. If the item is larger than
     * the container in a dimension, its bounds are the middle position.
     */
    AxisAlignedBoundingBox position_bounds(
            ItemPos item_pos,
            const ItemState& item_state) const;

    /** Recompute the losses of the current layout. */
    void compute_losses();

    /** Update the guided local search weights. */
    void update_weights();

    /** Solve the nonlinear program of each bin with overlap. */
    void step(const optimizationtools::Timer& timer);

    /** Solve the nonlinear program of a bin. */
    void solve_bin(
            BinPos bin_pos,
            const std::vector<ItemPos>& bin_items,
            const optimizationtools::Timer& timer);

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    BlackBoxOptimizationSeparatorParameters parameters_;

    /** Stopping criteria of the separation of the initial layout. */
    Counter initial_maximum_number_of_iterations_without_improvement_ = -1;
    Counter initial_maximum_number_of_strikes_ = -1;

    /**
     * Direct trigonometry separator, to load the solutions, disrupt the
     * layouts and build the solutions (its layout is set before each use).
     */
    mutable Separator direct_trigonometry_separator_;

    /** Item types of the direct trigonometry separator. */
    std::shared_ptr<const Separator::ItemTypes> item_types_;

    /** No-fit polygons of the pairs of items. */
    std::shared_ptr<ItemNoFitPolygons> no_fit_polygons_;

    /** Bin types. */
    std::vector<BinTypeData> bin_types_;

    /** Item type of each item. */
    std::vector<ItemTypeId> items_item_type_id_;

    /** Length used to scale the variables of the nonlinear programs. */
    LengthDbl length_scale_ = 1.0;

    /** Margin (see 'BlackBoxOptimizationSeparatorParameters::margin_ratio'). */
    LengthDbl margin_ = 0.0;

    /** Number of hazards per item, including the exterior. */
    Counter number_of_hazards_ = 1;

    /** Current layout. */
    Layout items_state_;

    /** Entries of the pairs of items (triangular matrix). */
    std::vector<Entry> pair_entries_;

    /** Entries of the items and the hazards. */
    std::vector<Entry> hazard_entries_;

    /** Number of nonlinear programs solved. */
    Counter number_of_nonlinear_programs_ = 0;

};

}
}

/**
 * Separator
 *
 * Overlap minimization engine of the local search, following the separator of
 * Sparrow:
 * "An open-source heuristic to reboot 2D nesting research" (Gardeyn and
 * Vanden Berghe, 2025), https://doi.org/10.48550/arXiv.2509.13329
 *
 * The items may overlap each other, the defects and the borders of their bin,
 * and stick out of their bin. Each collision is quantified by a proxy of the
 * overlap area computed from the poles of the shapes. The separator moves the
 * colliding items one by one to minimize the total collision loss, weighted by
 * guided local search weights.
 *
 * Simplifications with respect to Sparrow:
 * - The rotations are discrete; a rotation is changed by sampling, never by
 *   coordinate descent.
 * - There is no spatial index: an evaluation goes through the items of the
 *   bin, filtered by their bounding boxes.
 */

#pragma once

#include "irregular/direct_trigonometry/polygon.hpp"

#include "packingsolver/irregular/optimize.hpp"
#include "packingsolver/irregular/solution.hpp"

#include "optimizationtools/utils/timer.hpp"

#include <limits>
#include <memory>
#include <random>

namespace packingsolver
{
namespace irregular
{

struct SeparatorParameters
{
    /** Maximum approximation ratio of the simplified shapes. */
    double maximum_approximation_ratio = 0.005;

    /**
     * Maximum number of iterations without improvement of a separation
     * attempt.
     */
    Counter maximum_number_of_iterations_without_improvement = 200;

    /**
     * Maximum number of consecutive separation attempts without substantial
     * improvement.
     */
    Counter maximum_number_of_strikes = 3;

    /**
     * Maximum number of consecutive separation attempts without substantial
     * improvement while the loss isn't smaller than the reference loss (see
     * 'Separator::separate').
     */
    Counter maximum_number_of_strikes_above_reference = 1;

    /** Number of samples in the whole bins, for a move. */
    Counter number_of_container_samples = 50;

    /** Number of samples around the current position, for a move. */
    Counter number_of_focused_samples = 25;

    /** Number of best samples refined by coordinate descent, for a move. */
    Counter number_of_coordinate_descents = 3;

    /**
     * Number of workers.
     *
     * At each iteration of a separation, each worker moves the colliding
     * items from the current layout, in its own random order, in its own
     * thread; the layout of the worker with the smallest weighted loss is
     * kept.
     */
    Counter number_of_workers = 3;

    /** Number of samples in the whole bins, to insert an item. */
    Counter number_of_insertion_samples = 1000;

    /** Weighting scheme of the guided local search weights. */
    LocalSearchWeightingScheme weighting_scheme = LocalSearchWeightingScheme::Multiplicative;
};

class Separator
{

public:

    /** Item type in a given orientation. */
    struct Orientation
    {
        /** Angle. */
        Angle angle = 0.0;

        /** Mirror. */
        bool mirror = false;

        /** Polygons of the shapes of the item. */
        std::vector<Polygon> polygons;

        /** Polygons of the shapes of the item inflated by the item spacing. */
        std::vector<Polygon> polygons_inflated;

        /** Poles of the shapes of the item. */
        std::vector<Pole> poles;

        /** Bounding box of the polygons. */
        AxisAlignedBoundingBox aabb;

        /** Bounding box of the inflated polygons. */
        AxisAlignedBoundingBox aabb_inflated;
    };

    struct SeparatorItemType
    {
        /** Orientations. */
        std::vector<Orientation> orientations;

        /**
         * For each bin type of the instance the item types are built from,
         * the allowed orientations.
         */
        std::vector<std::vector<Counter>> bin_types_orientation_ids;

        /** Area of the convex hull of the item. */
        AreaDbl convex_hull_area = 0.0;

        /** Diameter of the item. */
        LengthDbl diameter = 0.0;

        /** Smallest dimension of the bounding box of the item. */
        LengthDbl minimum_dimension = 0.0;

        /**
         * Number of poles checked first to detect a collision quickly: the
         * largest ones, covering half of the area of the item.
         */
        Counter number_of_fail_fast_poles = 0;
    };

    /** Item types of the separator. */
    using ItemTypes = std::vector<SeparatorItemType>;

    /**
     * Build the item types of the separator: the polygons and the poles of
     * the items in their allowed orientations.
     *
     * They depend only on the item types of the instance, so they can be
     * built once and shared by the separators of instances with the same
     * item types and the same bin type ids (the sub-instances of the
     * sequential feasibility algorithm). The allowed orientations of the
     * items with continuous rotations, which depend slightly on the
     * dimensions of the bins, are then the ones of the instance they are built
     * from.
     */
    static std::shared_ptr<const ItemTypes> build_item_types(
            const Instance& instance,
            const SeparatorParameters& parameters);

    /**
     * Constructor.
     *
     * The random number generator of the separator is seeded from
     * 'generator'.
     *
     * If 'item_types' is 'nullptr', the item types are built from the
     * instance.
     */
    Separator(
            const Instance& instance,
            const SeparatorParameters& parameters,
            std::mt19937_64& generator,
            const std::shared_ptr<const ItemTypes>& item_types = nullptr);

    /**
     * Load a solution of the instance.
     *
     * The solution might be infeasible and partial. The items which stick out
     * of their bin are first moved back by removing a slice of the bin at a
     * random position: the items after the slice are shifted. The missing
     * items are then inserted one by one, each at the best of sampled
     * positions, until the timer ends.
     */
    void set_solution(
            const Solution& solution,
            const optimizationtools::Timer& timer);

    /**
     * Try to remove all collisions.
     *
     * Return true iff successful. In any case, the current layout is the one
     * with the smallest loss found.
     *
     * While the smallest loss found isn't smaller than 'reference_loss', the
     * separation stops after
     * 'SeparatorParameters::maximum_number_of_strikes_above_reference'
     * strikes instead of 'SeparatorParameters::maximum_number_of_strikes'.
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

    /**
     * Set the stopping criteria of the next separations
     * ('SeparatorParameters::maximum_number_of_iterations_without_improvement'
     * and 'SeparatorParameters::maximum_number_of_strikes').
     */
    void set_stopping_criteria(
            Counter maximum_number_of_iterations_without_improvement,
            Counter maximum_number_of_strikes)
    {
        parameters_.maximum_number_of_iterations_without_improvement
            = maximum_number_of_iterations_without_improvement;
        parameters_.maximum_number_of_strikes = maximum_number_of_strikes;
    }

    /** Get the total (unweighted) collision loss of the current layout. */
    double total_loss() const;

    /** Get the number of evaluations of candidate placements. */
    Counter number_of_evaluations() const { return number_of_evaluations_; }

    /** Get the number of item moves. */
    Counter number_of_moves() const { return number_of_moves_; }

    /** Get the number of iterations of the separations. */
    Counter number_of_iterations() const { return number_of_iterations_; }

    /** Build the solution corresponding to the current layout. */
    Solution solution() const;

    /**
     * Disrupt the current layout by swapping two large items; the items
     * covered by one of them after the swap are moved to the space it left.
     */
    void disrupt();

    /**
     * Limit on the number of item moves, shared by the separators of a local
     * search (e.g. the ones of the sub-problems of the sequential
     * feasibility algorithm).
     */
    struct MovesLimit
    {
        /**
         * Maximum number of moves per worker: with several workers, the moves
         * of all the workers are counted, and the limit is multiplied by the
         * number of workers.
         */
        Counter maximum_number_of_moves = -1;

        /** Number of moves so far. */
        Counter number_of_moves = 0;

        /**
         * True iff the limit is reached. It is added as an end boolean to the
         * timer of the local search, so that the separations, the local
         * search and the sequential feasibility algorithm stop as at the end
         * of the time limit.
         */
        bool reached = false;
    };

    /** Set the limit on the number of moves of the separations. */
    void set_moves_limit(const std::shared_ptr<MovesLimit>& moves_limit)
    {
        moves_limit_ = moves_limit;
        number_of_moves_counted_ = number_of_moves_;
    }

    /** Layout. */
    struct ItemState
    {
        /** Bin. */
        BinPos bin_pos = -1;

        /** Orientation. */
        Counter orientation_id = -1;

        /** Position of the reference point of the item, in scaled coordinates. */
        Point position = {0.0, 0.0};
    };

    /** Layout: the states of the items. */
    using Layout = std::vector<ItemState>;

    /** Get the layout. */
    std::vector<ItemState> layout() const { return items_state_; }

    /** Set the layout; the collision tracker is rebuilt. */
    void set_layout(const std::vector<ItemState>& layout);

    /**
     * Distance between two layouts.
     *
     * Each item is described by its bin, its orientation, and the cell of a
     * grid containing the center of its bounding box; the size of the cells
     * of the grid of an item type is the smallest dimension of the item. The
     * copies of an item type being interchangeable, the distance is the
     * number of items whose description doesn't match the description of an
     * item of the other layout.
     */
    Counter distance(
            const std::vector<ItemState>& layout_1,
            const std::vector<ItemState>& layout_2) const;

private:

    /*
     * Static data
     */

    /** Fixed hazard of a bin: a border, a defect or a fixed item. */
    struct Hazard
    {
        /**
         * Polygons: inflated by the spacing for a border or a defect; not
         * inflated for a fixed item.
         */
        std::vector<Polygon> polygons;

        /**
         * True iff the polygons must be compared with the inflated polygons of
         * the items (fixed items).
         */
        bool against_inflated = false;

        /** Poles. */
        std::vector<Pole> poles;

        /**
         * Number of poles checked first to detect a collision quickly (see
         * 'SeparatorItemType::number_of_fail_fast_poles').
         */
        Counter number_of_fail_fast_poles = 0;

        /** Bounding box. */
        AxisAlignedBoundingBox aabb;

        /** Area of the convex hull. */
        AreaDbl convex_hull_area = 0.0;

        /** Diameter. */
        LengthDbl diameter = 0.0;
    };

    struct SeparatorBinType
    {
        /** Box in which the bounding boxes of the items must be. */
        AxisAlignedBoundingBox container;

        /** Fixed hazards. */
        std::vector<Hazard> hazards;

        /** Fixed items. */
        std::vector<SolutionItem> fixed_items;
    };

    /*
     * Dynamic data
     */

    /** Collision loss and guided local search weight. */
    struct Entry
    {
        double loss = 0.0;

        double weight = 1.0;
    };

    /** Evaluation of a candidate placement. */
    struct Evaluation
    {
        enum class Status
        {
            /** No collision; 'loss' is a placement score. */
            Clear,
            /** Collisions; 'loss' is the weighted collision loss. */
            Collision,
            /** Rejected: worse than the bound. */
            Invalid,
        };

        Status status = Status::Invalid;

        double loss = 0.0;

        bool operator<(const Evaluation& evaluation) const;
    };

    /** Candidate placement. */
    struct Candidate
    {
        ItemState state;

        Evaluation evaluation;
    };

    /*
     * Private methods
     */

    /** Get the entry of a pair of items. */
    inline Entry& pair_entry(ItemPos item_pos_1, ItemPos item_pos_2);
    inline const Entry& pair_entry(ItemPos item_pos_1, ItemPos item_pos_2) const;

    /** Get the entry of an item and a hazard ('0' is the exterior of the bin). */
    inline Entry& hazard_entry(ItemPos item_pos, Counter hazard_pos) { return hazard_entries_[item_pos * number_of_hazards_ + hazard_pos]; }
    inline const Entry& hazard_entry(ItemPos item_pos, Counter hazard_pos) const { return hazard_entries_[item_pos * number_of_hazards_ + hazard_pos]; }

    /**
     * Collision loss between two items; infinity as soon as it is known to
     * exceed 'maximum_loss'.
     */
    double collision_loss(
            ItemPos item_pos_1,
            const ItemState& item_state_1,
            ItemPos item_pos_2,
            double maximum_loss = std::numeric_limits<double>::infinity()) const;

    /**
     * Collision loss between an item and a hazard; infinity as soon as it is
     * known to exceed 'maximum_loss'.
     */
    double collision_loss(
            ItemPos item_pos,
            const ItemState& item_state,
            const Hazard& hazard,
            double maximum_loss = std::numeric_limits<double>::infinity()) const;

    /** Collision loss between an item and the exterior of its bin. */
    double exterior_loss(
            ItemPos item_pos,
            const ItemState& item_state) const;

    /** Return true iff the item collides with another item. */
    bool collide(
            ItemPos item_pos_1,
            const ItemState& item_state_1,
            ItemPos item_pos_2) const;

    /** Return true iff the item collides with a hazard. */
    bool collide(
            ItemPos item_pos,
            const ItemState& item_state,
            const Hazard& hazard) const;

    /** Return true iff the item sticks out of its bin. */
    bool collide_exterior(
            ItemPos item_pos,
            const ItemState& item_state) const;

    /**
     * Evaluate a candidate placement of an item.
     *
     * If 'insertion' is true, the item is not in the layout and the score of
     * a clear placement favors the bottom-left of the first bins.
     */
    Evaluation evaluate(
            ItemPos item_pos,
            const ItemState& item_state,
            const Evaluation& bound,
            bool insertion);

    /** Search a placement for an item. */
    Candidate search_placement(
            ItemPos item_pos,
            bool insertion);

    /** Refine a candidate placement by coordinate descent. */
    Candidate refine(
            ItemPos item_pos,
            const Candidate& candidate,
            LengthDbl step_initial,
            LengthDbl step_limit,
            bool insertion);

    /** Sample a candidate in a bin. */
    bool sample(
            ItemPos item_pos,
            BinPos bin_pos,
            const AxisAlignedBoundingBox* focus,
            ItemState& item_state);

    /** World bounding box of an item. */
    AxisAlignedBoundingBox item_aabb(
            ItemPos item_pos,
            const ItemState& item_state) const;

    /** Remove an item from the layout. */
    void remove_item(ItemPos item_pos);

    /** Add an item to the layout (it must not be in it). */
    void add_item(
            ItemPos item_pos,
            const ItemState& item_state);

    /** Recompute the collision losses of an item. */
    void update_item_losses(ItemPos item_pos);

    /** Rebuild the collision tracker; the weights are reset. */
    void rebuild_tracker();

    /** Total weighted loss of an item. */
    double item_loss(ItemPos item_pos) const;

    /** Update the guided local search weights. */
    void update_weights();

    /** Move each colliding item, in a random order. */
    void move_items(const optimizationtools::Timer& timer);

    /** Move the colliding items with each worker, and keep the best layout. */
    void move_items_workers(const optimizationtools::Timer& timer);

    /** Copy the layout and the collision tracker of another separator. */
    void copy_state(const Separator& separator);

    /** Total weighted loss. */
    double total_weighted_loss() const;

    /** Repair the items sticking out of their bins. */
    void shift_overflowing_items();

    /** Insert the items which are not in the layout, until the timer ends. */
    void insert_missing_items(const optimizationtools::Timer& timer);

    /*
     * Private attributes
     */

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    SeparatorParameters parameters_;

    /** Stopping criteria of the separation of the initial layout. */
    Counter initial_maximum_number_of_iterations_without_improvement_ = -1;
    Counter initial_maximum_number_of_strikes_ = -1;

    /** Random number generator. */
    std::mt19937_64 generator_;

    /** Item types (possibly shared with other separators). */
    std::shared_ptr<const ItemTypes> item_types_pointer_;

    /** Item types. */
    const ItemTypes& item_types_;

    /**
     * For each bin type and each item type, the allowed orientations.
     *
     * They are the ones of the item types for the bin types they have been
     * built for; for the other bin types (the restricted last bin of a
     * sub-instance of the sequential feasibility algorithm), all the
     * orientations of the item types.
     */
    std::vector<std::vector<std::vector<Counter>>> orientation_ids_;

    /** Bin types. */
    std::vector<SeparatorBinType> bin_types_;

    /** Item type of each item. */
    std::vector<ItemTypeId> items_item_type_id_;

    /** Number of hazards per item, including the exterior. */
    Counter number_of_hazards_ = 1;

    /** State of each item. */
    std::vector<ItemState> items_state_;

    /** Items of each bin. */
    std::vector<std::vector<ItemPos>> bins_items_;

    /** Position of each item in the item list of its bin. */
    std::vector<ItemPos> items_bin_item_pos_;

    /** Collision entries of the pairs of items (triangular matrix). */
    std::vector<Entry> pair_entries_;

    /** Collision entries of the items and the hazards. */
    std::vector<Entry> hazard_entries_;

    /** Number of evaluations of candidate placements. */
    Counter number_of_evaluations_ = 0;

    /** Number of item moves. */
    Counter number_of_moves_ = 0;

    /** Limit on the number of moves (see 'MovesLimit'). */
    std::shared_ptr<MovesLimit> moves_limit_;

    /** Number of moves already added to 'moves_limit_'. */
    Counter number_of_moves_counted_ = 0;

    /** Add the new moves to 'moves_limit_' and check the limit. */
    void count_moves();

    /** Number of iterations of the separations. */
    Counter number_of_iterations_ = 0;

    /**
     * Workers (copies of the separator), created at the first iteration of a
     * separation with several workers.
     */
    std::vector<std::shared_ptr<Separator>> workers_;

};

}
}

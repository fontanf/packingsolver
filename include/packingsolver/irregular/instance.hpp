#pragma once

#include "packingsolver/algorithms/common.hpp"

#include "shape/shape.hpp"

#include <functional>
#include <memory>

namespace packingsolver
{
namespace irregular
{

using namespace shape;

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

enum class LeftoverMode
{
    BottomLeft,
    BottomRight,
    TopLeft,
    TopRight,
    Left,
    Right,
    Bottom,
    Top,
};

std::istream& operator>>(
        std::istream& in,
        LeftoverMode& leftover_mode);

std::ostream& operator<<(
        std::ostream& os,
        LeftoverMode leftover_mode);

struct Parameters
{
    /**
     * Quality rules.
     *
     * if 'quality_rules_[quality_rule][k] = 0' (resp. '1'), then defect 'k' is
     * not allowed (resp. allowed) for quality rule rule 'quality_rule'.
     */
    std::vector<std::vector<uint8_t>> quality_rules;

    /** Minimum distance between two items. */
    LengthDbl item_item_minimum_spacing = 0.0;

    /** Reference corner/edge to compute the leftover value. */
    LeftoverMode leftover_mode = LeftoverMode::BottomLeft;

    /**
     * For the BinPackingWithLeftovers objective, also compare the solutions
     * on the sum of the leftover values of the bins and on the sum of their
     * staircase leftover values, after the leftover value and the staircase
     * leftover value of the last bin.
     */
    bool leftover_all_bins = true;

    /** Scale value. */
    LengthDbl scale_value = std::numeric_limits<LengthDbl>::infinity();

    /**
     * Imposed Aspect ratio for the open dimension xy objective.
     *
     * If '-1', no aspect ratio is imposed.
     */
    LengthDbl open_dimension_xy_aspect_ratio = -1;
};

/**
 * Defect structure for a problem of type 'irregular'.
 */
struct Defect
{
    /** Shape. */
    ShapeWithHoles shape_orig;

    /** Type of the defect. */
    DefectTypeId type = -1;

    /** Minimum distance between and item and the defect. */
    LengthDbl item_defect_minimum_spacing = 0.0;

    /*
     * Computed attributes.
     */

    /** Scaled shape. */
    ShapeWithHoles shape_scaled;

    /** Inflated (scaled) shape. */
    ShapeWithHoles shape_inflated;

    std::string to_string(Counter indentation) const;
};

/**
 * Structure representing a single item placement that is already fixed
 * (i.e. part of the solution determined before the search begins).
 * Fixed items belong to a bin type and apply to every bin of that type.
 */
struct FixedItem
{
    /** Item type. */
    ItemTypeId item_type_id;

    /** Position of the bottom-left corner of the item. */
    Point bl_corner;

    /** Rotation angle of the item. */
    Angle angle;

    /** Mirror the item. */
    bool mirror;
};

/**
 * Bin type structure for a problem of type 'irregular'.
 */
struct BinType
{
    /** Cost of the bin type. */
    Profit cost;

    /** Maximum number of copies of the bin type. */
    BinPos copies;

    /** Minimum number of copies to use of the bin type. */
    BinPos copies_min;

    /** Shape of the bin type. */
    Shape shape_orig;

    /** Defects of the bin type. */
    std::vector<Defect> defects;

    /** Fixed items pre-placed in every bin of this type. */
    std::vector<FixedItem> fixed_items;

    /** Minimum distance between and item and the bin. */
    LengthDbl item_bin_minimum_spacing = 0.0;

    /** Resources of this bin type; a resource's id is its index in this vector. */
    std::vector<Resource> resources;

    /*
     * Computed attributes.
     */

    /** Scaled shape. */
    Shape shape_scaled;

    /** Borders. */
    std::vector<Defect> borders;

    /** Area of the bin type. */
    AreaDbl area_orig = 0.0;

    /** Scaled area of the bin type. */
    AreaDbl area_scaled = 0.0;

    /** Bounding box of the original bin type. */
    AxisAlignedBoundingBox aabb_orig;

    /** Bounding box of the scaled bin type. */
    AxisAlignedBoundingBox aabb_scaled;


    AreaDbl space() const { return area_orig; }

    AreaDbl packable_area(QualityRule quality_rule) const { (void)quality_rule; return 0; } // TODO

    /** Get the number of resources of this bin type. */
    inline ResourceId number_of_resources() const { return resources.size(); }

    /** Get a resource of this bin type. */
    inline const Resource& resource(ResourceId resource_id) const { return resources[resource_id]; }

    std::string to_string(Counter indentation) const;

    void write_svg(
            const std::string& file_path) const;
};

struct ItemShape
{
    /** Main shape. */
    ShapeWithHoles shape_orig;

    /** Quality rule. */
    QualityRule quality_rule = -1;

    /*
     * Computed attributes.
     */

    /** Scaled shape. */
    ShapeWithHoles shape_scaled;

    /** Inflated (scaled) shape. */
    ShapeWithHoles shape_inflated;


    bool check() const;

    std::string to_string(Counter indentation) const;
};

/**
 * Allowed rotation entry for an item type.
 *
 * Defines a continuous range of allowed angles [start_angle, end_angle] and
 * whether the item shape is mirrored (axial symmetry about the y-axis) for
 * this rotation entry.
 */
struct AllowedRotation
{
    Angle start_angle;
    Angle end_angle;
    bool mirror;
};

/**
 * Placement of a single item copy: item type, position and orientation.
 */
struct SolutionItem
{
    /** Item type. */
    ItemTypeId item_type_id;

    /** Position of the bottom-left corner of the item. */
    Point bl_corner;

    /** Rotation angle of the item. */
    Angle angle;

    /** Mirror the item. */
    bool mirror;

    /** True if this item is a fixed item of the instance. */
    bool is_fixed = false;
};

bool operator==(const SolutionItem& solution_item_1, const SolutionItem& solution_item_2);
bool operator!=(const SolutionItem& solution_item_1, const SolutionItem& solution_item_2);

std::ostream& operator<<(std::ostream &os, const SolutionItem& item);

/**
 * Item type structure for a problem of type 'irregular'.
 */
struct ItemType
{
    /** Profit of the item type. */
    Profit profit;

    /** Number of copies of the item type. */
    ItemPos copies;

    /**
     * Minimum number of copies of the item type to pack.
     *
     * Set to '-1' before 'InstanceBuilder::build()' resolves it: '0' for
     * objective 'Knapsack' (packing an item type is optional), 'copies' for
     * every other objective (every copy of every item type must be packed).
     */
    ItemPos copies_min = -1;

    /**
     * Shape of the item type.
     *
     * The shape is composed of multiple non-overlaping sub-shapes which may
     * follow different quality rules.
     */
    std::vector<ItemShape> shapes;

    /**
     * Allowed rotations of the item type.
     *
     * Each entry defines a continuous range [start_angle, end_angle] and
     * whether the item is mirrored for that range. Mirrored and non-mirrored
     * ranges are listed independently, allowing fine-grained control over
     * which (angle, mirror) combinations are permitted.
     */
    std::vector<AllowedRotation> allowed_rotations = {{0, 0, false}};

    /*
     * Computed attributes.
     */

    /** Number of copies already fixed in the instance. */
    ItemPos copies_fixed = 0;

    /** Area of the item type. */
    AreaDbl area_orig = 0;

    /** Area of the item type. */
    AreaDbl area_scaled = 0;

    /**
     * For each bin type (indexed by bin_type_id - a resource's id is only
     * ever local to one bin type, see 'BinType::resources'), the bin
     * type's resources this item type has a non-zero consumption for,
     * together with the position of this item type's own entry in each
     * resource's own 'Resource::item_consumptions' (see
     * 'ItemResourceConsumption') - so a caller can jump straight to its
     * schedule with no search. Lets algorithms that need every resource a
     * given item type (in a given bin type) might affect skip the rest,
     * instead of scanning every resource of the bin type for every item.
     */
    std::vector<std::vector<ItemResourceConsumption>> resources;

    AreaDbl space() const { return area_orig; }

    /** Return type of shape of the item type. */
    ShapeType shape_type() const;

    /**
     * Bounding box of the item type in a given orientation.
     *
     * 'type' selects the shapes: 0 for the original shapes, 1 for the scaled
     * shapes, 2 for the inflated shapes.
     */
    AxisAlignedBoundingBox compute_min_max(
            Angle angle = 0.0,
            bool mirror = false,
            int type = 0) const;

    /** Bounding boxes of the item type in one of its discrete orientations. */
    struct DiscreteRotationMinMax
    {
        /** Angle. */
        Angle angle = 0.0;

        /** Mirror. */
        bool mirror = false;

        /** Bounding boxes, for each type of shapes (see 'compute_min_max'). */
        AxisAlignedBoundingBox aabbs[3];
    };

    /**
     * Bounding boxes of the item type in its discrete allowed orientations,
     * computed when building the instance, so that 'compute_min_max' doesn't
     * need to transform the shapes again.
     */
    std::vector<DiscreteRotationMinMax> discrete_rotations_min_max;

    bool has_full_continuous_rotations() const;

    bool has_only_discrete_rotations() const;

    std::string to_string(Counter indentation) const;

    bool is_rotation_allowed(Angle angle, bool mirror) const
    {
        for (const auto& range: this->allowed_rotations)
            if (range.mirror == mirror
                    && range.start_angle <= angle
                    && angle <= range.end_angle)
                return true;
        return false;
    }

    void write_svg(
            const std::string& file_path) const;
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Instance ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class Solution;

/**
 * User-provided feasibility callback.
 *
 * Called on a fully-built Solution in addition to the built-in feasibility
 * checks; see 'FeasibilityCallbackResult' for what it may return.
 */
using FeasibilityCallback = std::function<FeasibilityCallbackResult(const Solution&)>;

/** Simplified shapes of an instance; see 'irregular/shape_simplification.hpp'. */
struct SimplifiedInstance;

/** Cache of the simplified shapes; see 'irregular/instance_cache.hpp'. */
struct SimplifiedInstancesCache;

/** Periodic packing; see 'irregular/instance_cache.hpp'. */
struct PeriodicItemPacking;

/** Cache of the periodic packings; see 'irregular/instance_cache.hpp'. */
struct PeriodicPackingsCache;

/**
 * Instance class for a problem of type "irregular".
 */
class Instance
{

public:

    /*
     * Getters
     */

    /** Get the problem type. */
    static inline ProblemType type() { return ProblemType::Irregular; };

    /** Get the objective of the problem. */
    inline Objective objective() const { return objective_; }

    /**
     * Get the knapsack goal: for the 'Knapsack' objective, the search stops
     * as soon as a solution reaches it, or as soon as the bound proves that
     * it can't be reached (e.g. a pricing problem of a column generation,
     * which only needs a column of negative reduced cost, or a proof that
     * there is none). Infinite by default.
     */
    inline Profit knapsack_goal() const { return knapsack_goal_; }

    /*
     * Getters: parameters
     */

    /** Get parameters. */
    const Parameters& parameters() const { return parameters_; }

    /** Get the scaled item_item_minimum_spacing. */
    inline LengthDbl item_spacing_scaled() const
    {
        return parameters().scale_value * parameters().item_item_minimum_spacing;
    }

    /** Get the feasibility callback. */
    inline const FeasibilityCallback& feasibility_callback() const { return feasibility_callback_; }

    /**
     * Return 'true' iff quality_rule 'quality_rule' can contain a defect of
     * type 'type'.
     */
    bool can_contain(QualityRule quality_rule, DefectTypeId type) const;

    /*
     * Getters: bin types
     */

    /** Get the number of bin types. */
    inline BinTypeId number_of_bin_types() const { return bin_types_.size(); }

    /** Get bin type i. */
    inline const BinType& bin_type(BinTypeId bin_type_id) const { return bin_types_[bin_type_id]; }

    /** Get the scaled item_bin_minimum_spacing of bin type 'bin_type_id'. */
    inline LengthDbl bin_spacing_scaled(BinTypeId bin_type_id) const
    {
        return parameters().scale_value * bin_type(bin_type_id).item_bin_minimum_spacing;
    }

    /** Get the number of bins. */
    inline BinPos number_of_bins() const { return bin_type_ids_.size(); }

    /** Get the i_pos's bin. */
    inline BinTypeId bin_type_id(BinPos bin_pos) const { return bin_type_ids_[bin_pos]; }

    /** Get the total area of the bins before bin i_pos. */
    inline AreaDbl previous_bin_area(BinPos bin_pos) const { return previous_bins_area_[bin_pos]; }

    /** Get the total area of the bins. */
    inline AreaDbl bin_area() const { return bin_area_; }

    /** Get the largest cost of the bins. */
    inline Profit largest_bin_cost() const { return largest_bin_cost_; }

    /** Get the number of defects. */
    inline DefectId number_of_defects() const { return number_of_defects_; }

    /**
     * Get the position of the last bin whose bin type has fixed items.
     * Returns -1 if no bin type has fixed items.
     */
    inline BinPos last_bin_with_fixed_items() const { return last_bin_with_fixed_items_; }

    /*
     * Getters: item types
     */

    /* Get the number of item types. */
    inline ItemTypeId number_of_item_types() const { return item_types_.size(); }

    /** Get an item type. */
    inline const ItemType& item_type(ItemTypeId item_type_id) const { return item_types_[item_type_id]; }

    /**
     * Get the simplified shapes of the instance for a maximum approximation
     * ratio (see 'irregular/shape_simplification.hpp').
     *
     * They are computed at the first call for this ratio and stored, so that
     * the next calls, and the sub-instances built from this instance (see
     * 'InstanceBuilder::set_original_instance'), get them without computing
     * them again. This method may be called from several threads: a call
     * waits until the simplified shapes are computed by another one.
     *
     * Return 'nullptr' if 'end' is true before the simplified shapes are
     * computed. In particular, if 'end' is already true, the simplified
     * shapes are only returned if they have already been computed.
     */
    const SimplifiedInstance* get_simplified_instance(
            double maximum_approximation_ratio,
            const bool& end) const;

    /**
     * Get the periodic packings of the item types with many copies, used by
     * the periodic-packing and maximal-spaces tree searches. The item types
     * of a periodic packing are given by the 'item_type_id' of its items.
     *
     * They are computed at the first call and stored, so that the next calls,
     * and the sub-instances built from this instance (see
     * 'InstanceBuilder::set_original_instance'), get them without computing
     * them again. This method may be called from several threads: a call
     * waits until the periodic packings are computed by another one.
     *
     * Return 'nullptr' if 'end' becomes true before the periodic packings
     * are computed. In particular, if 'end' is already true, they are only
     * returned if they have already been computed.
     */
    const std::vector<PeriodicItemPacking>* get_periodic_packings(
            const bool& end) const;

    /**
     * Return the scaled shape of item type 'item_type_id', shape 'item_shape_pos',
     * after applying the given mirror (axial symmetry about the y-axis) and
     * rotation angle.
     */
    ShapeWithHoles item_shape_scaled(
            ItemTypeId item_type_id,
            ItemPos item_shape_pos,
            Angle angle,
            bool mirror) const;

    /**
     * Return the original (unscaled) shape of item type 'item_type_id', shape
     * 'item_shape_pos', after applying the given mirror (axial symmetry about
     * the y-axis) and rotation angle.
     */
    ShapeWithHoles item_shape_orig(
            ItemTypeId item_type_id,
            ItemPos item_shape_pos,
            Angle angle,
            bool mirror) const;

    /** Get the number of items. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Get the number of fixed items. */
    inline ItemPos number_of_fixed_items() const { return number_of_fixed_items_; }

    /** Get the number rectangular of items. */
    inline ItemPos number_of_rectangular_items() const { return number_of_rectangular_items_; }

    /** Get the number circular of items. */
    inline ItemPos number_of_circular_items() const { return number_of_circular_items_; }

    /** Get the total area of the items. */
    inline AreaDbl item_area() const { return item_area_; }

    /** Get the mean area of the items. */
    inline AreaDbl mean_area() const { return item_area_ / number_of_items(); }

    /** Get the smallest area of the items. */
    inline AreaDbl smallest_item_area() const { return smallest_item_area_; }

    /** Get the largest area of the items. */
    inline AreaDbl largest_item_area() const { return largest_item_area_; }

    /** Get the total profit of the items. */
    inline Profit item_profit() const { return item_profit_; }

    /** Get the largest profit of the items. */
    inline Profit largest_item_profit() const { return largest_item_profit_; }

    /** Get the id of the item type with largest efficiency. */
    inline ItemTypeId largest_efficiency_item_type_id() const { return largest_efficiency_item_type_id_; }

    /** Get the largest number of copies of the items. */
    inline ItemPos largest_item_copies() const { return largest_item_copies_; }

    /** Return true iff all items have infinite copies. */
    inline bool unbounded_knapsack() const { return all_item_types_infinite_copies_; }

    /**
     * Conservatively check whether item type 'item_type_id' could fit in
     * some bin type.
     *
     * One-sided: 'false' is a guarantee the item fits in no bin type (its
     * bounding box, over every allowed discrete rotation, exceeds every
     * bin's bounding box). 'true' is not a guarantee it fits anywhere,
     * only that this couldn't be disproved (either its bounding box does
     * fit some bin, or it has a continuous rotation range, which isn't
     * checked, since exactly ruling that out would need an exact
     * geometric optimization over the range).
     */
    bool fits_some_bin(ItemTypeId item_type_id) const;

    /**
     * Conservatively check whether item type 'item_type_id' could fit in bin
     * type 'bin_type_id'.
     *
     * One-sided, like 'fits_some_bin': 'false' is a guarantee the item
     * doesn't fit in the bin type.
     */
    bool item_type_fits_bin_type(
            ItemTypeId item_type_id,
            BinTypeId bin_type_id) const;

    /**
     * Return 'true' iff some bin type has at least one resource - i.e. iff
     * it has an actual, non-trivial per-bin capacity constraint that
     * algorithms need to track.
     */
    bool resources_matter() const;

    /*
     * Export
     */

    /** Print the instance into a stream. */
    std::ostream& format(
            std::ostream& os,
            int verbosity_level = 1) const;

    /** Write the instance to a file. */
    void write(const std::string& instance_path) const;

private:

    /*
     * Private methods
     */

    /** Create an instance manually. */
    Instance() { }

    /*
     * Private attributes
     */

    /** Objective. */
    Objective objective_ = Objective::BinPacking;

    /** Knapsack goal. */
    Profit knapsack_goal_ = std::numeric_limits<Profit>::infinity();

    /** Parameters. */
    Parameters parameters_;

    /** User-provided feasibility callback. */
    FeasibilityCallback feasibility_callback_ = [](const Solution&) { return FeasibilityCallbackResult(); };

    /** Bin types. */
    std::vector<BinType> bin_types_;

    /** Item types. */
    std::vector<ItemType> item_types_;

    /**
     * Cache of the simplified shapes; see 'get_simplified_instance'. Shared
     * by the copies of the instance.
     */
    std::shared_ptr<SimplifiedInstancesCache> simplified_instances_cache_;

    /**
     * Cache of the periodic packings; see 'get_periodic_packings'. Shared by
     * the copies of the instance.
     */
    std::shared_ptr<PeriodicPackingsCache> periodic_packings_cache_;

    /*
     * Private attributes computed by the 'build' method
     */

    /** Convert bin position to bin type. */
    std::vector<BinTypeId> bin_type_ids_;

    /** For each bin position, the area of the previous bins. */
    std::vector<AreaDbl> previous_bins_area_;

    /** Total bin area. */
    AreaDbl bin_area_ = 0;

    /** Largest bin cost. */
    Profit largest_bin_cost_ = 0.0;

    /** Number of defects. */
    DefectId number_of_defects_ = 0;

    /** Position of the last bin whose bin type has fixed items (-1 if none). */
    BinPos last_bin_with_fixed_items_ = -1;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Number of fixed items. */
    ItemPos number_of_fixed_items_ = 0;

    /** Number of rectangular items. */
    ItemPos number_of_rectangular_items_ = 0;

    /** Number of circular items. */
    ItemPos number_of_circular_items_ = 0;

    /** Total item area. */
    AreaDbl item_area_ = 0;

    /** Total item profit. */
    Profit item_profit_ = 0;

    /** Largest item profit. */
    Profit largest_item_profit_ = 0.0;

    /** Smallest item area. */
    AreaDbl smallest_item_area_ = std::numeric_limits<AreaDbl>::infinity();

    /** Largest item area. */
    AreaDbl largest_item_area_ = 0.0;

    /** Id of the item with largest efficiency. */
    ItemTypeId largest_efficiency_item_type_id_ = -1;

    /** Largest item copies. */
    ItemPos largest_item_copies_ = 0;

    /** True iff all item types have an infinite number of copies. */
    bool all_item_types_infinite_copies_ = false;


    friend class InstanceBuilder;

};

}
}

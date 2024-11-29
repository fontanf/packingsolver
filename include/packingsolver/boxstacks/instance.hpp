#pragma once

#include "packingsolver/algorithms/common.hpp"
#include "packingsolver/algorithms/truck.hpp"
#include "packingsolver/rectangle/instance.hpp"

namespace packingsolver
{
namespace boxstacks
{

struct Point
{
    /** x-coordinate. */
    Length x;

    /** y-coordinate. */
    Length y;

    /** z-coordinate. */
    Length z;
};

std::ostream& operator<<(
        std::ostream& os,
        Point xyz);

struct Box
{
    /** x-length. */
    Length x;

    /** y-length. */
    Length y;

    /** z-length. */
    Length z;

    /** Get the volume of the box. */
    Volume volume() const { return x * y * z; }

    /** Get the length of the largest size of the box. */
    Length max() const { return std::max(std::max(x, y), z); }
};

std::ostream& operator<<(
        std::ostream& os,
        Box box);

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Group structure.
 *
 * Item types from the same group are delivered together.
 *
 * The way groups are packed in the bins depends on the unloading constraint
 * type of the instance.
 */
struct Group
{
    /** Id of the group. */
    GroupId id;

    /** Item types belonging to this group. */
    std::vector<ItemTypeId> item_types;

    /** Number of items. */
    ItemPos number_of_items = 0;
};

/**
 * Item type structure for a problem of type 'boxstacks'.
 */
struct ItemType
{
    /** Id of the item type. */
    ItemTypeId id;

    /** Dimensions of the item type. */
    Box box;

    /** Profit of the item type. */
    Profit profit;

    /** Number of copies of the item type. */
    ItemPos copies;

    /** Group of the item type for the unloading constraint. */
    GroupId group_id;

    /**
     * Allowed rotations of the item type.
     *
     * Rotations
     * - 0: x -> x, y -> y, z -> z (no rotation)
     * - 1: x -> y, y -> x, z -> z
     * - 2: x -> z, y -> y, z -> x
     * - 3: x -> y, y -> z, z -> x
     * - 4: x -> x, y -> z, z -> y
     * - 5: x -> z, y -> x, z -> y
     *
     * Examples:
     * - 1: only default orientation
     * - 3: the top faces is z, both remaining rotations are allowed
     * - 15: the top faces cannot be y, all remaining orientations are allowed
     * - 51: the top faces cannot be x, all remaining orientations are allowed
     * - 63: all orientations are allowed
     */
    int rotations;

    /** Weight of the item type. */
    Weight weight = 0;

    /**
     * Stackability id.
     *
     * An item can be packed over another item only if they have the same x and
     * y dimensions and the same 'stackability_id'.
     */
    StackabilityId stackability_id = 0;

    /**
     * Nesting height.
     *
     * Extra height to add when the item is packed above another item.
     */
    Length nesting_height = 0;

    /**
     * Maximum stackability.
     *
     * Maximum number of items in a stack containing this item type.
     */
    ItemPos maximum_stackability = std::numeric_limits<ItemPos>::max();

    /** Maximum weight of the items packed above items of this type. */
    Weight maximum_weight_above = std::numeric_limits<Weight>::max();

    /*
     * Computed attributes
     */

    /** Get the volume of the item type. */
    inline Volume volume() const { return box.volume(); }

    inline Area area() const { return box.x * box.y; }

    inline Volume space() const { return volume(); }

    inline bool can_rotate(int rotation) const { return ((rotations >> rotation) & 1); }
};

std::ostream& operator<<(
        std::ostream& os,
        const ItemType& item_type);

/**
 * Bin type structure for a problem of type 'boxstacks'.
 */
struct BinType
{
    /** Id of the bin type. */
    BinTypeId id;

    /** Dimensions of the bin type. */
    Box box;

    /** Cost of the bin type. */
    Profit cost;

    /** Maximum number of copies of the bin type. */
    BinPos copies;

    /** Minimum number of copies to use of the bin type. */
    BinPos copies_min;

    /** Defects of the bin type. */
    std::vector<rectangle::Defect> defects;

    /** Maximum weight. */
    Weight maximum_weight = 0;

    /**
     * Maximum stack density (in Weight / Length^2).
     *
     * The density is computed as the weight of the stack divided by its area.
     */
    double maximum_stack_density = std::numeric_limits<double>::max();

    /** Semi-trailer truck data. */
    SemiTrailerTruckData semi_trailer_truck_data;

    /*
     * Computed attributes
     */

    /** Get the floor area of the bin type. */
    inline Area area() const { return box.x * box.y; }

    /** Get the volume of the bin type. */
    inline Volume volume() const { return box.volume(); }

    inline Volume space() const { return volume(); }

};

std::ostream& operator<<(
        std::ostream& os,
        const BinType& bin_type);

struct Parameters
{
    /** Unloading constraint. */
    rectangle::UnloadingConstraint unloading_constraint = rectangle::UnloadingConstraint::None;

    /**
     * 'true' iff weight constraints must be satisfied for all items belonging
     * to a larger or equal group.
     */
    std::vector<bool> check_weight_constraints;
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Instance ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Instance class for a problem of type "boxstacks".
 */
class Instance
{

public:

    /*
     * Getters
     */

    /** Get the problem type. */
    static inline ProblemType type() { return ProblemType::BoxStacks; };

    /** Get the objective of the problem. */
    inline Objective objective() const { return objective_; }

    /*
     * Getters: parameters
     */

    /** Get parameters. */
    const Parameters& parameters() const { return parameters_; }

    /** Get unloading constraint type. */
    inline rectangle::UnloadingConstraint unloading_constraint() const { return parameters_.unloading_constraint; }

    inline bool check_weight_constraints(GroupId group_id) const { if (group_id >= (GroupId)parameters_.check_weight_constraints.size()) return true; return parameters_.check_weight_constraints[group_id]; }

    /*
     * Getters: bin types
     */

    /** Get the number of bin types. */
    inline BinTypeId number_of_bin_types() const { return bin_types_.size(); }

    /** Get a bin type. */
    inline const BinType& bin_type(BinTypeId bin_type_id) const { return bin_types_[bin_type_id]; }

    /** Get the number of bins. */
    inline BinPos number_of_bins() const { return bin_type_ids_.size(); }

    /** Get the id of a bin at a given position. */
    inline BinTypeId bin_type_id(BinPos bin_pos) const { return bin_type_ids_[bin_pos]; }

    /** Get the total volume of the bins. */
    inline Volume bin_volume() const { return bin_volume_; }

    /** Get the total floor area of the bins. */
    inline Area bin_area() const { return bin_area_; }

    /** Get the total weight of the bins. */
    inline Weight bin_weight() const { return bin_weight_; }

    /** Get the number of defects. */
    inline DefectId number_of_defects() const { return number_of_defects_; }

    /** Get the total volume of the bins before bin i_pos. */
    inline Area previous_bin_volume(BinPos bin_pos) const { return previous_bins_volume_[bin_pos]; }

    /*
     * Getters: bin type dimensions
     */

    /** Get the x of a bin type depending on its orientation. */
    inline Length x(
            const BinType& bin_type,
            Direction o) const;

    /** Get the y of a bin type depending on its orientation. */
    inline Length y(
            const BinType& bin_type,
            Direction o) const;

    /** Get the z of a bin type. */
    inline Length z(
            const BinType& bin_type) const;

    /*
     * Getters: defect coordinates
     */

    /**
     * Get the start x coordinate of a defect depending on the orientatino of
     * the bin.
     */
    inline Length x_start(
            const rectangle::Defect& defect,
            Direction o) const;

    /**
     * Get the end x coordinate of a defect depending on the orientatino of the
     * bin.
     */
    inline Length x_end(
            const rectangle::Defect& defect,
            Direction o) const;

    /**
     * Get the end y coordinate of a defect depending on the orientatino of the
     * bin.
     */
    inline Length y_end(
            const rectangle::Defect& defect,
            Direction o) const;

    /**
     * Get the start y coordinate of a defect depending on the orientatino of
     * the bin.
     */
    inline Length y_start(
            const rectangle::Defect& defect,
            Direction o) const;

    /*
     * Getters: item types
     */

    /* Get the number of item types. */
    inline ItemTypeId number_of_item_types() const { return item_types_.size(); }

    /** Get an item type. */
    inline const ItemType& item_type(ItemTypeId item_type_id) const { return item_types_[item_type_id]; }

    /** Get the number of items. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Get the number of groups. */
    inline StackId number_of_groups() const { return groups_.size(); }

    /** Get the total volume of the items. */
    inline Volume item_volume() const { return item_volume_; }

    /** Get the mean volume of the items. */
    inline Volume mean_volume() const { return item_volume_ / number_of_items(); }

    /** Get the total weight of the items. */
    inline Weight item_weight() const { return item_weight_; }

    /** Get the total profit of the items. */
    inline Profit item_profit() const { return item_profit_; }

    /** Get the id of the item type with maximum efficiency. */
    inline ItemTypeId max_efficiency_item_type_id() const { return max_efficiency_item_type_id_; }

    /** Return true iff all items have infinite copies. */
    inline bool unbounded_knapsack() const { return all_item_types_infinite_copies_; }

    /** Get a group. */
    inline const Group& group(GroupId group_id) const { return groups_[group_id]; }

    /*
     * Getters: item type dimensions
     */

    /**
     * Get the x of an item depending on its rotation and the orientation of
     * the bin.
     */
    inline Length x(
            const ItemType& item_type,
            int rotation,
            Direction o) const;

    /**
     * Get the y of an item depending on its rotation and the orientation of
     * the bin.
     */
    inline Length y(
            const ItemType& item_type,
            int rotation,
            Direction o) const;

    /**
     * Get the z of an item depending on its rotation.
     */
    inline Length z(
            const ItemType& item_type,
            int rotation) const;

    /*
     * Export
     */

    /** Print the instance into a stream. */
    std::ostream& format(
            std::ostream& os,
            int verbosity_level = 1) const;

    /** Write the instance to a file. */
    void write(const std::string& instance_path) const;

    /** Write the items to a file. */
    void write_item_types(const std::string& items_path) const;

    /** Write the bins to a file. */
    void write_bin_types(const std::string& bins_path) const;

    /** Write the parameters to a file. */
    void write_parameters(const std::string& parameters_path) const;

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
    Objective objective_;

    /** Parameters. */
    Parameters parameters_;

    /** Bin types. */
    std::vector<BinType> bin_types_;

    /** Item types. */
    std::vector<ItemType> item_types_;

    /*
     * Private attributes computed by the 'build' method
     */

    /** For each bin position, the corresponding bin type. */
    std::vector<BinTypeId> bin_type_ids_;

    /** For each bin position, the volume of the previous bins. */
    std::vector<Area> previous_bins_volume_;

    /** Total bin volume. */
    Volume bin_volume_ = 0;

    /** Total bin floor area. */
    Area bin_area_ = 0;

    /** Total weight of the bins. */
    Weight bin_weight_ = 0.0;

    /** Number of defects. */
    DefectId number_of_defects_ = 0;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Groups. */
    std::vector<Group> groups_;

    /** Total item volume. */
    Volume item_volume_ = 0;

    /** Total weight of the items. */
    Weight item_weight_ = 0.0;

    /** Total item profit. */
    Profit item_profit_ = 0;

    /** Id of the item with maximum efficiency. */
    ItemTypeId max_efficiency_item_type_id_ = -1;

    /** True iff all item types have an infinite number of copies. */
    bool all_item_types_infinite_copies_ = false;

    friend class InstanceBuilder;

};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Inlined methods ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Length Instance::x(
        const BinType& bin_type,
        Direction o) const
{
    return (o == Direction::X)? bin_type.box.x: bin_type.box.y;
}

Length Instance::y(
        const BinType& bin_type,
        Direction o) const
{
    return (o == Direction::X)? bin_type.box.y: bin_type.box.x;
}

Length Instance::z(
        const BinType& bin_type) const
{
    return bin_type.box.z;
}

Length Instance::x_start(
        const rectangle::Defect& defect,
        Direction o) const
{
    return (o == Direction::X)? defect.pos.x: defect.pos.y;
}

Length Instance::x_end(
        const rectangle::Defect& defect,
        Direction o) const
{
    return (o == Direction::X)?
        defect.pos.x + defect.rect.x:
        defect.pos.y + defect.rect.y;
}

Length Instance::y_end(
        const rectangle::Defect& defect,
        Direction o) const
{
    return (o == Direction::X)?
        defect.pos.y + defect.rect.y:
        defect.pos.x + defect.rect.x;
}

Length Instance::y_start(
        const rectangle::Defect& defect,
        Direction o) const
{
    return (o == Direction::X)? defect.pos.y: defect.pos.x;
}

Length Instance::x(
        const ItemType& item,
        int rotation,
        Direction o) const
{
    if (o == Direction::X) {
        switch (rotation) {
        case 0: {
            return item.box.x;
        } case 1: {
            return item.box.y;
        } case 2: {
            return item.box.z;
        } case 3: {
            return item.box.y;
        } case 4: {
            return item.box.x;
        } case 5: {
            return item.box.z;
        } default: {
            throw std::invalid_argument(
                    "Incorrect rotation value: '"
                    + std::to_string(rotation) + "'");
        }
        }
    } else {
        switch (rotation) {
        case 0: {
            return item.box.y;
        } case 1: {
            return item.box.x;
        } case 2: {
            return item.box.z;
        } case 3: {
            return item.box.x;
        } case 4: {
            return item.box.y;
        } case 5: {
            return item.box.z;
        } default: {
            throw std::invalid_argument(
                    "Incorrect rotation value: '"
                    + std::to_string(rotation) + "'");
        }
        }
    }
}

Length Instance::y(
        const ItemType& item,
        int rotation,
        Direction o) const
{
    if (o == Direction::X) {
        switch (rotation) {
        case 0: {
            return item.box.y;
        } case 1: {
            return item.box.x;
        } case 2: {
            return item.box.y;
        } case 3: {
            return item.box.z;
        } case 4: {
            return item.box.z;
        } case 5: {
            return item.box.x;
        } default: {
            throw std::invalid_argument(
                    "Incorrect rotation value: '"
                    + std::to_string(rotation) + "'");
        }
        }
    } else {
        switch (rotation) {
        case 0: {
            return item.box.x;
        } case 1: {
            return item.box.y;
        } case 2: {
            return item.box.x;
        } case 3: {
            return item.box.z;
        } case 4: {
            return item.box.z;
        } case 5: {
            return item.box.y;
        } default: {
            throw std::invalid_argument(
                    "Incorrect rotation value: '"
                    + std::to_string(rotation) + "'");
        }
        }
    }
}

Length Instance::z(
        const ItemType& item,
        int rotation) const
{
    switch (rotation) {
    case 0: {
        return item.box.z;
    } case 1: {
        return item.box.z;
    } case 2: {
        return item.box.x;
    } case 3: {
        return item.box.x;
    } case 4: {
        return item.box.y;
    } case 5: {
        return item.box.y;
    } default: {
        throw std::invalid_argument(
                "Incorrect rotation value: '"
                + std::to_string(rotation) + "'");
    }
    }
}

}
}


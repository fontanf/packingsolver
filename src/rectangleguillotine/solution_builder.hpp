#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

class SolutionBuilder
{

public:

    /** Constructor. */
    SolutionBuilder(const Instance& instance): solution_(instance) { }

    /** Add a bin. */
    void add_bin(
            BinTypeId bin_type_id,
            BinPos copies,
            CutOrientation first_cut_orientation);

    /**
     * Append a node.
     *
     * The depth cannot be strictly greater than the depth of the last added
     * node. If the depth is the same as the last
     *
     * The depth cannot be strictly smaller than 1 (the node at depth 0 is the
     * root node of the bin).
     *
     * The new node will be the last child of the last node added at depth
     * 'depth - 1'.
     */
    void add_node(
            Depth depth,
            Length cut_position);

    /** Set the item of the last node. */
    void set_last_node_item(
            ItemTypeId item_type_id);

    /*
     * Build
     */

    /** Build. */
    Solution build();

private:

    /*
     * Private methods
     */

    /*
     * Private attributes
     */

    /** Solution. */
    Solution solution_;

};

}
}

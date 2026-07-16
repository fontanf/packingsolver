#pragma once

#include "packingsolver/boxstacks/solution.hpp"

namespace packingsolver
{
namespace boxstacks
{

class SolutionBuilder
{

public:

    /** Constructor. */
    SolutionBuilder(const Instance& instance): solution_(instance) { }

    /** Add a bin at the end of the solution. */
    BinPos add_bin(
            BinTypeId bin_type_id,
            BinPos copies);

    /** Add a stack to the solution. */
    StackId add_stack(
            BinPos bin_pos,
            Length x_start,
            Length x_end,
            Length y_start,
            Length y_end);

    /** Add an item to the solution. */
    void add_item(
            BinPos bin_pos,
            StackId stack_id,
            ItemTypeId item_type_id,
            Rotation rotation);

    /*
     * Build
     */

    /** Build. */
    Solution build();

private:

    /*
     * Private attributes
     */

    /** Solution. */
    Solution solution_;

};

}
}

#pragma once

#include "packingsolver/box/solution.hpp"

namespace packingsolver
{
namespace box
{

class SolutionBuilder
{

public:

    /** Constructor. */
    SolutionBuilder(const Instance& instance): solution_(instance) { }

    /** Read a solution from a file. */
    void read(const std::string& certificate_path);

    /** Add a bin at the end of the solution. */
    BinPos add_bin(
            BinTypeId bin_type_id,
            BinPos copies);

    /** Add an item to the solution. */
    void add_item(
            BinPos bin_pos,
            ItemTypeId item_type_id,
            const Point& bl_corner,
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

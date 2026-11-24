#pragma once

#include "packingsolver/irregular/solution.hpp"

#include <istream>

namespace packingsolver
{
namespace irregular
{

class SolutionBuilder
{

public:

    /** Constructor. */
    SolutionBuilder(const Instance& instance): solution_(instance) { }

    /** Read a solution from a certificate file. */
    void read(const std::string& certificate_path);

    /** Read a solution from a certificate stream. */
    void read(std::istream& certificate_stream);

    /** Add a bin at the end of the solution. */
    BinPos add_bin(
            BinTypeId bin_type_id,
            BinPos copies);

    /** Add an item to the solution. */
    void add_item(
            BinPos bin_pos,
            ItemTypeId item_type_id,
            Point bl_corner,
            Angle angle,
            bool mirror);

    /**
     * Add the fixed item at position 'fixed_item_pos' of the bin type of bin
     * 'bin_pos' to the solution.
     */
    void add_fixed_item(
            BinPos bin_pos,
            ItemPos fixed_item_pos);

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

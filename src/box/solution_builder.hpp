#pragma once

#include "packingsolver/box/solution.hpp"

#include <istream>

namespace packingsolver
{
namespace box
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

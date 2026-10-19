#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <algorithm>
#include <vector>

namespace packingsolver
{

/**
 * What a bin type can hold of two item types A and B, A dominating B (see
 * 'dominated_item_copies_bound').
 */
struct DominatedItemsBinType
{
    /** Number of copies of the bin type. */
    BinPos copies = 0;

    /** 'true' iff B fits in the bin type. */
    bool fits_b = false;

    /**
     * Upper bound on the number of copies of A in a bin of this type ('0'
     * if A doesn't fit in it).
     */
    Counter maximum_number_of_copies_a = 0;

    /**
     * Upper bound on the number of items of A and B together in a bin of
     * this type ('0' if neither fits in it).
     */
    Counter maximum_number_of_items = 0;

    /**
     * 'true' iff A and B, and B and B, can not be packed together in a bin
     * of this type.
     */
    bool exclusive = false;
};

/**
 * Upper bound on the number of copies of an item type B an optimal
 * solution needs, when another item type A dominates it: in any solution,
 * a copy of B can be replaced by an unused copy of A, at the same place,
 * without lowering the profit or breaking any constraint.
 *
 * Replacing copies of B this way stops either once no copy of B is used,
 * or once every copy of A is used. In the second case:
 * - the bins hold at most 'sum(copies * maximum_number_of_items)' items of
 *   A and B, so at most that number minus 'copies_a' copies of B are used;
 * - if every bin type can hold B and is 'exclusive', a bin holding a copy
 *   of B holds no other item of A and B, and the copies of A take at least
 *   'ceil(copies_a / max(maximum_number_of_copies_a))' bins, so at most
 *   the number of bins minus that number of copies of B are used.
 *
 * The dominance alone ('A' at least as profitable, fitting wherever B
 * does, ...) is not enough to remove B: with several bins, or several
 * copies of B, a solution can use every copy of A and copies of B in other
 * bins.
 */
inline Counter dominated_item_copies_bound(
        Counter copies_a,
        Counter copies_b,
        const std::vector<DominatedItemsBinType>& bin_types)
{
    Counter number_of_items = 0;
    Counter number_of_bins = 0;
    Counter maximum_number_of_copies_a = 0;
    bool exclusive = true;
    for (const DominatedItemsBinType& bin_type: bin_types) {
        if (bin_type.maximum_number_of_items == 0)
            continue;
        number_of_items += bin_type.copies * bin_type.maximum_number_of_items;
        number_of_bins += bin_type.copies;
        maximum_number_of_copies_a = (std::max)(
                maximum_number_of_copies_a,
                bin_type.maximum_number_of_copies_a);
        if (!bin_type.fits_b || !bin_type.exclusive)
            exclusive = false;
    }
    Counter bound = (std::min)(copies_b, number_of_items - copies_a);
    if (exclusive && maximum_number_of_copies_a > 0) {
        Counter number_of_bins_a = (copies_a + maximum_number_of_copies_a - 1)
            / maximum_number_of_copies_a;
        bound = (std::min)(bound, number_of_bins - number_of_bins_a);
    }
    return (std::max)((Counter)0, bound);
}

}

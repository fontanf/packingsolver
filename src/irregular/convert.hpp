#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

/**
 * Convert a strip-packing instance into a bin-packing instance following the
 * methodology of Song et al. (2014):
 * - The stock sheet is made square (length set equal to width).
 * - The demand of each piece is multiplied by 100.
 */
Instance convert_song2014(const Instance& instance);

/**
 * Convert a strip-packing instance into a bin-packing instance following the
 * methodology of Martinez et al. (2017):
 * - Let md be the maximum length or width across all pieces in their initial
 *   orientation. The bin is a square of side factor * md.
 * - factor = 1.1 (Nest-SB), 1.5 (Nest-MB), or 2.0 (Nest-LB).
 */
Instance convert_martinez2017(const Instance& instance, double factor);

}
}

#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace packingsolver
{

/**
 * Value of a leftover.
 *
 * The leftover of a bin is the bin minus the box used by its items, the used
 * box being in a corner of the bin. On an axis, the used box may span the
 * whole bin, in which case the leftover is a box; otherwise it is an L-shape
 * (in two dimensions) or a box minus a corner box (in three dimensions).
 *
 * The value of a leftover is the integral, over the points of the leftover,
 * of their distance to the edge of the leftover, the distance being the
 * infinity norm (the half side of the largest square, or cube, centered on
 * the point and included in the leftover). Equivalently, it is the integral,
 * over t, of the area (or volume) of the leftover shrunk by t on every side.
 *
 * It is the area of the leftover times the average distance of its points to
 * its edge: a leftover which is thin, or split into small pieces, is worth
 * less than a compact leftover of the same area. The value of a w x h
 * rectangle, h <= w, is h^2 (3w - h) / 12, and the value of a square of side
 * s is s^3 / 6.
 *
 * The leftover shrunk by t is the bin shrunk by t minus the used box grown by
 * t, so its area (or volume) is:
 *     prod_i (D_i - 2t) - prod_i min(u_i, D_i - 2t)
 * where 'D' are the dimensions of the bin and 'u' the dimensions of the used
 * box. It is a polynomial of degree at most 3 between two consecutive values
 * of t where an arm of the leftover vanishes, (D_i - u_i) / 2, so Simpson's
 * rule computes the integral exactly on each of these intervals.
 */
template <std::size_t N>
double leftover_value(
        const std::array<double, N>& bin_dimensions,
        const std::array<double, N>& used_dimensions)
{
    static_assert(N >= 1 && N <= 3, "leftover_value: 1 to 3 dimensions");

    // Area (or volume) of the leftover shrunk by t.
    auto shrunk_leftover = [&bin_dimensions, &used_dimensions](double t)
    {
        double bin = 1.0;
        double used = 1.0;
        for (std::size_t i = 0; i < N; ++i) {
            double d = (std::max)(0.0, bin_dimensions[i] - 2 * t);
            double u = (std::max)(0.0, used_dimensions[i]);
            bin *= d;
            used *= (std::min)(u, d);
        }
        return (std::max)(0.0, bin - used);
    };

    double t_max = bin_dimensions[0] / 2;
    for (std::size_t i = 1; i < N; ++i)
        t_max = (std::min)(t_max, bin_dimensions[i] / 2);
    if (t_max <= 0)
        return 0.0;

    // Breakpoints. This function is called for each node of the tree
    // searches, so they are stored in a fixed-size array.
    std::array<double, N + 2> breakpoints;
    std::size_t number_of_breakpoints = 0;
    breakpoints[number_of_breakpoints++] = 0.0;
    breakpoints[number_of_breakpoints++] = t_max;
    for (std::size_t i = 0; i < N; ++i) {
        double t = (bin_dimensions[i] - used_dimensions[i]) / 2;
        if (t > 0 && t < t_max)
            breakpoints[number_of_breakpoints++] = t;
    }
    std::sort(breakpoints.begin(), breakpoints.begin() + number_of_breakpoints);

    // Simpson's rule on each interval.
    double value = 0.0;
    for (std::size_t pos = 0; pos + 1 < number_of_breakpoints; ++pos) {
        double t1 = breakpoints[pos];
        double t2 = breakpoints[pos + 1];
        if (t2 <= t1)
            continue;
        value += (t2 - t1) / 6 * (
                shrunk_leftover(t1)
                + 4 * shrunk_leftover((t1 + t2) / 2)
                + shrunk_leftover(t2));
    }
    return value;
}

/**
 * Value of a box: the value of the leftover of a bin of these dimensions with
 * no used box (see 'leftover_value').
 */
template <std::size_t N>
double box_leftover_value(
        const std::array<double, N>& dimensions)
{
    std::array<double, N> used_dimensions;
    used_dimensions.fill(0.0);
    return leftover_value(dimensions, used_dimensions);
}

/**
 * Value of a leftover which is cut into boxes, given the value of a box.
 *
 * The leftover, a bin minus a used box in a corner, is cut into boxes with
 * guillotine cuts along the faces of the used box: a cut along axis i at u_i
 * splits the leftover into the box beyond the cut and the leftover of the
 * bin of dimension u_i along axis i, which is cut in turn along the other
 * axes ('axes' is the bit mask of the axes which are not cut yet). The value
 * is the largest, over the orders of the cuts, of the sum of the values of
 * the boxes.
 */
template <std::size_t N, typename BoxValue>
double cut_leftover_value(
        const std::array<double, N>& bin_dimensions,
        const std::array<double, N>& used_dimensions,
        const BoxValue& box_value,
        unsigned axes = (1u << N) - 1)
{
    double best = -1;
    for (std::size_t i = 0; i < N; ++i) {
        if (!(axes & (1u << i)))
            continue;
        if (used_dimensions[i] >= bin_dimensions[i])
            continue;
        // Box beyond the cut.
        std::array<double, N> box = bin_dimensions;
        box[i] = bin_dimensions[i] - (std::max)(0.0, used_dimensions[i]);
        double v = box_value(box);
        // Rest of the leftover.
        std::array<double, N> rest = bin_dimensions;
        rest[i] = (std::max)(0.0, used_dimensions[i]);
        v += cut_leftover_value(rest, used_dimensions, box_value, axes & ~(1u << i));
        best = (std::max)(best, v);
    }
    // If no cut is possible, the rest is entirely used.
    return (best < 0)? 0.0: best;
}

/**
 * Value of a leftover which is cut into boxes, the value of a box being
 * given by 'box_leftover_value'.
 */
template <std::size_t N>
double cut_leftover_value(
        const std::array<double, N>& bin_dimensions,
        const std::array<double, N>& used_dimensions)
{
    return cut_leftover_value(
            bin_dimensions,
            used_dimensions,
            [](const std::array<double, N>& box) { return box_leftover_value(box); });
}

/**
 * Area (or volume) of the leftover of a bin minus a used box in a corner.
 */
template <std::size_t N>
double leftover_area(
        const std::array<double, N>& bin_dimensions,
        const std::array<double, N>& used_dimensions)
{
    double bin = 1.0;
    double used = 1.0;
    for (std::size_t i = 0; i < N; ++i) {
        bin *= bin_dimensions[i];
        used *= (std::min)(
                bin_dimensions[i],
                (std::max)(0.0, used_dimensions[i]));
    }
    return bin - used;
}

/**
 * Value of the leftover of a bin minus a used box in a corner, kept whole
 * ('cut' false) or cut into boxes ('cut' true), for a leftover value type.
 *
 * With 'LeftoverValueType::SquaredArea', the value is the square of the area
 * (or volume) of the leftover, or the sum of the squares of the areas of its
 * pieces if it is cut.
 */
template <std::size_t N>
double leftover_value(
        LeftoverValueType leftover_value_type,
        const std::array<double, N>& bin_dimensions,
        const std::array<double, N>& used_dimensions,
        bool cut)
{
    switch (leftover_value_type) {
    case LeftoverValueType::SquaredArea: {
        if (cut) {
            return cut_leftover_value(
                    bin_dimensions,
                    used_dimensions,
                    [](const std::array<double, N>& box)
                    {
                        double area = 1.0;
                        for (std::size_t i = 0; i < N; ++i)
                            area *= box[i];
                        return area * area;
                    });
        }
        double area = leftover_area(bin_dimensions, used_dimensions);
        return area * area;
    } case LeftoverValueType::Depth: {
        return (cut)?
            cut_leftover_value(bin_dimensions, used_dimensions):
            leftover_value(bin_dimensions, used_dimensions);
    }
    }
    return 0.0;
}

/**
 * Volume of the union of the boxes [0, e[0]] x ... x [0, e[N - 1]], for the
 * extents 'e' given.
 */
template <std::size_t N>
struct CornerBoxesUnionVolume
{
    static double compute(std::vector<std::array<double, N>> extents)
    {
        // By decreasing last coordinate: between two consecutive values of
        // the last coordinate, the section of the union is the union of the
        // sections of the boxes whose last coordinate is greater.
        std::sort(
                extents.begin(),
                extents.end(),
                [](
                    const std::array<double, N>& extent_1,
                    const std::array<double, N>& extent_2)
                {
                    return extent_1[N - 1] > extent_2[N - 1];
                });
        double volume = 0;
        std::vector<std::array<double, N - 1>> sections;
        for (std::size_t pos = 0; pos < extents.size(); ++pos) {
            std::array<double, N - 1> section;
            for (std::size_t i = 0; i < N - 1; ++i)
                section[i] = extents[pos][i];
            sections.push_back(section);
            double next = (pos + 1 < extents.size())? extents[pos + 1][N - 1]: 0.0;
            double height = extents[pos][N - 1] - next;
            if (height > 0)
                volume += CornerBoxesUnionVolume<N - 1>::compute(sections) * height;
        }
        return volume;
    }
};

template <>
struct CornerBoxesUnionVolume<1>
{
    static double compute(const std::vector<std::array<double, 1>>& extents)
    {
        double length = 0;
        for (const auto& extent: extents)
            length = (std::max)(length, extent[0]);
        return length;
    }
};

/**
 * Value of a staircase leftover: a bin minus the union of the boxes between
 * its corner and the extents given (the staircase of its items), for a
 * leftover value type.
 *
 * With 'LeftoverValueType::Depth', the value is computed as for an L-shape
 * (see 'leftover_value'): the leftover shrunk by t is the bin shrunk by t
 * minus the union of the boxes of dimensions min(e_i, D_i - 2t), whose
 * volume is a polynomial of degree at most N between two consecutive values
 * of t where a step of the staircase vanishes, (D_i - e_i) / 2.
 */
template <std::size_t N>
double staircase_leftover_value(
        LeftoverValueType leftover_value_type,
        const std::array<double, N>& bin_dimensions,
        const std::vector<std::array<double, N>>& extents)
{
    // Volume of the leftover shrunk by t.
    auto shrunk_leftover = [&bin_dimensions, &extents](double t)
    {
        double bin = 1.0;
        std::array<double, N> shrunk_bin;
        for (std::size_t i = 0; i < N; ++i) {
            shrunk_bin[i] = (std::max)(0.0, bin_dimensions[i] - 2 * t);
            bin *= shrunk_bin[i];
        }
        std::vector<std::array<double, N>> clipped_extents = extents;
        for (auto& extent: clipped_extents)
            for (std::size_t i = 0; i < N; ++i)
                extent[i] = (std::min)(shrunk_bin[i], (std::max)(0.0, extent[i]));
        return (std::max)(0.0, bin - CornerBoxesUnionVolume<N>::compute(clipped_extents));
    };

    if (leftover_value_type == LeftoverValueType::SquaredArea) {
        double area = shrunk_leftover(0.0);
        return area * area;
    }

    double t_max = bin_dimensions[0] / 2;
    for (std::size_t i = 1; i < N; ++i)
        t_max = (std::min)(t_max, bin_dimensions[i] / 2);
    if (t_max <= 0)
        return 0.0;

    // Breakpoints.
    std::vector<double> breakpoints = {0.0, t_max};
    for (const auto& extent: extents) {
        for (std::size_t i = 0; i < N; ++i) {
            double t = (bin_dimensions[i] - extent[i]) / 2;
            if (t > 0 && t < t_max)
                breakpoints.push_back(t);
        }
    }
    std::sort(breakpoints.begin(), breakpoints.end());
    breakpoints.erase(
            std::unique(breakpoints.begin(), breakpoints.end()),
            breakpoints.end());

    // Simpson's rule on each interval.
    double value = 0.0;
    double previous = shrunk_leftover(breakpoints[0]);
    for (std::size_t pos = 0; pos + 1 < breakpoints.size(); ++pos) {
        double t1 = breakpoints[pos];
        double t2 = breakpoints[pos + 1];
        double next = shrunk_leftover(t2);
        value += (t2 - t1) / 6 * (
                previous
                + 4 * shrunk_leftover((t1 + t2) / 2)
                + next);
        previous = next;
    }
    return value;
}

/**
 * Return 'true' iff a leftover value 'value_1' is strictly greater than a
 * leftover value 'value_2'.
 *
 * With 'LeftoverValueType::SquaredArea', the values only depend on integer
 * lengths, through products and sums, so they are compared without
 * tolerance; with 'LeftoverValueType::Depth', they are compared with a
 * relative tolerance.
 */
inline bool strictly_greater_leftover_value(
        LeftoverValueType leftover_value_type,
        double value_1,
        double value_2)
{
    return (leftover_value_type == LeftoverValueType::SquaredArea)?
        value_1 > value_2:
        strictly_greater_profit(value_1, value_2);
}

/**
 * Get a length from a leftover value in two dimensions, for the comparisons
 * with an absolute tolerance on lengths: the fourth root of a squared area,
 * the cube root of a value of type 'LeftoverValueType::Depth'.
 */
inline double leftover_value_length_2d(
        LeftoverValueType leftover_value_type,
        double value)
{
    return (leftover_value_type == LeftoverValueType::SquaredArea)?
        std::sqrt(std::sqrt(value)):
        std::cbrt(value);
}

}

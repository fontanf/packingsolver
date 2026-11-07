#include "rectangleguillotine/normal_positions.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

NormalPositions packingsolver::rectangleguillotine::compute_normal_positions(
        const std::vector<Length>& dimensions,
        Length length,
        Length cut_thickness)
{
    // A combination of 'n' items, followed by their 'n' cuts, has a length
    // which is a sum of 'dimension + cut_thickness'; its normal position is
    // this length minus the last cut.
    std::vector<bool> reached(length + cut_thickness + 1, false);
    reached[0] = true;
    for (Length x = 0; x <= length + cut_thickness; ++x) {
        if (!reached[x])
            continue;
        for (Length dimension: dimensions) {
            Length next = x + dimension + cut_thickness;
            if (next <= length + cut_thickness)
                reached[next] = true;
        }
    }
    NormalPositions normal_positions;
    normal_positions.positions.push_back(0);
    for (Length x = cut_thickness + 1; x <= length + cut_thickness; ++x)
        if (reached[x])
            normal_positions.positions.push_back(x - cut_thickness);
    normal_positions.floor.resize(length + 1);
    Counter index = 0;
    for (Length x = 0; x <= length; ++x) {
        while (index + 1 < (Counter)normal_positions.positions.size()
                && normal_positions.positions[index + 1] <= x) {
            ++index;
        }
        normal_positions.floor[x] = index;
    }
    return normal_positions;
}

NormalPositions packingsolver::rectangleguillotine::compute_raster_points(
        const NormalPositions& normal_positions,
        Length length,
        Length cut_thickness)
{
    std::vector<bool> is_raster_point(length + 1, false);
    is_raster_point[0] = true;
    is_raster_point[normal_positions.positions[normal_positions.floor[length]]] = true;
    for (Length position: normal_positions.positions) {
        if (position == 0)
            continue;
        Length rest = length - position - cut_thickness;
        if (rest < 0)
            break;
        is_raster_point[normal_positions.positions[normal_positions.floor[rest]]] = true;
    }
    NormalPositions raster_points;
    for (Length x = 0; x <= length; ++x)
        if (is_raster_point[x])
            raster_points.positions.push_back(x);
    raster_points.floor.resize(length + 1);
    Counter index = 0;
    for (Length x = 0; x <= length; ++x) {
        while (index + 1 < (Counter)raster_points.positions.size()
                && raster_points.positions[index + 1] <= x) {
            ++index;
        }
        raster_points.floor[x] = index;
    }
    return raster_points;
}

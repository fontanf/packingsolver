#include "irregular/leftover.hpp"

#include "algorithms/leftover.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * Get the dimensions of the rectangle used by the items of a bin, from the
 * corner (or the side) of the leftover mode to the opposite corner (or side)
 * of the bounding box of the items.
 */
std::array<double, 2> used_dimensions(
        LeftoverMode leftover_mode,
        const AxisAlignedBoundingBox& bin_aabb,
        const AxisAlignedBoundingBox& items_aabb)
{
    LengthDbl width = bin_aabb.x_max - bin_aabb.x_min;
    LengthDbl height = bin_aabb.y_max - bin_aabb.y_min;
    LengthDbl from_left = (std::max)(0.0, (std::min)(width, items_aabb.x_max - bin_aabb.x_min));
    LengthDbl from_right = (std::max)(0.0, (std::min)(width, bin_aabb.x_max - items_aabb.x_min));
    LengthDbl from_bottom = (std::max)(0.0, (std::min)(height, items_aabb.y_max - bin_aabb.y_min));
    LengthDbl from_top = (std::max)(0.0, (std::min)(height, bin_aabb.y_max - items_aabb.y_min));
    switch (leftover_mode_base(leftover_mode)) {
    case LeftoverMode::BottomLeft: {
        return {from_left, from_bottom};
    } case LeftoverMode::BottomRight: {
        return {from_right, from_bottom};
    } case LeftoverMode::TopLeft: {
        return {from_left, from_top};
    } case LeftoverMode::TopRight: {
        return {from_right, from_top};
    } case LeftoverMode::Left: {
        return {from_left, height};
    } case LeftoverMode::Right: {
        return {from_right, height};
    } case LeftoverMode::Bottom: {
        return {width, from_bottom};
    } case LeftoverMode::Top: {
        return {width, from_top};
    } default: {
        return {0, 0};
    }
    }
}

}

double packingsolver::irregular::leftover_value(
        LeftoverValueType leftover_value_type,
        LeftoverMode leftover_mode,
        const AxisAlignedBoundingBox& bin_aabb,
        const AxisAlignedBoundingBox& items_aabb)
{
    std::array<double, 2> bin = {
        bin_aabb.x_max - bin_aabb.x_min,
        bin_aabb.y_max - bin_aabb.y_min};
    std::array<double, 2> used = used_dimensions(leftover_mode, bin_aabb, items_aabb);
    return packingsolver::leftover_value(
            leftover_value_type,
            bin,
            used,
            leftover_mode_cut(leftover_mode));
}

AreaDbl packingsolver::irregular::leftover_area(
        LeftoverMode leftover_mode,
        const AxisAlignedBoundingBox& bin_aabb,
        const AxisAlignedBoundingBox& items_aabb)
{
    std::array<double, 2> used = used_dimensions(leftover_mode, bin_aabb, items_aabb);
    return (bin_aabb.x_max - bin_aabb.x_min) * (bin_aabb.y_max - bin_aabb.y_min)
        - used[0] * used[1];
}

double packingsolver::irregular::leftover_value(
        LeftoverValueType leftover_value_type,
        const AxisAlignedBoundingBox& bin_aabb)
{
    return packingsolver::leftover_value<2>(
            leftover_value_type,
            {bin_aabb.x_max - bin_aabb.x_min, bin_aabb.y_max - bin_aabb.y_min},
            {0, 0},
            false);
}

void Staircase::add_item(
        LeftoverMode leftover_mode,
        const AxisAlignedBoundingBox& bin_aabb,
        const AxisAlignedBoundingBox& item_aabb)
{
    LengthDbl from_left = item_aabb.x_max - bin_aabb.x_min;
    LengthDbl from_right = bin_aabb.x_max - item_aabb.x_min;
    LengthDbl from_bottom = item_aabb.y_max - bin_aabb.y_min;
    LengthDbl from_top = bin_aabb.y_max - item_aabb.y_min;
    LengthDbl width = bin_aabb.x_max - bin_aabb.x_min;
    LengthDbl height = bin_aabb.y_max - bin_aabb.y_min;
    std::pair<LengthDbl, LengthDbl> extent;
    switch (leftover_mode_base(leftover_mode)) {
    case LeftoverMode::BottomLeft: {
        extent = {from_left, from_bottom};
        break;
    } case LeftoverMode::BottomRight: {
        extent = {from_right, from_bottom};
        break;
    } case LeftoverMode::TopLeft: {
        extent = {from_left, from_top};
        break;
    } case LeftoverMode::TopRight: {
        extent = {from_right, from_top};
        break;
    } case LeftoverMode::Left: {
        extent = {from_left, height};
        break;
    } case LeftoverMode::Right: {
        extent = {from_right, height};
        break;
    } case LeftoverMode::Bottom: {
        extent = {width, from_bottom};
        break;
    } case LeftoverMode::Top: {
        extent = {width, from_top};
        break;
    }
    }

    // Check if the new extent is dominated.
    for (const auto& e: extents_)
        if (e.first >= extent.first && e.second >= extent.second)
            return;

    // Remove the extents dominated by the new one and insert it.
    std::vector<std::pair<LengthDbl, LengthDbl>> extents;
    extents.reserve(extents_.size() + 1);
    bool inserted = false;
    for (const auto& e: extents_) {
        if (e.first <= extent.first && e.second <= extent.second)
            continue;
        if (!inserted && e.first < extent.first) {
            extents.push_back(extent);
            inserted = true;
        }
        extents.push_back(e);
    }
    if (!inserted)
        extents.push_back(extent);
    extents_.swap(extents);
}

AreaDbl Staircase::dominated_area() const
{
    AreaDbl area = 0;
    for (ItemPos pos = 0; pos < (ItemPos)extents_.size(); ++pos) {
        LengthDbl next_extent = (pos + 1 < (ItemPos)extents_.size())?
            extents_[pos + 1].first: 0;
        area += (extents_[pos].first - next_extent) * extents_[pos].second;
    }
    return area;
}

AreaDbl Staircase::leftover_value(
        const AxisAlignedBoundingBox& bin_aabb) const
{
    return (bin_aabb.x_max - bin_aabb.x_min)
        * (bin_aabb.y_max - bin_aabb.y_min)
        - dominated_area();
}

double Staircase::value(
        LeftoverValueType leftover_value_type,
        const AxisAlignedBoundingBox& bin_aabb) const
{
    std::vector<std::array<double, 2>> extents;
    extents.reserve(extents_.size());
    for (const auto& extent: extents_)
        extents.push_back({extent.first, extent.second});
    return packingsolver::staircase_leftover_value<2>(
            leftover_value_type,
            {bin_aabb.x_max - bin_aabb.x_min, bin_aabb.y_max - bin_aabb.y_min},
            extents);
}

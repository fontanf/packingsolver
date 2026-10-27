#include "irregular/leftover.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

AreaDbl packingsolver::irregular::leftover_value(
        LeftoverMode leftover_mode,
        const AxisAlignedBoundingBox& bin_aabb,
        const AxisAlignedBoundingBox& items_aabb)
{
    LengthDbl width = bin_aabb.x_max - bin_aabb.x_min;
    LengthDbl height = bin_aabb.y_max - bin_aabb.y_min;
    switch (leftover_mode) {
    case LeftoverMode::BottomLeft: {
        return width * height
            - (items_aabb.x_max - bin_aabb.x_min) * (items_aabb.y_max - bin_aabb.y_min);
    } case LeftoverMode::BottomRight: {
        return width * height
            - (bin_aabb.x_max - items_aabb.x_min) * (items_aabb.y_max - bin_aabb.y_min);
    } case LeftoverMode::TopLeft: {
        return width * height
            - (items_aabb.x_max - bin_aabb.x_min) * (bin_aabb.y_max - items_aabb.y_min);
    } case LeftoverMode::TopRight: {
        return width * height
            - (bin_aabb.x_max - items_aabb.x_min) * (bin_aabb.y_max - items_aabb.y_min);
    } case LeftoverMode::Left: {
        return width * height
            - (items_aabb.x_max - bin_aabb.x_min) * height;
    } case LeftoverMode::Right: {
        return width * height
            - (bin_aabb.x_max - items_aabb.x_min) * height;
    } case LeftoverMode::Bottom: {
        return width * height
            - width * (items_aabb.y_max - bin_aabb.y_min);
    } case LeftoverMode::Top: {
        return width * height
            - width * (bin_aabb.y_max - items_aabb.y_min);
    }
    }
    return 0;
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
    switch (leftover_mode) {
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

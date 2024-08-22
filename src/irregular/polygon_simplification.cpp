#include "irregular/polygon_simplification.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{


std::vector<TrapezoidSetId> compute_trapezoids_below(
        const std::vector<GeneralizedTrapezoid>& trapezoids)
{
    std::vector<TrapezoidSetId> trapezoid_below(trapezoids.size(), -1);
    std::vector<TrapezoidSetId> trapezoid_above(trapezoids.size(), -1);

    std::vector<TrapezoidSetId> sorted_trapezoids(trapezoids.size(), -1);
    std::iota(sorted_trapezoids.begin(), sorted_trapezoids.end(), 0);
    std::sort(
            sorted_trapezoids.begin(),
            sorted_trapezoids.end(),
            [&trapezoids](
                TrapezoidPos trapezoid_pos_1,
                TrapezoidPos trapezoid_pos_2)
            {
                const GeneralizedTrapezoid& trapezoid_1 = trapezoids[trapezoid_pos_1];
                const GeneralizedTrapezoid& trapezoid_2 = trapezoids[trapezoid_pos_2];
                return trapezoid_1.y_top() > trapezoid_2.y_top();
            });
    for (TrapezoidSetId trapezoid_pos_1 = 0;
            trapezoid_pos_1 < trapezoids.size();
            ++trapezoid_pos_1) {
        //std::cout << "trapezoid_pos_1 " << trapezoid_pos_1 << std::endl;
        const GeneralizedTrapezoid& trapezoid_1 = trapezoids[sorted_trapezoids[trapezoid_pos_1]];
        //std::cout << "trapezoid_1 " << trapezoid_1 << std::endl;

        for (TrapezoidSetId trapezoid_pos_2 = trapezoid_pos_1 + 1;
                trapezoid_pos_2 < (TrapezoidPos)trapezoids.size();
                ++trapezoid_pos_2) {
            //std::cout << "trapezoid_pos_2 " << trapezoid_pos_2 << std::endl;
            const GeneralizedTrapezoid& trapezoid_2 = trapezoids[sorted_trapezoids[trapezoid_pos_2]];
            //std::cout << "trapezoid_2 " << trapezoid_2 << std::endl;

            if (trapezoid_above[sorted_trapezoids[trapezoid_pos_2]] != -1)
                continue;

            if (striclty_lesser(trapezoid_2.y_top(), trapezoid_1.y_bottom()))
                break;

            bool ok_1 = (!striclty_greater(trapezoid_2.x_top_left(), trapezoid_1.x_bottom_left())
                    && !striclty_lesser(trapezoid_2.x_top_right(), trapezoid_1.x_bottom_right()));
            bool ok_2 = (!striclty_lesser(trapezoid_2.x_top_left(), trapezoid_1.x_bottom_left())
                    && !striclty_greater(trapezoid_2.x_top_right(), trapezoid_1.x_bottom_right()));
            if (equal(trapezoid_2.y_top(), trapezoid_1.y_bottom())
                    && (ok_1 || ok_2)) {
                trapezoid_below[sorted_trapezoids[trapezoid_pos_1]]
                    = sorted_trapezoids[trapezoid_pos_2];
                trapezoid_above[sorted_trapezoids[trapezoid_pos_2]]
                    = sorted_trapezoids[trapezoid_pos_1];
                //std::cout << "above "
                //    << sorted_trapezoids[trapezoid_pos_1]
                //    << " " << trapezoid_1 << std::endl;
                //std::cout << "below "
                //    << sorted_trapezoids[trapezoid_pos_2]
                //    << " " << trapezoid_2 << std::endl;
                break;
            }
        }

    }
    return trapezoid_below;
}

GeneralizedTrapezoid merge(
        const GeneralizedTrapezoid& trapezoid_above,
        const GeneralizedTrapezoid& trapezoid_below)
{
    // Check input.
    if (!equal(trapezoid_above.y_bottom(), trapezoid_below.y_top())) {
        std::stringstream ss;
        ss << "merge."
            << " trapezoid_above: " << trapezoid_above
            << "; trapezoid_below: " << trapezoid_below
            << "." << std::endl;
        throw std::invalid_argument(ss.str());
    }

    //std::cout << "merge" << std::endl;
    //std::cout << "trapezoid_above " << trapezoid_above << std::endl;
    //std::cout << "trapezoid_below " << trapezoid_below << std::endl;

    LengthDbl h = trapezoid_above.y_top() - trapezoid_below.y_bottom();

    // Compute left side.
    LengthDbl xl_max = -std::numeric_limits<LengthDbl>::infinity();
    xl_max = std::max(xl_max, trapezoid_above.x_top_left());
    xl_max = std::max(xl_max, trapezoid_above.x_bottom_left());
    xl_max = std::max(xl_max, trapezoid_below.x_top_left());
    xl_max = std::max(xl_max, trapezoid_below.x_bottom_left());
    LengthDbl area_left_best = std::numeric_limits<AreaDbl>::infinity();
    LengthDbl xbl = -std::numeric_limits<LengthDbl>::infinity();
    LengthDbl xtl = -std::numeric_limits<LengthDbl>::infinity();
    // Try extending left of trapezoid above.
    {
        LengthDbl xtl_cur = trapezoid_above.x_top_left();
        LengthDbl xbl_cur = trapezoid_above.x_left(trapezoid_below.y_bottom());
        if (!striclty_greater(xbl_cur, trapezoid_below.x_bottom_left())
                && !striclty_greater(trapezoid_above.x_bottom_left(), trapezoid_below.x_top_left())) {
            AreaDbl area_cur = h * ((std::max)(xbl_cur, xtl_cur) - (std::min)(xbl_cur, xtl_cur)) / 2
                    + h * (xl_max - (std::min)(xbl_cur, xtl_cur));
            //std::cout << "area_cur " << area_cur << std::endl;
            //std::cout << "area_left_best " << area_left_best << std::endl;
            if (area_left_best > area_cur) {
                xtl = xtl_cur;
                xbl = xbl_cur;
                area_left_best = area_cur;
            }
        }
    }
    // Try extending right of trapezoid above.
    {
        LengthDbl xtl_cur = trapezoid_below.x_left(trapezoid_above.y_top());
        LengthDbl xbl_cur = trapezoid_below.x_bottom_left();
        if (!striclty_greater(xtl_cur, trapezoid_above.x_top_left())
                && !striclty_greater(trapezoid_below.x_top_left(), trapezoid_above.x_bottom_left())) {
            AreaDbl area_cur = h * ((std::max)(xbl_cur, xtl_cur) - (std::min)(xbl_cur, xtl_cur)) / 2
                    + h * (xl_max - (std::min)(xbl_cur, xtl_cur));
            //std::cout << "area_cur " << area_cur << std::endl;
            //std::cout << "area_left_best " << area_left_best << std::endl;
            if (area_left_best > area_cur) {
                xtl = xtl_cur;
                xbl = xbl_cur;
                area_left_best = area_cur;
            }
        }
    }
    // Try linking top left of trapezoid above with bottom left of trapezoid
    // below.
    {
        LengthDbl xtl_cur = trapezoid_above.x_top_left();
        LengthDbl xbl_cur = trapezoid_below.x_bottom_left();
        GeneralizedTrapezoid trapezoid_tmp(
                trapezoid_below.y_bottom(),
                trapezoid_above.y_top(),
                xbl_cur,
                xl_max,
                xtl_cur,
                xl_max);
        //std::cout << "trapezoid_tmp.x_left(trapezoid_below.y_top()) " << trapezoid_tmp.x_left(trapezoid_below.y_top()) << std::endl;
        //std::cout << "trapezoid_below.x_top_left() " << trapezoid_below.x_top_left() << std::endl;
        //std::cout << "trapezoid_above.x_bottom_left() " << trapezoid_above.x_bottom_left() << std::endl;
        if (!striclty_greater(trapezoid_tmp.x_left(trapezoid_below.y_top()), trapezoid_below.x_top_left())
                && !striclty_greater(trapezoid_tmp.x_left(trapezoid_below.y_top()), trapezoid_above.x_bottom_left())) {
            AreaDbl area_cur = h * ((std::max)(xbl_cur, xtl_cur) - (std::min)(xbl_cur, xtl_cur)) / 2
                    + h * (xl_max - (std::min)(xbl_cur, xtl_cur));
            //std::cout << "area_cur " << area_cur << std::endl;
            //std::cout << "area_left_best " << area_left_best << std::endl;
            if (area_left_best > area_cur) {
                //std::cout << "update" << std::endl;
                xtl = xtl_cur;
                xbl = xbl_cur;
                area_left_best = area_cur;
            }
        }
    }

    // Compute right side.
    LengthDbl xr_min = std::numeric_limits<LengthDbl>::infinity();
    xr_min = std::min(xr_min, trapezoid_above.x_top_right());
    xr_min = std::min(xr_min, trapezoid_above.x_bottom_right());
    xr_min = std::min(xr_min, trapezoid_below.x_top_right());
    xr_min = std::min(xr_min, trapezoid_below.x_bottom_right());
    LengthDbl area_right_best = std::numeric_limits<AreaDbl>::infinity();
    LengthDbl xbr = std::numeric_limits<LengthDbl>::infinity();
    LengthDbl xtr = std::numeric_limits<LengthDbl>::infinity();
    // Try extending right of trapezoid above.
    {
        LengthDbl xtr_cur = trapezoid_above.x_top_right();
        LengthDbl xbr_cur = trapezoid_above.x_right(trapezoid_below.y_bottom());
        if (!striclty_lesser(xbr_cur, trapezoid_below.x_bottom_right())
                && !striclty_lesser(trapezoid_above.x_bottom_right(), trapezoid_below.x_top_right())) {
            AreaDbl area_cur = h * ((std::max)(xbr_cur, xtr_cur) - (std::min)(xbr_cur, xtr_cur)) / 2
                    + h * ((std::min)(xbr_cur, xtr_cur) - xr_min);
            //std::cout << "area_cur " << area_cur << std::endl;
            //std::cout << "area_right_best " << area_right_best << std::endl;
            if (area_right_best > area_cur) {
                xtr = xtr_cur;
                xbr = xbr_cur;
                area_right_best = area_cur;
            }
        }
    }
    // Try extending right of trapezoid above.
    {
        LengthDbl xtr_cur = trapezoid_below.x_right(trapezoid_above.y_top());
        LengthDbl xbr_cur = trapezoid_below.x_bottom_right();
        //std::cout << "xtr_cur " << xtr_cur << std::endl;
        //std::cout << "trapezoid_above.x_top_right() " << trapezoid_above.x_top_right() << std::endl;
        //std::cout << "trapezoid_below.x_top_right() " << trapezoid_below.x_top_right() << std::endl;
        //std::cout << "trapezoid_above.x_bottom_right() " << trapezoid_above.x_bottom_right() << std::endl;
        if (!striclty_lesser(xtr_cur, trapezoid_above.x_top_right())
                && !striclty_lesser(trapezoid_below.x_top_right(), trapezoid_above.x_bottom_right())) {
            AreaDbl area_cur = h * ((std::max)(xbr_cur, xtr_cur) - (std::min)(xbr_cur, xtr_cur)) / 2
                    + h * ((std::min)(xbr_cur, xtr_cur) - xr_min);
            //std::cout << "area_cur " << area_cur << std::endl;
            //std::cout << "area_right_best " << area_right_best << std::endl;
            if (area_right_best > area_cur) {
                xtr = xtr_cur;
                xbr = xbr_cur;
                area_right_best = area_cur;
            }
        }
    }
    // Try linking top right of trapezoid above with bottom right of trapezoid
    // below.
    {
        LengthDbl xtr_cur = trapezoid_above.x_top_right();
        LengthDbl xbr_cur = trapezoid_below.x_bottom_right();
        GeneralizedTrapezoid trapezoid_tmp(
                trapezoid_below.y_bottom(),
                trapezoid_above.y_top(),
                xr_min,
                xbr_cur,
                xr_min,
                xtr_cur);
        //std::cout << "trapezoid_tmp.x_right(trapezoid_below.y_top()) " << trapezoid_tmp.x_right(trapezoid_below.y_top()) << std::endl;
        //std::cout << "trapezoid_below.x_top_right() " << trapezoid_below.x_top_right() << std::endl;
        //std::cout << "trapezoid_above.x_bottom_right() " << trapezoid_above.x_bottom_right() << std::endl;
        if (!striclty_lesser(trapezoid_tmp.x_right(trapezoid_below.y_top()), trapezoid_below.x_top_right())
                && !striclty_lesser(trapezoid_tmp.x_right(trapezoid_below.y_top()), trapezoid_above.x_bottom_right())) {
            AreaDbl area_cur = h * ((std::max)(xbr_cur, xtr_cur) - (std::min)(xbr_cur, xtr_cur)) / 2
                    + h * ((std::min)(xbr_cur, xtr_cur) - xr_min);
            //std::cout << "area_cur " << area_cur << std::endl;
            //std::cout << "area_right_best " << area_right_best << std::endl;
            if (area_right_best > area_cur) {
                xtr = xtr_cur;
                xbr = xbr_cur;
                area_right_best = area_cur;
            }
        }
    }

    GeneralizedTrapezoid trapezoid_merged(
            trapezoid_below.y_bottom(),
            trapezoid_above.y_top(),
            xbl,
            xbr,
            xtl,
            xtr);
    //std::cout << "trapezoid_merged " << trapezoid_merged << std::endl;
    return trapezoid_merged;
}

struct PolygonSimplificationElement
{
    TrapezoidSetId trapezoid_set_id;
    ItemShapePos item_shape_pos;
    TrapezoidPos trapezoid_above_pos;
    TrapezoidPos trapezoid_below_pos;
    double merge_cost;
};

}

std::vector<TrapezoidSet> packingsolver::irregular::polygon_simplification(
        const Instance& instance,
        const std::vector<TrapezoidSet>& trapezoid_sets)
{
    //std::cout << "polygon_simplification..." << std::endl;

    // Compute trapezoid_below.
    std::vector<TrapezoidSet> trapezoid_sets_tmp = trapezoid_sets;
    std::vector<std::vector<std::vector<TrapezoidSetId>>> trapezoids_below(trapezoid_sets.size());
    std::vector<PolygonSimplificationElement> merge_candidates_tmp;
    //std::cout << "trapezoid_sets.size() " << trapezoid_sets.size() << std::endl;
    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)trapezoid_sets.size();
            ++trapezoid_set_id) {
        //std::cout << "trapezoid_set_id " << trapezoid_set_id << std::endl;
        const TrapezoidSet& trapezoid_set = trapezoid_sets[trapezoid_set_id];
        const ItemType& item_type = instance.item_type(trapezoid_set.item_type_id);
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
                ++item_shape_pos) {
            const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
            trapezoids_below[trapezoid_set_id].push_back(
                    compute_trapezoids_below(item_shape_trapezoids));
            for (TrapezoidPos item_shape_trapezoid_pos = 0;
                    item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                    ++item_shape_trapezoid_pos) {
                const GeneralizedTrapezoid& trapezoid_above = item_shape_trapezoids[item_shape_trapezoid_pos];
                TrapezoidSetId trapezoid_below_pos = trapezoids_below[trapezoid_set_id][item_shape_pos][item_shape_trapezoid_pos];
                if (trapezoid_below_pos != -1) {
                    const GeneralizedTrapezoid& trapezoid_below = trapezoid_sets_tmp[trapezoid_set_id].shapes[item_shape_pos][trapezoid_below_pos];
                    PolygonSimplificationElement candidate;
                    candidate.trapezoid_set_id = trapezoid_set_id;
                    candidate.item_shape_pos = item_shape_pos;
                    candidate.trapezoid_above_pos = item_shape_trapezoid_pos;
                    candidate.trapezoid_below_pos = trapezoid_below_pos;
                    GeneralizedTrapezoid trapezoid_merge = merge(trapezoid_above, trapezoid_below);
                    double c_old = trapezoid_above.area() + trapezoid_below.area();
                    double c_new = trapezoid_merge.area();
                    candidate.merge_cost = (c_new - c_old) * item_type.copies;
                    //if (striclty_lesser(c_new, c_old)
                    //        && !equal(trapezoid_above.y_top(), trapezoid_above.y_bottom())
                    //        && !equal(trapezoid_below.y_top(), trapezoid_below.y_bottom())) {
                    //    //std::cout << "above " << trapezoid_above << std::endl;
                    //    //std::cout << "below " << trapezoid_below << std::endl;
                    //    //std::cout << "merge " << trapezoid_merge << std::endl;
                    //    //std::cout << "cost " << candidate.merge_cost << std::endl;
                    //    //std::cout << "c_old " << c_old << std::endl;
                    //    //std::cout << "c_new " << c_new << std::endl;
                    //    throw std::logic_error(
                    //            "polygon_simplification.");
                    //}
                    merge_candidates_tmp.push_back(candidate);
                }
            }
        }
    }
    //std::cout << "merge_candidates_tmp.size() " << merge_candidates_tmp.size() << std::endl;

    // Create candidate set.
    auto cmp = [](
            const PolygonSimplificationElement& candidate_1,
            const PolygonSimplificationElement& candidate_2)
    {
        return candidate_1.merge_cost < candidate_2.merge_cost;
    };
    std::multiset<PolygonSimplificationElement, decltype(cmp)> merge_candidates(
            merge_candidates_tmp.begin(),
            merge_candidates_tmp.end(),
            cmp);
    //std::cout << "merge_candidates.size() " << merge_candidates.size() << std::endl;

    // Current merge cost for each trapezoid set.
    std::vector<double> trapezoid_set_merge_cost(trapezoid_sets.size(), 0.0);

    // Current merge cost for each item type.
    std::vector<double> item_type_merge_cost(instance.number_of_item_types(), 0.0);

    // Current total merge cost.
    double total_merge_cost = 0.0;

    // We allow a merge cost of 1% of the bin or item area.
    double maximum_merge_cost = (instance.objective() == Objective::Knapsack)?
        instance.bin_area():
        instance.item_area();
    maximum_merge_cost *= 0.01;

    // Compute the total number of trapezoids.
    TrapezoidPos total_number_of_trapezoids = 0;
    for (const TrapezoidSet& trapezoid_set: trapezoid_sets) {
        const ItemType& item_type = instance.item_type(trapezoid_set.item_type_id);
        for (const std::vector<GeneralizedTrapezoid>& item_shape_trapezoids: trapezoid_set.shapes) {
            total_number_of_trapezoids += item_shape_trapezoids.size()
                * item_type.copies;
        }
    }
    //std::cout << "total_number_of_trapezoids " << total_number_of_trapezoids << std::endl;

    // Initialize is_trapezoid_removed.
    std::vector<std::vector<std::vector<uint8_t>>> is_trapezoid_removed(trapezoid_sets.size());
    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)trapezoid_sets.size();
            ++trapezoid_set_id) {
        const TrapezoidSet& trapezoid_set = trapezoid_sets[trapezoid_set_id];
        is_trapezoid_removed[trapezoid_set_id]
            = std::vector<std::vector<uint8_t>>(trapezoid_set.shapes.size());
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
                ++item_shape_pos) {
            const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
            is_trapezoid_removed[trapezoid_set_id][item_shape_pos]
                = std::vector<uint8_t>(item_shape_trapezoids.size(), 0);
        }
    }

    while (!merge_candidates.empty()) {

        // Check the total number of trapezoids.
        if (total_number_of_trapezoids
                <= 16 * instance.number_of_items()) {
            break;
        }

        // Get the best candidate from the candidate set.
        PolygonSimplificationElement candidate = *merge_candidates.begin();
        //std::cout << std::endl;
        //std::cout << "candidate"
        //    << " trapezoid_set_id " << candidate.trapezoid_set_id
        //    << " item_shape_pos " << candidate.item_shape_pos
        //    << " above " << candidate.trapezoid_above_pos
        //    << " below " << candidate.trapezoid_below_pos
        //    << " cost " << candidate.merge_cost
        //    << std::endl;
        merge_candidates.erase(merge_candidates.begin());

        if (is_trapezoid_removed[candidate.trapezoid_set_id][candidate.item_shape_pos][candidate.trapezoid_above_pos])
            continue;
        if (is_trapezoid_removed[candidate.trapezoid_set_id][candidate.item_shape_pos][candidate.trapezoid_below_pos])
            continue;

        const TrapezoidSet& trapezoid_set = trapezoid_sets[candidate.trapezoid_set_id];
        ItemTypeId item_type_id = trapezoid_set.item_type_id;
        const ItemType& item_type = instance.item_type(item_type_id);

        const GeneralizedTrapezoid& trapezoid_above = trapezoid_sets_tmp[candidate.trapezoid_set_id].shapes[candidate.item_shape_pos][candidate.trapezoid_above_pos];
        const GeneralizedTrapezoid& trapezoid_below = trapezoid_sets_tmp[candidate.trapezoid_set_id].shapes[candidate.item_shape_pos][candidate.trapezoid_below_pos];
        GeneralizedTrapezoid trapezoid_merge = merge(trapezoid_above, trapezoid_below);
        double merge_cost = trapezoid_merge.area()
            - trapezoid_above.area()
            - trapezoid_below.area();
        merge_cost *= item_type.copies;
        if (candidate.merge_cost != merge_cost) {
            candidate.merge_cost = merge_cost;
            merge_candidates.insert(candidate);
            continue;
        }

        // Compute potential_total_merge_cost.
        double potential_trapezoid_set_merge_cost = trapezoid_set_merge_cost[candidate.trapezoid_set_id]
            + candidate.merge_cost;
        double potential_total_merge_cost = 0.0;
        if (item_type_merge_cost[item_type_id] < potential_trapezoid_set_merge_cost) {
            potential_total_merge_cost = total_merge_cost
                + potential_trapezoid_set_merge_cost
                - item_type_merge_cost[item_type_id];
        }

        // Merge.
        if (potential_total_merge_cost <= maximum_merge_cost) {
            //std::cout << "merge"
            //    << " trapezoid_set_id " << candidate.trapezoid_set_id
            //    << " item_shape_pos " << candidate.item_shape_pos
            //    << " above " << candidate.trapezoid_above_pos
            //    << " below " << candidate.trapezoid_below_pos
            //    << std::endl;
            //std::cout << "above " << trapezoid_above << std::endl;
            //std::cout << "below " << trapezoid_below << std::endl;
            //std::cout << "merge " << trapezoid_merge << std::endl;

            // Update trapezoid_set_tmp.
            trapezoid_sets_tmp[candidate.trapezoid_set_id].shapes[candidate.item_shape_pos][candidate.trapezoid_above_pos] = trapezoid_merge;
            //std::cout << "merge " << trapezoid_sets_tmp[candidate.trapezoid_set_id].shapes[candidate.item_shape_pos][candidate.trapezoid_above_pos] << std::endl;

            // Update is_trapezoid_removed.
            is_trapezoid_removed[candidate.trapezoid_set_id][candidate.item_shape_pos][candidate.trapezoid_below_pos] = 1;

            // Update the total merge cost.
            total_merge_cost += candidate.merge_cost;

            // Update the total number of trapezoids.
            total_number_of_trapezoids -= item_type.copies;

            // Create new candidate.
            TrapezoidPos trapezoid_below_new_pos = trapezoids_below[candidate.trapezoid_set_id][candidate.item_shape_pos][candidate.trapezoid_below_pos];
            //std::cout << "trapezoid_below_new_pos " << trapezoid_below_new_pos << std::endl;
            trapezoids_below[candidate.trapezoid_set_id][candidate.item_shape_pos][candidate.trapezoid_above_pos] = trapezoid_below_new_pos;
            if (trapezoid_below_new_pos != -1) {
                PolygonSimplificationElement candidate_new;
                candidate_new.trapezoid_set_id = candidate.trapezoid_set_id;
                candidate_new.item_shape_pos = candidate.item_shape_pos;
                candidate_new.trapezoid_above_pos = candidate.trapezoid_above_pos;
                candidate_new.trapezoid_below_pos = trapezoid_below_new_pos;
                //std::cout
                //    << " trapezoid_set_id " << candidate_new.trapezoid_set_id
                //    << " item_shape_pos " << candidate_new.item_shape_pos
                //    << " trapezoid_above_pos " << candidate_new.trapezoid_above_pos
                //    << " trapezoid_below_pos " << candidate_new.trapezoid_below_pos
                //    << std::endl;
                candidate_new.merge_cost = merge(
                        trapezoid_sets_tmp[candidate_new.trapezoid_set_id].shapes[candidate_new.item_shape_pos][candidate_new.trapezoid_above_pos],
                        trapezoid_sets_tmp[candidate_new.trapezoid_set_id].shapes[candidate_new.item_shape_pos][candidate_new.trapezoid_below_pos]).area()
                    - trapezoid_sets_tmp[candidate_new.trapezoid_set_id].shapes[candidate_new.item_shape_pos][candidate_new.trapezoid_above_pos].area()
                    - trapezoid_sets_tmp[candidate_new.trapezoid_set_id].shapes[candidate_new.item_shape_pos][candidate_new.trapezoid_below_pos].area();
                candidate.merge_cost *= item_type.copies;
                merge_candidates.insert(candidate_new);
            }
        }
    }
    //std::cout << "total_number_of_trapezoids " << total_number_of_trapezoids << " / " << 16 * instance.number_of_items() << std::endl;
    //std::cout << "total_merge_cost " << total_merge_cost << " / " << maximum_merge_cost << std::endl;

    // Build trapezoid_sets_new from trapezoid_sets_tmp and is_trapezoid_removed.
    std::vector<TrapezoidSet> trapezoid_sets_new(trapezoid_sets.size());
    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)trapezoid_sets.size();
            ++trapezoid_set_id) {
        const TrapezoidSet& trapezoid_set = trapezoid_sets_tmp[trapezoid_set_id];
        trapezoid_sets_new[trapezoid_set_id].shapes
            = std::vector<std::vector<GeneralizedTrapezoid>>(trapezoid_set.shapes.size());
        trapezoid_sets_new[trapezoid_set_id].item_type_id = trapezoid_set.item_type_id;
        trapezoid_sets_new[trapezoid_set_id].angle = trapezoid_set.angle;
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
                ++item_shape_pos) {
            const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
            for (TrapezoidPos item_shape_trapezoid_pos = 0;
                    item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                    ++item_shape_trapezoid_pos) {
                const auto& item_shape_trapezoid = item_shape_trapezoids[item_shape_trapezoid_pos];
                if (is_trapezoid_removed[trapezoid_set_id][item_shape_pos][item_shape_trapezoid_pos])
                    continue;
                trapezoid_sets_new[trapezoid_set_id].shapes[item_shape_pos].push_back(item_shape_trapezoid);
            }
        }
    }

    //std::cout << "polygon_simplification end" << std::endl;

    return trapezoid_sets_new;
}

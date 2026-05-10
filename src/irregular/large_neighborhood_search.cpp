#include "irregular/large_neighborhood_search.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"

#include "shape/shape.hpp"
#include "shape/intersection_tree.hpp"

#include <random>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

struct Rectangle
{
    LengthDbl x_min;
    LengthDbl x_max;
    LengthDbl y_min;
    LengthDbl y_max;
};

}

const LargeNeighborhoodSearchOutput packingsolver::irregular::large_neighborhood_search(
        const Instance& instance,
        const LargeNeighborhoodSearchParameters& parameters)
{
    LargeNeighborhoodSearchOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Compute an initial solution with a greedy tree search (queue size 1).
    OptimizeParameters initial_params;
    initial_params.verbosity_level = 0;
    initial_params.timer = parameters.timer;
    initial_params.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    initial_params.optimization_mode = OptimizationMode::NotAnytime;
    initial_params.use_tree_search = true;
    initial_params.not_anytime_tree_search_queue_size = 1;
    auto initial_output = optimize(instance, initial_params);
    Solution solution = initial_output.solution_pool.best();
    algorithm_formatter.update_solution(solution, "initial solution");

    if (solution.full()) {
        algorithm_formatter.end();
        return output;
    }

    // Initialize profits area-proportionally.
    std::vector<Profit> profits(instance.number_of_item_types());
    for (ItemTypeId id = 0; id < instance.number_of_item_types(); ++id)
        profits[id] = instance.item_type(id).area_orig;

    std::mt19937 rng(0);

    for (Counter iter = 0; !parameters.timer.needs_to_end(); ++iter) {
        if (algorithm_formatter.end_boolean())
            break;
        if (solution.full())
            break;
        if (solution.number_of_different_bins() == 0)
            break;

        // Select a bin uniformly at random.
        BinPos bin_pos = std::uniform_int_distribution<BinPos>(
                0, solution.number_of_different_bins() - 1)(rng);
        const SolutionBin& sbin = solution.bin(bin_pos);
        BinTypeId bin_type_id = sbin.bin_type_id;
        const BinType& bin_type = instance.bin_type(bin_type_id);
        const AxisAlignedBoundingBox& bin_aabb = bin_type.aabb;
        LengthDbl bin_w = bin_aabb.x_max - bin_aabb.x_min;
        LengthDbl bin_h = bin_aabb.y_max - bin_aabb.y_min;
        if (bin_w <= 0 || bin_h <= 0)
            continue;

        // Build IntersectionTree for the non-fixed items in the selected bin.
        // Item shapes are transformed to world coordinates (original space):
        // mirror → rotate → shift by bl_corner.
        std::vector<shape::ShapeWithHoles> tree_shapes;
        std::vector<ItemPos> shape_to_item; // tree shape index → index in sbin.items

        for (ItemPos ip = 0; ip < (ItemPos)sbin.items.size(); ++ip) {
            const SolutionItem& sol_item = sbin.items[ip];
            if (sol_item.is_fixed)
                continue;
            const ItemType& item_type = instance.item_type(sol_item.item_type_id);
            for (int sp = 0; sp < (int)item_type.shapes.size(); ++sp) {
                shape::ShapeWithHoles swh = item_type.shapes[sp].shape_orig;
                if (sol_item.mirror)
                    swh = swh.axial_symmetry_y_axis();
                swh = swh.rotate(sol_item.angle);
                swh.shift(sol_item.bl_corner.x, sol_item.bl_corner.y);
                tree_shapes.push_back(std::move(swh));
                shape_to_item.push_back(ip);
            }
        }

        // Pre-compute per-item AABBs as the union over all shapes.
        const LengthDbl INF = std::numeric_limits<LengthDbl>::infinity();
        std::vector<AxisAlignedBoundingBox> item_aabbs(
                sbin.items.size(), {INF, -INF, INF, -INF});
        for (int si = 0; si < (int)tree_shapes.size(); ++si) {
            ItemPos ip = shape_to_item[si];
            AxisAlignedBoundingBox aabb = tree_shapes[si].shape.compute_min_max();
            item_aabbs[ip].x_min = std::min(item_aabbs[ip].x_min, aabb.x_min);
            item_aabbs[ip].x_max = std::max(item_aabbs[ip].x_max, aabb.x_max);
            item_aabbs[ip].y_min = std::min(item_aabbs[ip].y_min, aabb.y_min);
            item_aabbs[ip].y_max = std::max(item_aabbs[ip].y_max, aabb.y_max);
        }

        shape::IntersectionTree tree(tree_shapes, {}, {});

        // Generate candidate rectangles and select the one with the highest waste.
        // Waste = rectangle area - area of non-fixed items whose AABB is fully inside.
        Rectangle best_rect = {};
        AreaDbl best_waste = -1;

        for (Counter c = 0; c < parameters.number_of_rectangle_candidates; ++c) {
            // Generate a rectangle satisfying the aspect ratio constraint.
            // Alternate between horizontal (wide) and vertical (tall) strips.
            bool horizontal = std::uniform_int_distribution<int>(0, 1)(rng) == 0;
            LengthDbl w, h;
            if (horizontal) {
                // w >= aspect_ratio * h: draw h in [0, bin_h / aspect_ratio], then w in [aspect_ratio * h, bin_w].
                LengthDbl max_h = bin_h / parameters.minimum_aspect_ratio;
                if (max_h <= 0)
                    continue;
                h = std::uniform_real_distribution<LengthDbl>(0, max_h)(rng);
                LengthDbl min_w = parameters.minimum_aspect_ratio * h;
                if (min_w >= bin_w)
                    continue;
                w = std::uniform_real_distribution<LengthDbl>(min_w, bin_w)(rng);
            } else {
                // h >= aspect_ratio * w
                LengthDbl max_w = bin_w / parameters.minimum_aspect_ratio;
                if (max_w <= 0)
                    continue;
                w = std::uniform_real_distribution<LengthDbl>(0, max_w)(rng);
                LengthDbl min_h = parameters.minimum_aspect_ratio * w;
                if (min_h >= bin_h)
                    continue;
                h = std::uniform_real_distribution<LengthDbl>(min_h, bin_h)(rng);
            }
            LengthDbl max_x = bin_w - w;
            LengthDbl max_y = bin_h - h;
            if (max_x < 0 || max_y < 0)
                continue;
            LengthDbl x0 = bin_aabb.x_min
                + std::uniform_real_distribution<LengthDbl>(0, max_x)(rng);
            LengthDbl y0 = bin_aabb.y_min
                + std::uniform_real_distribution<LengthDbl>(0, max_y)(rng);
            Rectangle rect = {x0, x0 + w, y0, y0 + h};

            // Find items intersecting the rectangle.
            shape::Shape rect_shape = shape::build_rectangle(
                    rect.x_min, rect.x_max, rect.y_min, rect.y_max);
            auto intersect_result = tree.intersect(rect_shape, false);

            // Mark intersecting shapes.
            std::vector<bool> shape_hit(tree_shapes.size(), false);
            for (shape::ShapePos si : intersect_result.shape_ids)
                shape_hit[si] = true;

            // An item is "fully inside" if its union AABB fits within the rectangle.
            std::vector<bool> item_fully_inside(sbin.items.size(), false);
            for (int si = 0; si < (int)tree_shapes.size(); ++si) {
                if (!shape_hit[si])
                    continue;
                ItemPos ip = shape_to_item[si];
                const AxisAlignedBoundingBox& aabb = item_aabbs[ip];
                if (aabb.x_min >= rect.x_min && aabb.x_max <= rect.x_max
                        && aabb.y_min >= rect.y_min && aabb.y_max <= rect.y_max) {
                    item_fully_inside[ip] = true;
                }
            }

            AreaDbl rect_area = w * h;
            AreaDbl inside_area = 0;
            for (ItemPos ip = 0; ip < (ItemPos)sbin.items.size(); ++ip) {
                if (!sbin.items[ip].is_fixed && item_fully_inside[ip])
                    inside_area += instance.item_type(sbin.items[ip].item_type_id).area_orig;
            }
            AreaDbl waste = rect_area - inside_area;
            if (waste > best_waste) {
                best_waste = waste;
                best_rect = rect;
            }
        }

        if (best_waste < 0)
            continue;

        // Classify items in the selected bin using the best rectangle.
        shape::Shape rect_shape = shape::build_rectangle(
                best_rect.x_min, best_rect.x_max, best_rect.y_min, best_rect.y_max);
        auto intersect_result = tree.intersect(rect_shape, false);

        std::vector<bool> shape_hit(tree_shapes.size(), false);
        for (shape::ShapePos si : intersect_result.shape_ids)
            shape_hit[si] = true;

        // item_is_free[ip]: item is fully inside the rectangle → freed for sub-problem.
        // Items not fully inside (partial overlap or outside) are fixed in sub-problem.
        std::vector<bool> item_is_free(sbin.items.size(), false);
        for (int si = 0; si < (int)tree_shapes.size(); ++si) {
            if (!shape_hit[si])
                continue;
            ItemPos ip = shape_to_item[si];
            const AxisAlignedBoundingBox& aabb = item_aabbs[ip];
            if (aabb.x_min >= best_rect.x_min && aabb.x_max <= best_rect.x_max
                    && aabb.y_min >= best_rect.y_min && aabb.y_max <= best_rect.y_max) {
                item_is_free[ip] = true;
            }
        }

        // Count non-fixed placed copies across all bins.
        std::vector<ItemPos> placed_all(instance.number_of_item_types(), 0);
        for (BinPos bp = 0; bp < solution.number_of_different_bins(); ++bp) {
            for (const SolutionItem& item : solution.bin(bp).items) {
                if (!item.is_fixed)
                    placed_all[item.item_type_id]++;
            }
        }

        // Count copies freed from the rectangle (fully inside it) in the selected bin.
        std::vector<ItemPos> free_in_bin(instance.number_of_item_types(), 0);
        for (ItemPos ip = 0; ip < (ItemPos)sbin.items.size(); ++ip) {
            if (!sbin.items[ip].is_fixed && item_is_free[ip])
                free_in_bin[sbin.items[ip].item_type_id]++;
        }

        // free_copies[id] = copies available to the sub-problem Knapsack:
        // = (total copies) - (placed in other bins + fixed in selected bin)
        // = total - placed_all + free_in_bin
        std::vector<ItemPos> free_copies(instance.number_of_item_types());
        for (ItemTypeId id = 0; id < instance.number_of_item_types(); ++id)
            free_copies[id] = instance.item_type(id).copies - placed_all[id] + free_in_bin[id];

        // Build sub-instance (single bin, Knapsack objective).
        InstanceBuilder sub_builder;
        sub_builder.set_objective(Objective::Knapsack);
        sub_builder.set_parameters(instance.parameters());

        // The sub-instance has one bin type (sub ID = 0).
        sub_builder.add_bin_type(instance, bin_type_id, 1);
        const BinTypeId sub_bin_type_id = 0;

        // Add item types and track orig↔sub mappings.
        // add_item_type(const Instance&, ...) returns void and assigns IDs sequentially.
        std::vector<ItemTypeId> orig_to_sub(instance.number_of_item_types(), -1);
        std::vector<ItemTypeId> sub_to_orig;
        ItemTypeId sub_item_count = 0;

        auto add_to_sub = [&](ItemTypeId orig_id) {
            if (orig_to_sub[orig_id] != -1)
                return;
            sub_builder.add_item_type(instance, orig_id, profits[orig_id], free_copies[orig_id]);
            orig_to_sub[orig_id] = sub_item_count++;
            sub_to_orig.push_back(orig_id);
        };

        // Ensure all item types that appear in the selected bin are in the sub-instance
        // (needed for geometry even when free_copies == 0).
        for (const SolutionItem& item : sbin.items) {
            if (!item.is_fixed)
                add_to_sub(item.item_type_id);
        }
        // Also add item types for unplaced items that can fill freed space.
        for (ItemTypeId id = 0; id < instance.number_of_item_types(); ++id) {
            if (free_copies[id] > 0)
                add_to_sub(id);
        }

        // Fix the partially-inside and outside items in the sub-problem.
        for (ItemPos ip = 0; ip < (ItemPos)sbin.items.size(); ++ip) {
            const SolutionItem& item = sbin.items[ip];
            if (item.is_fixed || item_is_free[ip])
                continue;
            sub_builder.add_fixed_item(
                    sub_bin_type_id,
                    orig_to_sub[item.item_type_id],
                    item.bl_corner,
                    item.angle,
                    item.mirror);
        }

        Instance sub_instance = sub_builder.build();

        // Solve the sub-instance with tree search only.
        OptimizeParameters sub_params;
        sub_params.verbosity_level = 0;
        sub_params.timer = parameters.timer;
        sub_params.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        sub_params.optimization_mode = OptimizationMode::NotAnytime;
        sub_params.use_tree_search = true;
        sub_params.not_anytime_tree_search_queue_size = parameters.subproblem_queue_size;
        auto sub_output = optimize(sub_instance, sub_params);
        const Solution& sub_sol = sub_output.solution_pool.best();

        // Rebuild the main solution for the selected bin.
        // - All other bins: copy unchanged.
        // - Selected bin: keep non-freed items at original positions, add sub-problem placements.
        Solution new_solution(instance);
        for (BinPos bp = 0; bp < solution.number_of_different_bins(); ++bp) {
            const SolutionBin& sbin_b = solution.bin(bp);
            new_solution.add_bin(sbin_b.bin_type_id, 1);

            if (bp != bin_pos) {
                for (const SolutionItem& item : sbin_b.items) {
                    new_solution.add_item(
                            bp,
                            item.item_type_id,
                            item.bl_corner,
                            item.angle,
                            item.mirror,
                            item.is_fixed);
                }
            } else {
                // Keep items that were not freed (fixed in sub-problem or original fixed items).
                for (ItemPos ip = 0; ip < (ItemPos)sbin.items.size(); ++ip) {
                    const SolutionItem& item = sbin.items[ip];
                    if (item.is_fixed || !item_is_free[ip]) {
                        new_solution.add_item(
                                bp,
                                item.item_type_id,
                                item.bl_corner,
                                item.angle,
                                item.mirror,
                                item.is_fixed);
                    }
                }
                // Add items placed by the sub-problem (non-fixed in sub-solution).
                if (sub_sol.number_of_different_bins() > 0) {
                    for (const SolutionItem& sub_item : sub_sol.bin(0).items) {
                        if (sub_item.is_fixed)
                            continue;
                        ItemTypeId orig_id = sub_to_orig[sub_item.item_type_id];
                        new_solution.add_item(
                                bp,
                                orig_id,
                                sub_item.bl_corner,
                                sub_item.angle,
                                sub_item.mirror);
                    }
                }
            }
        }
        solution = new_solution;

        if (output.solution_pool.best().profit() < solution.profit()) {
            std::stringstream ss;
            ss << "it " << iter;
            algorithm_formatter.update_solution(solution, ss.str());
        }

        // Increase profits of item types that still have unplaced copies.
        std::fill(placed_all.begin(), placed_all.end(), 0);
        for (BinPos bp = 0; bp < solution.number_of_different_bins(); ++bp) {
            for (const SolutionItem& item : solution.bin(bp).items) {
                if (!item.is_fixed)
                    placed_all[item.item_type_id]++;
            }
        }
        for (ItemTypeId id = 0; id < instance.number_of_item_types(); ++id) {
            if (placed_all[id] < instance.item_type(id).copies)
                profits[id] *= parameters.profit_multiplier;
        }
    }

    algorithm_formatter.end();
    return output;
}

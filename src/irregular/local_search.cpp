#include "irregular/local_search.hpp"
#include "irregular/sequential_feasibility.hpp"
#include "irregular/linear_programming.hpp"
#include "irregular/solution_builder.hpp"
#include "irregular/local_search_direct_trigonometry/polygon.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "shape/boolean_operations.hpp"
#include "shape/shapes_intersections.hpp"

#include "localsearchsolver/population.hpp"
//#include "shape/writer.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <random>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

struct LocalSearchBinData
{
    /** Penalty per item position in this bin. */
    std::vector<Profit> item_penalties;

    /** Lambda (scale factor) per item position in this bin. */
    std::vector<double> lambda;

    /** Remaining area in this bin (heuristic for bin selection). */
    AreaDbl remaining_area = 0;
};

/**
 * Assign a single item to the solution.  Chooses a random orientation and
 * places the item inside the largest free region of the selected bin (boolean
 * difference of the bin AABB minus existing item shapes, defects, and
 * borders).  The bin is the valid one (item AABB fits) with the most remaining
 * area; falls back to the bin with the most remaining area if none fits.
 * Returns the bin position the item was placed in, or -1 if the selected bin
 * has no free space left (the item is then not added).
 */
BinPos assign_item_to_bin(
        const Instance& instance,
        std::mt19937_64& rng,
        ItemTypeId item_type_id,
        Solution& solution,
        const std::vector<LocalSearchBinData>& bin_data)
{
    const ItemType& item_type = instance.item_type(item_type_id);

    // Choose a random allowed rotation entry, then a random angle within it.
    std::uniform_int_distribution<Counter> range_dist(
            0, (Counter)item_type.allowed_rotations.size() - 1);
    const AllowedRotation& rotation_range = item_type.allowed_rotations[range_dist(rng)];
    std::uniform_real_distribution<Angle> angle_dist(
            rotation_range.start_angle, rotation_range.end_angle);
    const Angle angle = angle_dist(rng);
    const bool mirror = rotation_range.mirror;

    const AxisAlignedBoundingBox item_aabb = item_type.compute_min_max(angle, mirror);

    // Select the valid bin (item fits at full scale) with the most remaining area.
    // Fall back to the bin with the most remaining area if no bin fits.
    BinPos best_bin_pos = -1;
    AreaDbl best_remaining = -std::numeric_limits<AreaDbl>::infinity();
    for (BinPos bin_pos = 0;
            bin_pos < (BinPos)solution.number_of_different_bins();
            ++bin_pos) {
        const BinTypeId bin_type_id = solution.bin(bin_pos).bin_type_id;
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (bin_type.aabb_orig.x_max - bin_type.aabb_orig.x_min < item_aabb.x_max - item_aabb.x_min
                || bin_type.aabb_orig.y_max - bin_type.aabb_orig.y_min < item_aabb.y_max - item_aabb.y_min)
            continue;
        if (bin_data[bin_pos].remaining_area > best_remaining) {
            best_remaining = bin_data[bin_pos].remaining_area;
            best_bin_pos = bin_pos;
        }
    }
    if (best_bin_pos == -1) {
        best_bin_pos = 0;
        for (BinPos bin_pos = 1;
                bin_pos < (BinPos)solution.number_of_different_bins();
                ++bin_pos) {
            if (bin_data[bin_pos].remaining_area > bin_data[best_bin_pos].remaining_area)
                best_bin_pos = bin_pos;
        }
    }

    // Compute the free space in the bin as the boolean difference between the
    // bin AABB and the shapes of existing items, defects, and borders.
    const BinTypeId best_bin_type_id = solution.bin(best_bin_pos).bin_type_id;
    const BinType& best_bin_type = instance.bin_type(best_bin_type_id);
    const AxisAlignedBoundingBox& bin_aabb = best_bin_type.aabb_orig;

    shape::ShapeWithHoles bin_shape{instance.parameters().scale_value * shape::build_rectangle(bin_aabb), {}};

    std::vector<shape::ShapeWithHoles> occupied_shapes;
    const SolutionBin& best_bin = solution.bin(best_bin_pos);
    for (ItemPos item_pos = 0;
            item_pos < (ItemPos)best_bin.items.size();
            ++item_pos) {
        const ItemType& placed_item_type = instance.item_type(best_bin.items[item_pos].item_type_id);
        for (ItemShapePos shape_pos = 0;
                shape_pos < (ItemShapePos)placed_item_type.shapes.size();
                ++shape_pos) {
            occupied_shapes.push_back(solution.shape_scaled(best_bin_pos, item_pos, shape_pos));
        }
    }
    for (const Defect& defect: best_bin_type.defects)
        occupied_shapes.push_back(defect.shape_scaled);
    for (const Defect& border: best_bin_type.borders)
        occupied_shapes.push_back(border.shape_scaled);

    const shape::MultiShapeWithHoles free_regions_multi = shape::compute_difference(
            shape::MultiShapeWithHoles{{bin_shape}},
            shape::MultiShapeWithHoles{occupied_shapes});
    const std::vector<shape::ShapeWithHoles>& free_regions = free_regions_multi.shapes_with_holes;
    //shape::Writer().add_shape_with_holes(bin_shape, "Bin")
    //    .add_shapes_with_holes(occupied_shapes, "Occupied")
    //    .add_shapes_with_holes(free_regions, "Difference")
    //    .write_json("difference.json");

    // Select the free region with the largest area.
    Counter best_region_pos = -1;
    AreaDbl best_area = -1.0;
    for (Counter region_pos = 0;
            region_pos < (Counter)free_regions.size();
            ++region_pos) {
        const AreaDbl area = free_regions[region_pos].compute_area();
        if (area > best_area) {
            best_area = area;
            best_region_pos = region_pos;
        }
    }

    // If the bin has no free space left, the item can't be placed in it: a
    // point inside another item can't be separated from it by the
    // shrinkage LP.
    if (best_region_pos == -1)
        return -1;
    Point bl_corner = free_regions[best_region_pos].find_point_strictly_inside();
    //std::cout << "bl_corner " << bl_corner.to_string() << std::endl;
    bl_corner = 1.0 / instance.parameters().scale_value * bl_corner;
    //std::cout << "bl_corner " << bl_corner.to_string() << std::endl;
    SolutionBuilder solution_builder(instance);
    for (BinPos bp = 0; bp < solution.number_of_different_bins(); ++bp) {
        const SolutionBin& bin = solution.bin(bp);
        solution_builder.add_bin(bin.bin_type_id, bin.copies);
        for (const SolutionItem& item: bin.items) {
            solution_builder.add_item(
                    bp,
                    item.item_type_id,
                    item.bl_corner,
                    item.angle,
                    item.mirror,
                    item.is_fixed);
        }
    }
    solution_builder.add_item(best_bin_pos, item_type_id, bl_corner, angle, mirror);
    solution = solution_builder.build();

    return best_bin_pos;
}

/**
 * Assign an item to a bin, then run minimize_shrinkage on that bin until it
 * is feasible (all items at full scale).  Updates solution and bin_data in
 * place.  Returns false if the item fits in no bin, if feasibility wasn't
 * reached within
 * 'LocalSearchParameters::maximum_number_of_iterations_without_improvement'
 * iterations without improvement, or if the timer or end-flag fired before
 * feasibility was reached.
 */
bool pack_item(
        const Instance& instance,
        std::mt19937_64& rng,
        ItemTypeId item_type_id,
        Solution& solution,
        std::vector<LocalSearchBinData>& bin_data,
        AlgorithmFormatter& algorithm_formatter,
        const LocalSearchParameters& parameters)
{
    if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
        return false;

    // 'optimize_item_types_fit' only checks the original instance: the bins
    // of the sub-instances built by 'sequential_feasibility' are narrower.
    if (!instance.fits_some_bin(item_type_id))
        return false;

    const ItemType& item_type = instance.item_type(item_type_id);

    const BinPos bin_pos = assign_item_to_bin(
            instance, rng, item_type_id, solution, bin_data);
    if (bin_pos == -1)
        return false;

    bin_data[bin_pos].remaining_area -= item_type.area_scaled;
    bin_data[bin_pos].item_penalties.push_back(1.0);
    bin_data[bin_pos].lambda.push_back(0.0);

    Solution bin_solution(instance);
    bin_solution.append_bin(solution, bin_pos, 1);

    bool bin_feasible = false;
    // Sum of the scale factors of the items of the bin: it measures how far
    // the bin is from being feasible.
    double best_lambda_sum = -1;
    Counter number_of_iterations_without_improvement = 0;
    while (!bin_feasible) {
        if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
            return false;
        if (parameters.maximum_number_of_iterations_without_improvement != -1
                && number_of_iterations_without_improvement
                >= parameters.maximum_number_of_iterations_without_improvement) {
            return false;
        }

        LinearProgrammingMinimizeShrinkageParameters lp_params;
        lp_params.timer = parameters.timer;

        const LinearProgrammingMinimizeShrinkageOutput lp_output =
                linear_programming_minimize_shrinkage(
                        bin_solution,
                        bin_data[bin_pos].lambda,
                        bin_data[bin_pos].item_penalties,
                        lp_params);

        bin_feasible = lp_output.feasible;
        bin_data[bin_pos].lambda = lp_output.final_lambda;

        double lambda_sum = 0;
        for (double lambda: lp_output.final_lambda)
            lambda_sum += lambda;
        if (shape::strictly_greater(lambda_sum, best_lambda_sum)) {
            best_lambda_sum = lambda_sum;
            number_of_iterations_without_improvement = 0;
        } else {
            number_of_iterations_without_improvement++;
        }

        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)lp_output.items_shrunken.size();
                ++item_pos) {
            if (lp_output.items_shrunken[item_pos])
                bin_data[bin_pos].item_penalties[item_pos] += 1.0;
        }

        bin_solution = lp_output.solution;
    }

    // Rebuild solution with the updated bin.
    Solution new_solution(instance);
    for (BinPos bp = 0; bp < solution.number_of_bins(); ++bp) {
        if (bp == bin_pos) {
            new_solution.append_bin(bin_solution, 0, 1);
        } else {
            new_solution.append_bin(solution, bp, 1);
        }
    }
    solution = std::move(new_solution);
    return true;
}

/**
 * Local search with the linear programming engine, for objective Feasibility.
 */
void local_search_linear_programming(
        const Instance& instance,
        std::mt19937_64& generator,
        const LocalSearchParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{

    SolutionBuilder solution_builder(instance);
    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos)
        solution_builder.add_bin(instance.bin_type_id(bin_pos), 1);

    // Pre-place fixed items and initialize their bin_data entries.
    // Per-bin state.
    std::vector<LocalSearchBinData> bin_data(instance.number_of_bins());
    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos) {
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(bin_pos));
        bin_data[bin_pos].remaining_area = bin_type.area_scaled;
        for (const FixedItem& fixed_item: bin_type.fixed_items) {
            const ItemType& item_type = instance.item_type(fixed_item.item_type_id);
            solution_builder.add_item(
                    bin_pos,
                    fixed_item.item_type_id,
                    fixed_item.bl_corner,
                    fixed_item.angle,
                    fixed_item.mirror,
                    true);
            bin_data[bin_pos].remaining_area -= item_type.area_scaled;
            bin_data[bin_pos].item_penalties.push_back(
                    std::numeric_limits<Profit>::infinity());
            bin_data[bin_pos].lambda.push_back(1.0);
        }
    }
    Solution solution = solution_builder.build();

    // Build a flat list of all remaining item copies sorted ascending by area
    // so that pop_back processes the largest items first.
    // Exclude copies already fixed in the bin types.
    std::vector<ItemTypeId> unpacked_items;
    unpacked_items.reserve(instance.number_of_items());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemPos copy = 0; copy < item_type.copies - item_type.copies_fixed; ++copy)
            unpacked_items.push_back(item_type_id);
    }
    std::sort(unpacked_items.begin(), unpacked_items.end(),
            [&](ItemTypeId a, ItemTypeId b) {
                return instance.item_type(a).area_orig < instance.item_type(b).area_orig;
            });

    // Pack all items into the available bins.
    while (!unpacked_items.empty()) {
        if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
            break;
        const ItemTypeId item_type_id = unpacked_items.back();
        unpacked_items.pop_back();
        if (!pack_item(
                instance,
                generator,
                item_type_id,
                solution,
                bin_data,
                algorithm_formatter,
                parameters))
            break;
    }

    std::stringstream ss;
    ss << "LS";
    algorithm_formatter.update_solution(solution, ss.str());
}

/**
 * Return true iff the solution contains all the items, without overlaps and
 * inside the bins, according to the exact shapes.
 *
 * Same checks as 'Solution::compute_overlapping_items', but only the pairs of
 * shapes whose bounding boxes intersect are tested, and the pairs of shapes
 * made of line segments are tested with the polygon intersection test (the
 * holes of the items being ignored, which is conservative).
 */
bool is_feasible(const Solution& solution)
{
    if (!solution.full() || !solution.feasible())
        return false;
    const Instance& instance = solution.instance();
    const LengthDbl scale_value = instance.parameters().scale_value;
    const Point origin = {0.0, 0.0};

    // Polygons of the item shapes in each orientation, not translated.
    std::map<std::tuple<ItemTypeId, ItemShapePos, Angle, bool>, Polygon> item_polygons;

    for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        const BinType& bin_type = instance.bin_type(solution_bin.bin_type_id);

        // Shapes of the bin: the obstacles (item_pos == -1) and the items.
        struct BinShape
        {
            ItemPos item_pos;
            const ShapeWithHoles* shape;
            const Polygon* polygon;
            Point translation;
            AxisAlignedBoundingBox aabb;
        };
        std::vector<ShapeWithHoles> item_shapes;
        std::vector<BinShape> bin_shapes;
        std::vector<Polygon> obstacle_polygons;
        std::vector<const Defect*> obstacles;
        for (const Defect& border: bin_type.borders)
            obstacles.push_back(&border);
        for (const Defect& defect: bin_type.defects)
            obstacles.push_back(&defect);
        obstacle_polygons.reserve(obstacles.size());
        for (const Defect* obstacle: obstacles) {
            BinShape bin_shape;
            bin_shape.item_pos = -1;
            bin_shape.shape = &obstacle->shape_scaled;
            bin_shape.polygon = nullptr;
            bin_shape.translation = origin;
            bin_shape.aabb = obstacle->shape_scaled.shape.compute_min_max();
            if (obstacle->shape_scaled.shape.is_polygon()) {
                obstacle_polygons.push_back(build_polygon(obstacle->shape_scaled.shape));
                bin_shape.polygon = &obstacle_polygons.back();
            }
            bin_shapes.push_back(bin_shape);
        }
        Counter number_of_item_shapes = 0;
        for (const SolutionItem& item: solution_bin.items)
            number_of_item_shapes += instance.item_type(item.item_type_id).shapes.size();
        item_shapes.reserve(number_of_item_shapes);
        for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_bin.items.size(); ++item_pos) {
            const SolutionItem& item = solution_bin.items[item_pos];
            const ItemType& item_type = instance.item_type(item.item_type_id);
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                item_shapes.push_back(solution.shape_scaled(bin_pos, item_pos, item_shape_pos));
                BinShape bin_shape;
                bin_shape.item_pos = item_pos;
                bin_shape.shape = &item_shapes.back();
                bin_shape.polygon = nullptr;
                bin_shape.translation = {scale_value * item.bl_corner.x, scale_value * item.bl_corner.y};
                bin_shape.aabb = item_shapes.back().shape.compute_min_max();
                // Inside the bin.
                if (shape::strictly_lesser(bin_shape.aabb.x_min, bin_type.aabb_scaled.x_min)
                        || shape::strictly_greater(bin_shape.aabb.x_max, bin_type.aabb_scaled.x_max)
                        || shape::strictly_lesser(bin_shape.aabb.y_min, bin_type.aabb_scaled.y_min)
                        || shape::strictly_greater(bin_shape.aabb.y_max, bin_type.aabb_scaled.y_max)) {
                    return false;
                }
                std::tuple<ItemTypeId, ItemShapePos, Angle, bool> key(
                        item.item_type_id,
                        item_shape_pos,
                        item.angle,
                        item.mirror);
                auto item_polygon_it = item_polygons.find(key);
                if (item_polygon_it == item_polygons.end()) {
                    ShapeWithHoles shape = instance.item_shape_scaled(
                            item.item_type_id,
                            item_shape_pos,
                            item.angle,
                            item.mirror);
                    if (shape.shape.is_polygon()) {
                        item_polygon_it = item_polygons.insert({key, build_polygon(shape.shape)}).first;
                    }
                }
                if (item_polygon_it != item_polygons.end())
                    bin_shape.polygon = &item_polygon_it->second;
                bin_shapes.push_back(bin_shape);
            }
        }

        // Pairs of shapes whose bounding boxes intersect.
        std::sort(
                bin_shapes.begin(),
                bin_shapes.end(),
                [](const BinShape& bin_shape_1, const BinShape& bin_shape_2)
                {
                    return bin_shape_1.aabb.x_min < bin_shape_2.aabb.x_min;
                });
        for (Counter shape_pos_1 = 0; shape_pos_1 < (Counter)bin_shapes.size(); ++shape_pos_1) {
            const BinShape& bin_shape_1 = bin_shapes[shape_pos_1];
            for (Counter shape_pos_2 = shape_pos_1 + 1;
                    shape_pos_2 < (Counter)bin_shapes.size()
                    && bin_shapes[shape_pos_2].aabb.x_min < bin_shape_1.aabb.x_max;
                    ++shape_pos_2) {
                const BinShape& bin_shape_2 = bin_shapes[shape_pos_2];
                if (bin_shape_1.item_pos == -1 && bin_shape_2.item_pos == -1)
                    continue;
                if (bin_shape_1.item_pos == bin_shape_2.item_pos)
                    continue;
                if (bin_shape_2.aabb.y_min >= bin_shape_1.aabb.y_max
                        || bin_shape_1.aabb.y_min >= bin_shape_2.aabb.y_max) {
                    continue;
                }
                bool overlap = (bin_shape_1.polygon != nullptr && bin_shape_2.polygon != nullptr)?
                    intersect(
                            *bin_shape_1.polygon,
                            bin_shape_1.translation,
                            *bin_shape_2.polygon,
                            bin_shape_2.translation):
                    shape::intersect(*bin_shape_1.shape, *bin_shape_2.shape, true);
                if (overlap)
                    return false;
            }
        }
    }
    return true;
}

/**
 * Local search with the direct trigonometry engine, for objective
 * Feasibility.
 *
 * Population-based iterated local search, generalizing the exploration phase
 * of Sparrow at a fixed bin size: the layout is separated; after a failure,
 * the layout is added to the population, and the next separation starts from
 * a disruption of a layout of the population.
 */
void local_search_direct_trigonometry(
        const Instance& instance,
        std::mt19937_64& generator,
        const Solution* initial_solution,
        const LocalSearchParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    Separator separator(
            instance,
            parameters.separator_parameters,
            generator,
            parameters.separator_item_types);
    if (initial_solution != nullptr) {
        separator.set_solution(
                convert_initial_solution(instance, *initial_solution),
                parameters.timer);
    } else {
        separator.set_solution(Solution(instance), parameters.timer);
    }

    // Layouts of the failed separations: in a population, and sorted by
    // increasing loss.
    struct FailedLayout
    {
        std::vector<Separator::ItemState> layout;

        double loss;
    };
    localsearchsolver::Population<FailedLayout, double>::Parameters population_parameters;
    population_parameters.minimum_size = parameters.population_minimum_size;
    population_parameters.maximum_size = parameters.population_maximum_size;
    population_parameters.number_of_elite_solutions = parameters.population_number_of_elite_solutions;
    population_parameters.number_of_closest_neighbors = parameters.population_number_of_closest_neighbors;
    localsearchsolver::Population<FailedLayout, double> population(
            [](const FailedLayout& failed_layout)
            {
                return failed_layout.loss;
            },
            [&separator](
                const FailedLayout& failed_layout_1,
                const FailedLayout& failed_layout_2)
            {
                return separator.distance(
                        failed_layout_1.layout,
                        failed_layout_2.layout);
            },
            population_parameters);
    std::vector<FailedLayout> sorted_failed_layouts;

    std::normal_distribution<double> normal_distribution(0.0, 0.25);
    for (Counter number_of_failed_separations = 0;;) {
        if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
            return;
        if (separator.separate(parameters.timer)) {
            Solution solution = separator.solution();
            if (is_feasible(solution)) {
                algorithm_formatter.update_solution(solution, "LS");
                return;
            }
        }

        FailedLayout failed_layout = {separator.layout(), separator.total_loss()};
        population.add(failed_layout, generator);
        sorted_failed_layouts.insert(
                std::upper_bound(
                    sorted_failed_layouts.begin(),
                    sorted_failed_layouts.end(),
                    failed_layout,
                    [](const FailedLayout& failed_layout_1, const FailedLayout& failed_layout_2)
                    {
                        return failed_layout_1.loss < failed_layout_2.loss;
                    }),
                failed_layout);
        number_of_failed_separations++;
        if (number_of_failed_separations >= parameters.maximum_number_of_failed_separations)
            return;

        // Disrupt the selected layout.
        if (parameters.restart_selection == RestartSelection::Population) {
            // Binary tournament in the population.
            separator.set_layout(population.binary_tournament_single(generator).layout);
        } else {
            // A layout with a small loss (selection of Sparrow).
            double sample = (std::min)(std::abs(normal_distribution(generator)), 0.999);
            Counter failed_layout_pos = (Counter)(sample * sorted_failed_layouts.size());
            separator.set_layout(sorted_failed_layouts[failed_layout_pos].layout);
        }
        separator.disrupt();
    }
}

}  // namespace

Solution packingsolver::irregular::convert_initial_solution(
        const Instance& instance,
        const Solution& solution)
{
    std::vector<BinPos> bin_types_remaining_copies(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        bin_types_remaining_copies[bin_type_id]
            = instance.bin_type(bin_type_id).copies;
    }

    // Bin type of each copy of each bin of 'solution' (-1 if dropped).
    std::vector<std::vector<BinTypeId>> bins_bin_type_ids(solution.number_of_different_bins());
    // First, the bins of the same bin type.
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& bin = solution.bin(bin_pos);
        bins_bin_type_ids[bin_pos].resize(bin.copies, -1);
        if (bin.bin_type_id >= instance.number_of_bin_types())
            continue;
        for (BinPos copy = 0; copy < bin.copies; ++copy) {
            if (bin_types_remaining_copies[bin.bin_type_id] == 0)
                break;
            bins_bin_type_ids[bin_pos][copy] = bin.bin_type_id;
            bin_types_remaining_copies[bin.bin_type_id]--;
        }
    }
    // Then, the remaining bins.
    BinTypeId bin_type_id_cur = 0;
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        for (BinTypeId& bin_type_id: bins_bin_type_ids[bin_pos]) {
            if (bin_type_id != -1)
                continue;
            while (bin_type_id_cur < instance.number_of_bin_types()
                    && bin_types_remaining_copies[bin_type_id_cur] == 0) {
                bin_type_id_cur++;
            }
            if (bin_type_id_cur == instance.number_of_bin_types())
                break;
            bin_type_id = bin_type_id_cur;
            bin_types_remaining_copies[bin_type_id_cur]--;
        }
    }

    SolutionBuilder solution_builder(instance);
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& bin = solution.bin(bin_pos);
        for (BinTypeId bin_type_id: bins_bin_type_ids[bin_pos]) {
            if (bin_type_id == -1)
                continue;
            BinPos bin_pos_new = solution_builder.add_bin(bin_type_id, 1);
            for (const SolutionItem& item: bin.items) {
                solution_builder.add_item(
                        bin_pos_new,
                        item.item_type_id,
                        item.bl_corner,
                        item.angle,
                        item.mirror,
                        item.is_fixed);
            }
        }
    }
    return solution_builder.build();
}

LocalSearchOutput packingsolver::irregular::local_search(
        const Instance& instance,
        std::mt19937_64& generator,
        const Solution* initial_solution,
        const LocalSearchParameters& parameters)
{
    if (instance.objective() != Objective::Feasibility) {
        LocalSearchOutput output(instance);
        AlgorithmFormatter algorithm_formatter(instance, parameters, output);
        algorithm_formatter.start();
        algorithm_formatter.print_header();

        // The item types of the separator are the same for all the
        // sub-problems.
        std::shared_ptr<const Separator::ItemTypes> separator_item_types
            = parameters.separator_item_types;
        if (parameters.engine == LocalSearchEngine::DirectTrigonometry
                && separator_item_types == nullptr) {
            separator_item_types = Separator::build_item_types(
                    instance,
                    parameters.separator_parameters);
        }

        // The sequential feasibility algorithm only goes on after a
        // sub-problem without feasible solution during the compression: then,
        // each sub-problem is a single, more persistent, separation attempt.
        bool compression = false;
        SequentialFeasibilitySolver solver = [&parameters, &algorithm_formatter, &generator, &compression, initial_solution, separator_item_types](
                const Instance& sub_instance,
                const Solution* previous_solution)
        {
            LocalSearchParameters inner_parameters;
            inner_parameters.verbosity_level = 0;
            inner_parameters.timer = parameters.timer;
            inner_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            inner_parameters.engine = parameters.engine;
            inner_parameters.shrink_ratio = parameters.shrink_ratio;
            inner_parameters.compression = parameters.compression;
            inner_parameters.exploration_time_ratio = parameters.exploration_time_ratio;
            inner_parameters.separator_parameters = parameters.separator_parameters;
            inner_parameters.separator_item_types = separator_item_types;
            inner_parameters.restart_selection = parameters.restart_selection;
            inner_parameters.maximum_number_of_failed_separations
                = parameters.maximum_number_of_failed_separations;
            inner_parameters.population_minimum_size = parameters.population_minimum_size;
            inner_parameters.population_maximum_size = parameters.population_maximum_size;
            inner_parameters.population_number_of_elite_solutions
                = parameters.population_number_of_elite_solutions;
            inner_parameters.population_number_of_closest_neighbors
                = parameters.population_number_of_closest_neighbors;
            inner_parameters.maximum_number_of_iterations_without_improvement
                = parameters.maximum_number_of_iterations_without_improvement;
            if (!compression && !std::isinf(parameters.timer.time_limit())) {
                // Exploration: go on until the end of the exploration.
                inner_parameters.timer.set_time_limit(
                        parameters.exploration_time_ratio * parameters.timer.time_limit());
                inner_parameters.maximum_number_of_failed_separations
                    = std::numeric_limits<Counter>::max();
            }
            if (compression) {
                inner_parameters.maximum_number_of_failed_separations
                    = parameters.compression_maximum_number_of_failed_separations;
                inner_parameters.separator_parameters = parameters.compression_separator_parameters;
                inner_parameters.separator_parameters.maximum_approximation_ratio
                    = parameters.separator_parameters.maximum_approximation_ratio;
            }
            // The first sub-problem has no previous solution: warm start it
            // from the initial solution.
            SolutionPool<Instance, Solution> solution_pool = local_search(
                    sub_instance,
                    generator,
                    (previous_solution != nullptr)? previous_solution: initial_solution,
                    inner_parameters).solution_pool;
            if (!solution_pool.best().feasible())
                compression = true;
            return solution_pool;
        };

        SequentialFeasibilityParameters sf_parameters;
        sf_parameters.verbosity_level = 0;
        sf_parameters.shrink_ratio = parameters.shrink_ratio;
        sf_parameters.compression = parameters.compression;
        sf_parameters.timer = parameters.timer;
        sf_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        sf_parameters.new_solution_callback = [&algorithm_formatter](
                const packingsolver::Output<Instance, Solution>& sf_output)
        {
            algorithm_formatter.update_solution(
                    sf_output.solution_pool.best(),
                    sf_output.solution_pool.best_label());
        };

        sequential_feasibility(instance, solver, sf_parameters);

        algorithm_formatter.end();
        return output;
    }

    LocalSearchOutput output(instance);

    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (parameters.engine == LocalSearchEngine::LinearProgramming) {
        local_search_linear_programming(instance, generator, parameters, algorithm_formatter);
    } else {
        local_search_direct_trigonometry(instance, generator, initial_solution, parameters, algorithm_formatter);
    }

    algorithm_formatter.end();
    return output;
}

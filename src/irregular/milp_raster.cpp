#include "irregular/milp_raster.hpp"

#include "irregular/rotations.hpp"
#include "irregular/solution_builder.hpp"
#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "shape/rasterization.hpp"
#include "shape/offset.hpp"
#include "shape/approximation.hpp"
#include "shape/boolean_operations.hpp"
#include "shape/convex_partition.hpp"
//#include "shape/writer.hpp"

#ifdef CBC_FOUND
#include "mathoptsolverscmake/mathopt_cbc.hpp"
#endif
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif
#ifdef XPRESS_FOUND
#include "mathoptsolverscmake/mathopt_xpress.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <thread>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

LengthDbl compute_gcd(LengthDbl a, LengthDbl b)
{
    if (shape::equal(b, 0.0))
        return a;
    if (a == -1)
        return b;
    while (!shape::equal(b, 0.0)) {
        double temp = b;
        b = std::fmod(a, b);
        a = temp;
    }
    return a;
}

using PlacementId = int64_t;

struct BasePlacement
{
    ItemTypeId item_type_id;
    Angle angle;
    bool mirror;
    shape::ColumnId col;
    shape::RowId row;
    LengthDbl aabb_x_min;
    LengthDbl aabb_y_min;
};

struct BinTypeData
{
    shape::ColumnId col_shift;
    shape::ColumnId col_max;
    shape::RowId row_shift;
    shape::RowId row_max;
    shape::ColumnId num_cols;
    shape::RowId num_rows;
    std::vector<std::vector<bool>> available;
    /** Grid cells blocked by fixed items of this bin type. */
    std::vector<std::vector<bool>> fixed_items;
    std::vector<BasePlacement> base_placements;
    std::vector<std::vector<std::vector<PlacementId>>> base_cell_placements;
    std::vector<std::vector<PlacementId>> base_item_placements;
};

Solution solve_milp_raster_for_cell_size(
        const Instance& instance,
        const std::vector<std::vector<std::vector<ItemTypeRotation>>>& item_type_rotations,
        const MilpRasterParameters& parameters,
        MilpRasterOutput& output,
        AlgorithmFormatter& algorithm_formatter,
        LengthDbl cell_size,
        BinPos number_of_bins)
{
    //std::cout << "solve_milp_raster_for_cell_size"
    //    << " cell_size " << cell_size
    //    << " number_of_bins " << number_of_bins
    //    << std::endl;
    const LengthDbl scale = instance.parameters().scale_value;

    std::vector<BinTypeData> bin_type_data(instance.number_of_bin_types());

    //shape::Writer writer;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinTypeData& data = bin_type_data[bin_type_id];

        data.col_shift = (shape::ColumnId)std::floor(bin_type.aabb_scaled.x_min / cell_size);
        if (shape::strictly_greater(bin_type.aabb_scaled.x_min, data.col_shift * cell_size))
            data.col_shift++;
        data.col_max = (shape::ColumnId)std::floor(bin_type.aabb_scaled.x_max / cell_size);
        if (shape::strictly_lesser(bin_type.aabb_scaled.x_max, (data.col_max + 1) * cell_size))
            data.col_max--;
        data.row_shift = (shape::RowId)std::floor(bin_type.aabb_scaled.y_min / cell_size);
        if (shape::strictly_greater(bin_type.aabb_scaled.y_min, data.row_shift * cell_size))
            data.row_shift++;
        data.row_max = (shape::RowId)std::floor(bin_type.aabb_scaled.y_max / cell_size);
        if (shape::strictly_lesser(bin_type.aabb_scaled.y_max, (data.row_max + 1) * cell_size))
            data.row_max--;
        data.num_cols = data.col_max - data.col_shift + 1;
        data.num_rows = data.row_max - data.row_shift + 1;

        data.available.assign(data.num_cols, std::vector<bool>(data.num_rows, true));
        // Item-bin spacing along the sides of the bounding box of the bin (the
        // borders only cover the parts of the bounding box outside the bin).
        LengthDbl bin_spacing = instance.bin_spacing_scaled(bin_type_id);
        if (bin_spacing > 0) {
            for (shape::ColumnId c = 0; c < data.num_cols; ++c) {
                for (shape::RowId r = 0; r < data.num_rows; ++r) {
                    LengthDbl x_start = (c + data.col_shift) * cell_size;
                    LengthDbl y_start = (r + data.row_shift) * cell_size;
                    if (shape::strictly_lesser(x_start, bin_type.aabb_scaled.x_min + bin_spacing)
                            || shape::strictly_greater(x_start + cell_size, bin_type.aabb_scaled.x_max - bin_spacing)
                            || shape::strictly_lesser(y_start, bin_type.aabb_scaled.y_min + bin_spacing)
                            || shape::strictly_greater(y_start + cell_size, bin_type.aabb_scaled.y_max - bin_spacing)) {
                        data.available[c][r] = false;
                    }
                }
            }
        }

        for (Counter border_pos = 0;
                border_pos < (Counter)bin_type.borders.size();
                ++border_pos) {
            // Inflated by the item-bin spacing.
            const ShapeWithHoles& border_shape = bin_type.borders[border_pos].shape_inflated;
            shape::RasterizedGrid border_grid = shape::rasterization(border_shape, cell_size, cell_size);
            //writer.add_shape_with_holes(border_shape, "Border " + std::to_string(border_pos));
            for (shape::ColumnId column = border_grid.column_offset;
                    column < border_grid.column_offset + border_grid.number_of_columns;
                    ++column) {
                for (shape::RowId row = border_grid.row_offset;
                        row < border_grid.row_offset + border_grid.number_of_rows;
                        ++row) {
                    if (!shape::strictly_greater(border_grid.at(column, row).coverage, 0.0))
                        continue;
                    shape::ColumnId c = column - data.col_shift;
                    shape::RowId r = row - data.row_shift;
                    if (c >= 0 && c < data.num_cols && r >= 0 && r < data.num_rows)
                        data.available[c][r] = false;
                }
            }
        }

        for (Counter defect_pos = 0;
                defect_pos < (Counter)bin_type.defects.size();
                ++defect_pos) {
            // Inflated by the item-defect spacing.
            const ShapeWithHoles& defect_shape = bin_type.defects[defect_pos].shape_inflated;
            shape::RasterizedGrid defect_grid = shape::rasterization(defect_shape, cell_size, cell_size);
            //writer.add_shape_with_holes(defect_shape, "Defect " + std::to_string(defect_pos));
            for (shape::ColumnId column = defect_grid.column_offset;
                    column < defect_grid.column_offset + defect_grid.number_of_columns;
                    ++column) {
                for (shape::RowId row = defect_grid.row_offset;
                        row < defect_grid.row_offset + defect_grid.number_of_rows;
                        ++row) {
                    if (!shape::strictly_greater(defect_grid.at(column, row).coverage, 0.0))
                        continue;
                    shape::ColumnId c = column - data.col_shift;
                    shape::RowId r = row - data.row_shift;
                    if (c >= 0 && c < data.num_cols && r >= 0 && r < data.num_rows)
                        data.available[c][r] = false;
                }
            }
        }

        // Mark cells occupied by fixed items of this bin type.
        data.fixed_items.assign(data.num_cols, std::vector<bool>(data.num_rows, false));
        for (const FixedItem& fixed_item: bin_type.fixed_items) {
            const ItemType& item_type = instance.item_type(fixed_item.item_type_id);
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                // Inflated by half the item-item spacing, as the placements
                // (see below).
                ShapeWithHoles transformed = shape::inflate(
                        instance.item_shape_scaled(fixed_item.item_type_id, item_shape_pos, fixed_item.angle, fixed_item.mirror),
                        instance.item_spacing_scaled() / 2);
                transformed.shift(
                        fixed_item.bl_corner.x * scale,
                        fixed_item.bl_corner.y * scale);
                shape::RasterizedGrid raster_grid = shape::rasterization(transformed, cell_size, cell_size);
                for (shape::ColumnId column = raster_grid.column_offset;
                        column < raster_grid.column_offset + raster_grid.number_of_columns;
                        ++column) {
                    for (shape::RowId row = raster_grid.row_offset;
                            row < raster_grid.row_offset + raster_grid.number_of_rows;
                            ++row) {
                        if (!shape::strictly_greater(raster_grid.at(column, row).coverage, 0.0))
                            continue;
                        shape::ColumnId c = column - data.col_shift;
                        shape::RowId r = row - data.row_shift;
                        if (c >= 0 && c < data.num_cols && r >= 0 && r < data.num_rows)
                            data.fixed_items[c][r] = true;
                    }
                }
            }
        }

        data.base_cell_placements.assign(
                data.num_cols,
                std::vector<std::vector<PlacementId>>(data.num_rows));
        data.base_item_placements.resize(instance.number_of_item_types());

        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);

            for (const ItemTypeRotation& rotation: item_type_rotations[bin_type_id][item_type_id]) {
                // Compute AABB of all item shapes after applying mirror then
                // rotation, matching the order used in convert_shape.
                AxisAlignedBoundingBox combined_aabb;
                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type.shapes.size();
                        ++item_shape_pos) {
                    ShapeWithHoles transformed = instance.item_shape_scaled(item_type_id, item_shape_pos, rotation.angle, rotation.mirror);
                    AxisAlignedBoundingBox shape_aabb = transformed.compute_min_max();
                    combined_aabb = merge(combined_aabb, shape_aabb);
                }

                // Rasterize each item shape (shifted to origin) and collect cells:
                // - the cells of the item itself, which must be available
                //   (outside of the borders and the defects, inflated by
                //   their spacings);
                // - the cells of the item inflated by half the item-item
                //   spacing, which can't be covered by another placement
                //   (whose cells are inflated the same way) or by a fixed
                //   item (inflated the same way too), so that the items are
                //   at least the item-item spacing apart.
                LengthDbl half_item_spacing = instance.item_spacing_scaled() / 2;
                std::vector<std::pair<shape::ColumnId, shape::RowId>> placement_cells;
                std::vector<std::pair<shape::ColumnId, shape::RowId>> conflict_cells;
                auto collect_cells = [cell_size](
                        const ShapeWithHoles& shape,
                        std::vector<std::pair<shape::ColumnId, shape::RowId>>& cells)
                {
                    shape::RasterizedGrid raster_grid = shape::rasterization(shape, cell_size, cell_size);
                    for (shape::ColumnId column = raster_grid.column_offset;
                            column < raster_grid.column_offset + raster_grid.number_of_columns;
                            ++column) {
                        for (shape::RowId row = raster_grid.row_offset;
                                row < raster_grid.row_offset + raster_grid.number_of_rows;
                                ++row) {
                            if (shape::strictly_greater(raster_grid.at(column, row).coverage, 0.0))
                                cells.push_back(std::make_pair(column, row));
                        }
                    }
                };
                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type.shapes.size();
                        ++item_shape_pos) {
                    ShapeWithHoles transformed = instance.item_shape_scaled(item_type_id, item_shape_pos, rotation.angle, rotation.mirror);
                    transformed.shift(-combined_aabb.x_min, -combined_aabb.y_min);
                    collect_cells(transformed, placement_cells);
                    if (half_item_spacing > 0)
                        collect_cells(shape::inflate(transformed, half_item_spacing), conflict_cells);
                }

                if (placement_cells.empty())
                    continue;

                std::sort(placement_cells.begin(), placement_cells.end());
                placement_cells.erase(std::unique(placement_cells.begin(), placement_cells.end()), placement_cells.end());
                if (half_item_spacing > 0) {
                    std::sort(conflict_cells.begin(), conflict_cells.end());
                    conflict_cells.erase(std::unique(conflict_cells.begin(), conflict_cells.end()), conflict_cells.end());
                } else {
                    conflict_cells = placement_cells;
                }

                shape::ColumnId item_min_col = placement_cells[0].first;
                shape::ColumnId item_max_col = placement_cells[0].first;
                shape::RowId item_min_row = placement_cells[0].second;
                shape::RowId item_max_row = placement_cells[0].second;
                for (const std::pair<shape::ColumnId, shape::RowId>& cr: placement_cells) {
                    if (cr.first < item_min_col) item_min_col = cr.first;
                    if (cr.first > item_max_col) item_max_col = cr.first;
                    if (cr.second < item_min_row) item_min_row = cr.second;
                    if (cr.second > item_max_row) item_max_row = cr.second;
                }

                shape::ColumnId col_start = data.col_shift - item_min_col;
                shape::ColumnId col_end = data.col_max - item_max_col;
                shape::RowId row_start = data.row_shift - item_min_row;
                shape::RowId row_end = data.row_max - item_max_row;

                for (shape::ColumnId col = col_start; col <= col_end; ++col) {
                    for (shape::RowId row = row_start; row <= row_end; ++row) {
                        bool valid = true;
                        for (const std::pair<shape::ColumnId, shape::RowId>& cr: placement_cells) {
                            shape::ColumnId bc = col + cr.first - data.col_shift;
                            shape::RowId br = row + cr.second - data.row_shift;
                            if (!data.available[bc][br]) {
                                valid = false;
                                break;
                            }
                        }
                        if (!valid)
                            continue;

                        PlacementId placement_id = (PlacementId)data.base_placements.size();
                        BasePlacement bp;
                        bp.item_type_id = item_type_id;
                        bp.angle = rotation.angle;
                        bp.mirror = rotation.mirror;
                        bp.col = col;
                        bp.row = row;
                        bp.aabb_x_min = combined_aabb.x_min;
                        bp.aabb_y_min = combined_aabb.y_min;
                        data.base_placements.push_back(bp);
                        data.base_item_placements[item_type_id].push_back(placement_id);

                        // The conflict cells outside of the grid are not
                        // needed: if two items in the grid are closer than
                        // the spacing, their inflated shapes overlap around
                        // the middle of their closest points, which is in the
                        // grid.
                        for (const std::pair<shape::ColumnId, shape::RowId>& cr: conflict_cells) {
                            shape::ColumnId bc = col + cr.first - data.col_shift;
                            shape::RowId br = row + cr.second - data.row_shift;
                            if (bc < 0 || bc >= data.num_cols || br < 0 || br >= data.num_rows)
                                continue;
                            data.base_cell_placements[bc][br].push_back(placement_id);
                        }
                    }
                }
            }
        }
    }

    //writer.write_json("rasterization_" + std::to_string(number_of_bins) + "_" + std::to_string(cell_size) + ".json");

    std::vector<PlacementId> global_offset(number_of_bins);
    PlacementId total_placements = 0;
    for (BinPos bin_pos = 0; bin_pos < number_of_bins; ++bin_pos) {
        global_offset[bin_pos] = total_placements;
        total_placements += (PlacementId)bin_type_data[instance.bin_type_id(bin_pos)].base_placements.size();
    }

    // Build MILP model.
    mathoptsolverscmake::MathOptModel model;
    model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;

    model.variables_lower_bounds.assign(total_placements, 0.0);
    model.variables_upper_bounds.assign(total_placements, 1.0);
    model.variables_types.assign(total_placements, mathoptsolverscmake::VariableType::Integer);
    model.objective_coefficients.assign(total_placements, 0.0);

    if (instance.objective() == Objective::Knapsack) {
        for (BinPos bin_pos = 0; bin_pos < number_of_bins; ++bin_pos) {
            BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
            const BinTypeData& data = bin_type_data[bin_type_id];
            for (PlacementId placement_id = 0;
                    placement_id < (PlacementId)data.base_placements.size();
                    ++placement_id) {
                const BasePlacement& bp = data.base_placements[placement_id];
                const ItemType& item_type = instance.item_type(bp.item_type_id);
                model.objective_coefficients[global_offset[bin_pos] + placement_id] = item_type.profit;
            }
        }
    }

    // Copy constraints: for each item type, remaining_copies_min - copies_fixed
    // <= total placements across all bins <= copies - copies_fixed.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemPos remaining_copies = item_type.copies - item_type.copies_fixed;
        ItemPos remaining_copies_min = (std::max)(
                (ItemPos)0,
                item_type.copies_min - item_type.copies_fixed);
        model.constraints_starts.push_back(model.elements_variables.size());
        model.constraints_lower_bounds.push_back((double)remaining_copies_min);
        model.constraints_upper_bounds.push_back((double)remaining_copies);
        for (BinPos bin_pos = 0; bin_pos < number_of_bins; ++bin_pos) {
            BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
            const BinTypeData& data = bin_type_data[bin_type_id];
            for (PlacementId placement_id: data.base_item_placements[item_type_id]) {
                model.elements_variables.push_back(global_offset[bin_pos] + placement_id);
                model.elements_coefficients.push_back(1.0);
            }
        }
    }

    // Cell conflict constraints: for each (bin_pos, col, row), at most one placement,
    // or zero if the cell is occupied by a fixed item.
    for (BinPos bin_pos = 0; bin_pos < number_of_bins; ++bin_pos) {
        BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
        const BinTypeData& data = bin_type_data[bin_type_id];
        for (shape::ColumnId col_idx = 0; col_idx < data.num_cols; ++col_idx) {
            for (shape::RowId row_idx = 0; row_idx < data.num_rows; ++row_idx) {
                const std::vector<PlacementId>& cell_placements = data.base_cell_placements[col_idx][row_idx];
                if (cell_placements.empty())
                    continue;
                bool blocked = bin_type_data[bin_type_id].fixed_items[col_idx][row_idx];
                double upper_bound = blocked ? 0.0 : 1.0;
                if (cell_placements.size() <= 1 && !blocked)
                    continue;
                model.constraints_starts.push_back(model.elements_variables.size());
                model.constraints_lower_bounds.push_back(0.0);
                model.constraints_upper_bounds.push_back(upper_bound);
                for (PlacementId placement_id: cell_placements) {
                    model.elements_variables.push_back(global_offset[bin_pos] + placement_id);
                    model.elements_coefficients.push_back(1.0);
                }
            }
        }
    }

    auto retrieve_solution = [&](const std::vector<double>& milp_solution)
    {
        // Extract solution.
        SolutionBuilder solution_builder(instance);
        for (BinPos bin_pos = 0; bin_pos < number_of_bins; ++bin_pos) {
            BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
            const BinTypeData& data = bin_type_data[bin_type_id];
            BinPos solution_bin_pos = solution_builder.add_bin(bin_type_id, 1);
            for (PlacementId placement_id = 0;
                    placement_id < (PlacementId)data.base_placements.size();
                    ++placement_id) {
                if (milp_solution[global_offset[bin_pos] + placement_id] < 0.5)
                    continue;
                const BasePlacement& bp = data.base_placements[placement_id];
                Point bl_corner;
                bl_corner.x = (bp.col * cell_size - bp.aabb_x_min) / scale;
                bl_corner.y = (bp.row * cell_size - bp.aabb_y_min) / scale;
                solution_builder.add_item(solution_bin_pos, bp.item_type_id, bl_corner, bp.angle, bp.mirror);
            }
        }
        // Add fixed items to each bin up to the last bin with fixed items.
        // (The main loop above already created bins 0..number_of_bins-1.)
        for (BinPos bin_pos = 0;
                bin_pos <= instance.last_bin_with_fixed_items();
                ++bin_pos) {
            if (bin_pos >= number_of_bins) {
                BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
                solution_builder.add_bin(bin_type_id, 1);
            }
            BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
            const BinType& bin_type = instance.bin_type(bin_type_id);
            for (const FixedItem& fixed_item: bin_type.fixed_items) {
                solution_builder.add_item(
                        bin_pos,
                        fixed_item.item_type_id,
                        fixed_item.bl_corner,
                        fixed_item.angle,
                        fixed_item.mirror,
                        true);
            }
        }
        return solution_builder.build();
    };

    // Solve.
    std::vector<double> milp_solution;
    if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
        Highs highs;
        mathoptsolverscmake::reduce_printout(highs);
        mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
        if (parameters.maximum_number_of_nodes >= 0)
            mathoptsolverscmake::set_node_limit(highs, parameters.maximum_number_of_nodes);
        mathoptsolverscmake::load(highs, model);
        highs.setCallback([
                &instance,
                &parameters,
                &output,
                &algorithm_formatter,
                &retrieve_solution](
                    const int callback_type,
                    const std::string& message,
                    const HighsCallbackOutput* highs_output,
                    HighsCallbackInput* highs_input,
                    void*)
                {
                    // Check bound (Knapsack only).
                    if (instance.objective() == Objective::Knapsack) {
                        if (!highs_output->mip_solution.empty()) {
                            // Retrieve solution.
                            double milp_objective_value = highs_output->mip_primal_bound;
                            if (shape::strictly_lesser(output.solution_pool.best().profit(), milp_objective_value)) {
                                Solution solution = retrieve_solution(highs_output->mip_solution);
                                algorithm_formatter.update_solution(solution, "node " + std::to_string(highs_output->mip_node_count));
                            }
                        }
                    }

                    // HiGHS only allows requesting an interrupt from the
                    // dedicated 'kCallbackMipInterrupt' callback: it asserts
                    // internally that 'user_interrupt' is never set from an
                    // "informational" callback type such as this one's other
                    // registration, 'kCallbackMipImprovingSolution' (see
                    // 'HighsCallback::callbackAction' in HiGHS itself).
                    if (callback_type != HighsCallbackType::kCallbackMipInterrupt)
                        return;

                    if (instance.objective() == Objective::Knapsack) {
                        if (!shape::strictly_greater(highs_output->mip_dual_bound, output.solution_pool.best().profit()))
                            highs_input->user_interrupt = 1;
                    }

                    // Check end.
                    if (parameters.timer.needs_to_end()
                            || algorithm_formatter.end_boolean()) {
                        highs_input->user_interrupt = 1;
                    }
                },
                nullptr);
        HighsStatus highs_status;
        highs_status = highs.startCallback(HighsCallbackType::kCallbackMipImprovingSolution);
        highs_status = highs.startCallback(HighsCallbackType::kCallbackMipInterrupt);
        mathoptsolverscmake::solve(highs);
        milp_solution = mathoptsolverscmake::get_solution(highs);
#else
        throw std::invalid_argument(FUNC_SIGNATURE);
#endif
    } else {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }

    return retrieve_solution(milp_solution);
}

/**
 * Partition a polygon contained in a shape into convex parts.
 *
 * If the shape has circular arcs, they are replaced by line segments inside
 * the shape.
 */
std::vector<Shape> compute_inner_convex_parts(
        const ShapeWithHoles& shape,
        LengthDbl segment_length)
{
    if (shape.is_polygon())
        return shape::compute_convex_partition(shape);

    ShapeWithHoles outline = shape::approximate_shape_by_line_segments(
            shape.shape,
            segment_length,
            false);
    if (outline.shape.elements.empty())
        return {};
    shape::MultiShapeWithHoles polygons;
    polygons.shapes_with_holes.push_back(outline);
    if (!shape.holes.empty()) {
        shape::MultiShapeWithHoles holes;
        for (const Shape& hole: shape.holes) {
            holes.shapes_with_holes.push_back(shape::approximate_shape_by_line_segments(
                        hole,
                        segment_length,
                        true));
        }
        polygons = shape::compute_difference(polygons, holes);
    }
    std::vector<Shape> parts;
    for (const ShapeWithHoles& polygon: polygons.shapes_with_holes)
        for (const Shape& part: shape::compute_convex_partition(polygon))
            parts.push_back(part);
    return parts;
}

struct BoundCoefficient
{
    /** Column of the cell, relative to the cell of the placement. */
    shape::ColumnId column;

    /** Row of the cell, relative to the cell of the placement. */
    shape::RowId row;

    /** Lower bound on the fraction of the cell covered by the item. */
    double value;
};

struct BoundItemTypeRotation
{
    ItemTypeId item_type_id;

    ItemTypeRotation rotation;

    /** Bounding box of the item in this rotation. */
    AxisAlignedBoundingBox aabb;

    /** Area of the item, as a fraction of the cell area. */
    double area = 0.0;

    /** Non-zero coefficients of the placements in the cells. */
    std::vector<BoundCoefficient> coefficients;
};

/**
 * Compute the coefficients of the placements of an item type in a rotation in
 * the cells of the relaxation.
 *
 * With the bottom-left corner of the bounding box of the item at the origin,
 * a convex part covers a fraction A(c, r) of each cell (c, r). When the item
 * is moved to a corner (i * cell_size, j * cell_size), i, j in {0, 1}, it
 * covers a fraction A(c - i, r - j) of cell (c, r). The minimum over the
 * positions in the cell is reached at one of these corners.
 */
BoundItemTypeRotation compute_bound_item_type_rotation(
        const Instance& instance,
        ItemTypeId item_type_id,
        const ItemTypeRotation& rotation,
        LengthDbl cell_size)
{
    const ItemType& item_type = instance.item_type(item_type_id);

    BoundItemTypeRotation bound_item_type_rotation;
    bound_item_type_rotation.item_type_id = item_type_id;
    bound_item_type_rotation.rotation = rotation;

    std::vector<ShapeWithHoles> shapes;
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)item_type.shapes.size();
            ++item_shape_pos) {
        shapes.push_back(instance.item_shape_scaled(item_type_id, item_shape_pos, rotation.angle, rotation.mirror));
        bound_item_type_rotation.aabb = merge(
                bound_item_type_rotation.aabb,
                shapes.back().compute_min_max());
    }
    const AxisAlignedBoundingBox& aabb = bound_item_type_rotation.aabb;

    // Cells reached by the item, from column/row -1 to cover rounding errors.
    shape::ColumnId number_of_columns = (shape::ColumnId)std::ceil((aabb.x_max - aabb.x_min) / cell_size) + 3;
    shape::RowId number_of_rows = (shape::RowId)std::ceil((aabb.y_max - aabb.y_min) / cell_size) + 3;
    std::vector<double> coefficients(number_of_columns * number_of_rows, 0.0);

    for (ShapeWithHoles& shape: shapes) {
        bound_item_type_rotation.area += shape.compute_area() / (cell_size * cell_size);
        shape.shift(-aabb.x_min, -aabb.y_min);
        for (const Shape& part: compute_inner_convex_parts(shape, cell_size / 4)) {
            ShapeWithHoles part_with_holes;
            part_with_holes.shape = part;
            shape::RasterizedGrid grid = shape::rasterization(part_with_holes, cell_size, cell_size);
            auto coverage = [&grid](shape::ColumnId column, shape::RowId row)
            {
                if (column < grid.column_offset
                        || column >= grid.column_offset + grid.number_of_columns
                        || row < grid.row_offset
                        || row >= grid.row_offset + grid.number_of_rows) {
                    return 0.0;
                }
                return grid.at(column, row).coverage;
            };
            for (shape::ColumnId column = grid.column_offset + 1;
                    column < grid.column_offset + grid.number_of_columns;
                    ++column) {
                for (shape::RowId row = grid.row_offset + 1;
                        row < grid.row_offset + grid.number_of_rows;
                        ++row) {
                    double value = (std::min)({
                            coverage(column, row),
                            coverage(column - 1, row),
                            coverage(column, row - 1),
                            coverage(column - 1, row - 1)});
                    if (value <= 0.0)
                        continue;
                    // Dropping a coefficient keeps the relaxation valid.
                    if (column + 1 < 0 || column + 1 >= number_of_columns
                            || row + 1 < 0 || row + 1 >= number_of_rows) {
                        continue;
                    }
                    coefficients[(column + 1) * number_of_rows + (row + 1)] += value;
                }
            }
        }
    }

    for (shape::ColumnId column = 0; column < number_of_columns; ++column) {
        for (shape::RowId row = 0; row < number_of_rows; ++row) {
            double value = coefficients[column * number_of_rows + row];
            if (value <= 0.0)
                continue;
            BoundCoefficient coefficient;
            coefficient.column = column - 1;
            coefficient.row = row - 1;
            coefficient.value = value;
            bound_item_type_rotation.coefficients.push_back(coefficient);
        }
    }
    return bound_item_type_rotation;
}

/**
 * Solve the relaxation for a given cell size.
 *
 * Update the knapsack bound (objective 'Knapsack') or mark the instance as
 * infeasible if the relaxation is infeasible.
 */
void solve_milp_raster_bound_for_cell_size(
        const Instance& instance,
        const std::vector<std::vector<ItemTypeRotation>>& bound_rotations,
        const MilpRasterParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        LengthDbl cell_size)
{
    const LengthDbl scale = instance.parameters().scale_value;
    // Slack on the capacities of the cells (fractions of the cell area), for
    // the rounding errors of the areas.
    const double capacity_slack = 1e-6;
    // Tolerance on the placement domains, in cells.
    const double domain_tolerance = 1e-6;

    // The model may be large, so check the end while building it.
    auto needs_to_end = [&parameters, &algorithm_formatter]()
    {
        return parameters.timer.needs_to_end()
            || algorithm_formatter.end_boolean();
    };

    std::vector<BoundItemTypeRotation> bound_item_type_rotations;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.copies - item_type.copies_fixed <= 0)
            continue;
        for (const ItemTypeRotation& rotation: bound_rotations[item_type_id]) {
            if (needs_to_end())
                return;
            bound_item_type_rotations.push_back(compute_bound_item_type_rotation(
                        instance,
                        item_type_id,
                        rotation,
                        cell_size));
        }
    }

    mathoptsolverscmake::MathOptModel model;
    model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;
    std::vector<std::vector<PlacementId>> item_type_variables(instance.number_of_item_types());

    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos) {
        BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
        const BinType& bin_type = instance.bin_type(bin_type_id);

        // Capacities of the cells: area of the cell inside the bin minus the
        // area of the fixed items in it.
        ShapeWithHoles bin_shape;
        bin_shape.shape = bin_type.shape_scaled;
        shape::RasterizedGrid bin_grid = shape::rasterization(bin_shape, cell_size, cell_size);
        std::vector<double> capacities(bin_grid.number_of_columns * bin_grid.number_of_rows);
        for (shape::ColumnId column = 0; column < bin_grid.number_of_columns; ++column) {
            for (shape::RowId row = 0; row < bin_grid.number_of_rows; ++row) {
                capacities[column * bin_grid.number_of_rows + row] = bin_grid.at(
                        bin_grid.column_offset + column,
                        bin_grid.row_offset + row).coverage;
            }
        }
        if (bin_pos <= instance.last_bin_with_fixed_items()) {
            for (const FixedItem& fixed_item: bin_type.fixed_items) {
                const ItemType& item_type = instance.item_type(fixed_item.item_type_id);
                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type.shapes.size();
                        ++item_shape_pos) {
                    ShapeWithHoles transformed = instance.item_shape_scaled(
                            fixed_item.item_type_id,
                            item_shape_pos,
                            fixed_item.angle,
                            fixed_item.mirror);
                    transformed.shift(
                            fixed_item.bl_corner.x * scale,
                            fixed_item.bl_corner.y * scale);
                    shape::RasterizedGrid raster_grid = shape::rasterization(transformed, cell_size, cell_size);
                    for (shape::ColumnId column = raster_grid.column_offset;
                            column < raster_grid.column_offset + raster_grid.number_of_columns;
                            ++column) {
                        for (shape::RowId row = raster_grid.row_offset;
                                row < raster_grid.row_offset + raster_grid.number_of_rows;
                                ++row) {
                            shape::ColumnId c = column - bin_grid.column_offset;
                            shape::RowId r = row - bin_grid.row_offset;
                            if (c < 0 || c >= bin_grid.number_of_columns
                                    || r < 0 || r >= bin_grid.number_of_rows) {
                                continue;
                            }
                            capacities[c * bin_grid.number_of_rows + r] -= raster_grid.at(column, row).coverage;
                        }
                    }
                }
            }
        }

        // Placements.
        std::vector<std::pair<PlacementId, double>> bin_variables;
        std::vector<std::vector<std::pair<PlacementId, double>>> cell_variables(
                bin_grid.number_of_columns * bin_grid.number_of_rows);
        LengthDbl bin_spacing = instance.bin_spacing_scaled(bin_type_id);
        for (const BoundItemTypeRotation& bound_item_type_rotation: bound_item_type_rotations) {
            if (needs_to_end())
                return;
            const ItemType& item_type = instance.item_type(bound_item_type_rotation.item_type_id);
            const AxisAlignedBoundingBox& aabb = bound_item_type_rotation.aabb;
            LengthDbl x_min = bin_type.aabb_scaled.x_min + bin_spacing;
            LengthDbl x_max = bin_type.aabb_scaled.x_max - bin_spacing - (aabb.x_max - aabb.x_min);
            LengthDbl y_min = bin_type.aabb_scaled.y_min + bin_spacing;
            LengthDbl y_max = bin_type.aabb_scaled.y_max - bin_spacing - (aabb.y_max - aabb.y_min);
            if (x_max / cell_size < x_min / cell_size - domain_tolerance
                    || y_max / cell_size < y_min / cell_size - domain_tolerance) {
                continue;
            }
            shape::ColumnId column_start = (shape::ColumnId)std::floor(x_min / cell_size - domain_tolerance);
            shape::ColumnId column_end = (shape::ColumnId)std::floor(x_max / cell_size + domain_tolerance);
            shape::RowId row_start = (shape::RowId)std::floor(y_min / cell_size - domain_tolerance);
            shape::RowId row_end = (shape::RowId)std::floor(y_max / cell_size + domain_tolerance);
            for (shape::ColumnId column = column_start; column <= column_end; ++column) {
                for (shape::RowId row = row_start; row <= row_end; ++row) {
                    PlacementId variable_id = (PlacementId)model.variables_lower_bounds.size();
                    model.variables_lower_bounds.push_back(0.0);
                    model.variables_upper_bounds.push_back(
                            (double)(item_type.copies - item_type.copies_fixed));
                    model.variables_types.push_back(mathoptsolverscmake::VariableType::Integer);
                    model.objective_coefficients.push_back(
                            (instance.objective() == Objective::Knapsack)?
                            item_type.profit: 0.0);
                    item_type_variables[bound_item_type_rotation.item_type_id].push_back(variable_id);
                    bin_variables.push_back({variable_id, bound_item_type_rotation.area});
                    for (const BoundCoefficient& coefficient: bound_item_type_rotation.coefficients) {
                        // The cells outside of the grid are outside of the
                        // bin; dropping their constraints keeps the
                        // relaxation valid.
                        shape::ColumnId c = column + coefficient.column - bin_grid.column_offset;
                        shape::RowId r = row + coefficient.row - bin_grid.row_offset;
                        if (c < 0 || c >= bin_grid.number_of_columns
                                || r < 0 || r >= bin_grid.number_of_rows) {
                            continue;
                        }
                        cell_variables[c * bin_grid.number_of_rows + r].push_back({variable_id, coefficient.value});
                    }
                }
            }
        }

        // Area constraint: the coefficients of the cell capacity constraints
        // underestimate the areas of the items, so this one isn't implied by
        // them.
        double bin_capacity = capacity_slack;
        for (double capacity: capacities)
            bin_capacity += (std::max)(capacity, 0.0);
        if (!bin_variables.empty()) {
            model.constraints_starts.push_back(model.elements_variables.size());
            model.constraints_lower_bounds.push_back(0.0);
            model.constraints_upper_bounds.push_back(bin_capacity);
            for (const auto& element: bin_variables) {
                model.elements_variables.push_back(element.first);
                model.elements_coefficients.push_back(element.second);
            }
        }

        // Cell capacity constraints.
        for (Counter cell_pos = 0; cell_pos < (Counter)cell_variables.size(); ++cell_pos) {
            double capacity = (std::max)(capacities[cell_pos], 0.0) + capacity_slack;
            double maximum_consumption = 0.0;
            for (const auto& element: cell_variables[cell_pos])
                maximum_consumption += element.second * model.variables_upper_bounds[element.first];
            if (maximum_consumption <= capacity)
                continue;
            model.constraints_starts.push_back(model.elements_variables.size());
            model.constraints_lower_bounds.push_back(0.0);
            model.constraints_upper_bounds.push_back(capacity);
            for (const auto& element: cell_variables[cell_pos]) {
                model.elements_variables.push_back(element.first);
                model.elements_coefficients.push_back(element.second);
            }
        }
    }

    // Copy constraints.
    bool infeasible = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemPos remaining_copies = item_type.copies - item_type.copies_fixed;
        ItemPos remaining_copies_min = (std::max)(
                (ItemPos)0,
                item_type.copies_min - item_type.copies_fixed);
        if (item_type_variables[item_type_id].empty()) {
            if (remaining_copies_min > 0)
                infeasible = true;
            continue;
        }
        model.constraints_starts.push_back(model.elements_variables.size());
        model.constraints_lower_bounds.push_back((double)remaining_copies_min);
        model.constraints_upper_bounds.push_back((double)remaining_copies);
        for (PlacementId variable_id: item_type_variables[item_type_id]) {
            model.elements_variables.push_back(variable_id);
            model.elements_coefficients.push_back(1.0);
        }
    }

    // Profit of the fixed items, and worst case of the 'penalize' resources
    // with a negative penalty, which the relaxation doesn't consider.
    Profit profit_offset = negative_penalty_sum(instance);
    for (BinPos bin_pos = 0;
            bin_pos <= instance.last_bin_with_fixed_items();
            ++bin_pos) {
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(bin_pos));
        for (const FixedItem& fixed_item: bin_type.fixed_items)
            profit_offset += instance.item_type(fixed_item.item_type_id).profit;
    }

    if (infeasible) {
        algorithm_formatter.update_is_proven_infeasible();
        return;
    }
    if (needs_to_end())
        return;
    if (model.variables_lower_bounds.empty()) {
        if (instance.objective() == Objective::Knapsack)
            algorithm_formatter.update_knapsack_bound(profit_offset);
        return;
    }
    if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
        Highs highs;
        mathoptsolverscmake::reduce_printout(highs);
        mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
        if (parameters.maximum_number_of_nodes >= 0)
            mathoptsolverscmake::set_node_limit(highs, parameters.maximum_number_of_nodes);
        mathoptsolverscmake::load(highs, model);
        highs.setCallback([
                &instance,
                &algorithm_formatter,
                &needs_to_end,
                profit_offset](
                    const int callback_type,
                    const std::string& message,
                    const HighsCallbackOutput* highs_output,
                    HighsCallbackInput* highs_input,
                    void*)
                {
                    // The interrupt callbacks of the simplex and of the
                    // interior point method are needed to stop while
                    // solving the LP relaxation of a large model.
                    if (callback_type != HighsCallbackType::kCallbackMipInterrupt
                            && callback_type != HighsCallbackType::kCallbackSimplexInterrupt
                            && callback_type != HighsCallbackType::kCallbackIpmInterrupt) {
                        return;
                    }

                    if (callback_type == HighsCallbackType::kCallbackMipInterrupt
                            && instance.objective() == Objective::Knapsack
                            && std::isfinite(highs_output->mip_dual_bound)) {
                        algorithm_formatter.update_knapsack_bound(
                                highs_output->mip_dual_bound + profit_offset);
                    }

                    // Check end.
                    if (needs_to_end())
                        highs_input->user_interrupt = 1;
                },
                nullptr);
        highs.startCallback(HighsCallbackType::kCallbackMipInterrupt);
        highs.startCallback(HighsCallbackType::kCallbackSimplexInterrupt);
        highs.startCallback(HighsCallbackType::kCallbackIpmInterrupt);
        mathoptsolverscmake::solve(highs);
        if (highs.getModelStatus() == HighsModelStatus::kInfeasible
                || highs.getModelStatus() == HighsModelStatus::kUnboundedOrInfeasible) {
            algorithm_formatter.update_is_proven_infeasible();
        } else if (instance.objective() == Objective::Knapsack) {
            double bound = mathoptsolverscmake::get_bound(highs);
            if (std::isfinite(bound))
                algorithm_formatter.update_knapsack_bound(bound + profit_offset);
        }
#else
        throw std::invalid_argument(FUNC_SIGNATURE);
#endif
    } else {
        throw std::invalid_argument(FUNC_SIGNATURE);
    }
}

}

MilpRasterOutput packingsolver::irregular::milp_raster(
        const Instance& instance,
        const MilpRasterParameters& parameters)
{
    MilpRasterOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.objective() != Objective::Knapsack
            && instance.objective() != Objective::Feasibility) {
        throw std::invalid_argument(FUNC_SIGNATURE + ": unsupported objective.");
    }

    LengthDbl gcd = -1;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (const ItemShape& item_shape: item_type.shapes) {
            for (const shape::ShapeElement& element: item_shape.shape_scaled.shape.elements) {
                gcd = compute_gcd(gcd, std::abs(element.start.x));
                if (equal(gcd, 0.0))
                    break;
                gcd = compute_gcd(gcd, std::abs(element.start.y));
                if (equal(gcd, 0.0))
                    break;
            }
            if (equal(gcd, 0.0))
                break;
            for (const Shape& hole: item_shape.shape_scaled.holes) {
                for (const shape::ShapeElement& element: hole.elements) {
                    gcd = compute_gcd(gcd, element.start.x);
                    if (equal(gcd, 0.0))
                        break;
                    gcd = compute_gcd(gcd, element.start.y);
                    if (equal(gcd, 0.0))
                        break;
                }
                if (equal(gcd, 0.0))
                    break;
            }
            if (equal(gcd, 0.0))
                break;
        }
        if (equal(gcd, 0.0))
            break;
    }
    //std::cout << "gcd " << gcd << std::endl;
    LengthDbl cell_size = 100;
    if (!equal(gcd, 0.0)) {
        cell_size = gcd;
        while (cell_size <= 100)
            cell_size *= 2;
    }

    const std::vector<std::vector<std::vector<ItemTypeRotation>>> item_type_rotations = compute_item_type_rotations(instance);

    // Rotations of the relaxation: all the allowed rotations, so the
    // relaxation can't be used if one of them is a range of angles.
    bool use_bound = parameters.use_bound
        && parameters.optimization_mode == OptimizationMode::Exact;
    std::vector<std::vector<ItemTypeRotation>> bound_rotations(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types()
            && use_bound;
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.copies - item_type.copies_fixed <= 0)
            continue;
        if (!item_type.has_only_discrete_rotations()) {
            use_bound = false;
            break;
        }
        for (const AllowedRotation& allowed_rotation: item_type.allowed_rotations) {
            ItemTypeRotation rotation;
            rotation.angle = allowed_rotation.start_angle;
            rotation.mirror = allowed_rotation.mirror;
            bound_rotations[item_type_id].push_back(rotation);
        }
    }

    auto needs_to_end = [&parameters, &algorithm_formatter](
            Counter number_of_cell_sizes)
    {
        return parameters.timer.needs_to_end()
            || algorithm_formatter.end_boolean()
            || (parameters.maximum_number_of_cell_sizes != -1
                    && number_of_cell_sizes >= parameters.maximum_number_of_cell_sizes);
    };

    std::thread bound_thread;
    std::exception_ptr bound_exception_ptr;
    if (use_bound) {
        bound_thread = std::thread([
                &instance,
                &parameters,
                &algorithm_formatter,
                &bound_rotations,
                &bound_exception_ptr,
                &needs_to_end,
                cell_size]()
        {
            try {
                LengthDbl bound_cell_size = cell_size;
                for (Counter number_of_cell_sizes = 0;
                        !needs_to_end(number_of_cell_sizes);
                        ++number_of_cell_sizes, bound_cell_size /= 2) {
                    solve_milp_raster_bound_for_cell_size(
                            instance,
                            bound_rotations,
                            parameters,
                            algorithm_formatter,
                            bound_cell_size);
                }
            } catch (...) {
                bound_exception_ptr = std::current_exception();
            }
        });
    }

    for (Counter number_of_cell_sizes = 0;
            !needs_to_end(number_of_cell_sizes);
            ++number_of_cell_sizes, cell_size /= 2) {
        Solution solution = solve_milp_raster_for_cell_size(
                instance,
                item_type_rotations,
                parameters,
                output,
                algorithm_formatter,
                cell_size,
                instance.number_of_bins());
        algorithm_formatter.update_solution(solution, "milp-raster cs=" + std::to_string(cell_size));
    }

    if (bound_thread.joinable())
        bound_thread.join();
    if (bound_exception_ptr)
        std::rethrow_exception(bound_exception_ptr);

    algorithm_formatter.end();
    return output;
}



#include "irregular/milp_raster.hpp"

#include "irregular/rotations.hpp"
#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "shape/rasterization.hpp"
//#include "shape/writer.hpp"
#include "mathoptsolverscmake/milp.hpp"

#include <algorithm>
#include <cmath>

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
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations,
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

        LengthDbl x_min_scaled = bin_type.aabb.x_min * scale;
        LengthDbl x_max_scaled = bin_type.aabb.x_max * scale;
        LengthDbl y_min_scaled = bin_type.aabb.y_min * scale;
        LengthDbl y_max_scaled = bin_type.aabb.y_max * scale;

        data.col_shift = (shape::ColumnId)std::floor(x_min_scaled / cell_size);
        if (shape::strictly_greater(x_min_scaled, data.col_shift * cell_size))
            data.col_shift++;
        data.col_max = (shape::ColumnId)std::floor(x_max_scaled / cell_size);
        if (shape::strictly_lesser(x_max_scaled, (data.col_max + 1) * cell_size))
            data.col_max--;
        data.row_shift = (shape::RowId)std::floor(y_min_scaled / cell_size);
        if (shape::strictly_greater(y_min_scaled, data.row_shift * cell_size))
            data.row_shift++;
        data.row_max = (shape::RowId)std::floor(y_max_scaled / cell_size);
        if (shape::strictly_lesser(y_max_scaled, (data.row_max + 1) * cell_size))
            data.row_max--;
        data.num_cols = data.col_max - data.col_shift + 1;
        data.num_rows = data.row_max - data.row_shift + 1;

        data.available.assign(data.num_cols, std::vector<bool>(data.num_rows, true));

        for (Counter border_pos = 0;
                border_pos < (Counter)bin_type.borders.size();
                ++border_pos) {
            const ShapeWithHoles& border_shape = bin_type.borders[border_pos].shape_scaled;
            std::vector<shape::IntersectedCell> border_cells = shape::rasterization(border_shape, cell_size, cell_size);
            //writer.add_shape_with_holes(border_shape, "Border " + std::to_string(border_pos));
            //writer.add_shape_with_holes(cells_to_shapes(border_cells, cell_size, cell_size).front(), "Border " + std::to_string(border_pos));
            for (const shape::IntersectedCell& ic: border_cells) {
                shape::ColumnId c = ic.cell.column - data.col_shift;
                shape::RowId r = ic.cell.row - data.row_shift;
                if (c >= 0 && c < data.num_cols && r >= 0 && r < data.num_rows)
                    data.available[c][r] = false;
            }
        }

        for (Counter defect_pos = 0;
                defect_pos < (Counter)bin_type.defects.size();
                ++defect_pos) {
            const ShapeWithHoles& defect_shape = bin_type.defects[defect_pos].shape_scaled;
            std::vector<shape::IntersectedCell> defect_cells = shape::rasterization(defect_shape, cell_size, cell_size);
            //writer.add_shape_with_holes(defect_shape, "Defect " + std::to_string(defect_pos));
            //writer.add_shape_with_holes(cells_to_shapes(defect_cells, cell_size, cell_size).front(), "Defect " + std::to_string(defect_pos));
            for (const shape::IntersectedCell& ic: defect_cells) {
                shape::ColumnId c = ic.cell.column - data.col_shift;
                shape::RowId r = ic.cell.row - data.row_shift;
                if (c >= 0 && c < data.num_cols && r >= 0 && r < data.num_rows)
                    data.available[c][r] = false;
            }
        }

        // Mark cells occupied by fixed items of this bin type.
        data.fixed_items.assign(data.num_cols, std::vector<bool>(data.num_rows, false));
        for (const FixedItem& fixed_item: bin_type.fixed_items) {
            const ItemType& item_type = instance.item_type(fixed_item.item_type_id);
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                const ItemShape& item_shape = item_type.shapes[item_shape_pos];
                ShapeWithHoles transformed = fixed_item.mirror?
                    item_shape.shape_scaled.axial_symmetry_y_axis():
                    item_shape.shape_scaled;
                transformed = transformed.rotate(fixed_item.angle);
                transformed.shift(
                        fixed_item.bl_corner.x * scale,
                        fixed_item.bl_corner.y * scale);
                std::vector<shape::IntersectedCell> raster_cells = shape::rasterization(transformed, cell_size, cell_size);
                for (const shape::IntersectedCell& ic: raster_cells) {
                    shape::ColumnId c = ic.cell.column - data.col_shift;
                    shape::RowId r = ic.cell.row - data.row_shift;
                    if (c >= 0 && c < data.num_cols && r >= 0 && r < data.num_rows)
                        data.fixed_items[c][r] = true;
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

            for (const ItemTypeRotation& rotation: item_type_rotations[item_type_id]) {
                // Compute AABB of all item shapes after applying mirror then
                // rotation, matching the order used in convert_shape.
                AxisAlignedBoundingBox combined_aabb;
                for (const ItemShape& item_shape: item_type.shapes) {
                    ShapeWithHoles transformed = rotation.mirror?
                        item_shape.shape_scaled.axial_symmetry_y_axis():
                        item_shape.shape_scaled;
                    transformed = transformed.rotate(rotation.angle);
                    AxisAlignedBoundingBox shape_aabb = transformed.compute_min_max();
                    combined_aabb = merge(combined_aabb, shape_aabb);
                }

                // Rasterize each item shape (shifted to origin) and collect cells.
                std::vector<std::pair<shape::ColumnId, shape::RowId>> placement_cells;
                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type.shapes.size();
                        ++item_shape_pos) {
                    const ItemShape& item_shape = item_type.shapes[item_shape_pos];
                    ShapeWithHoles transformed = (rotation.mirror)?
                        item_shape.shape_scaled.axial_symmetry_y_axis():
                        item_shape.shape_scaled;
                    transformed = transformed.rotate(rotation.angle);
                    transformed.shift(-combined_aabb.x_min, -combined_aabb.y_min);
                    std::vector<shape::IntersectedCell> raster_cells = shape::rasterization(transformed, cell_size, cell_size);
                    for (const shape::IntersectedCell& ic: raster_cells)
                        placement_cells.push_back(std::make_pair(ic.cell.column, ic.cell.row));
                }

                if (placement_cells.empty())
                    continue;

                std::sort(placement_cells.begin(), placement_cells.end());
                placement_cells.erase(std::unique(placement_cells.begin(), placement_cells.end()), placement_cells.end());

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

                        for (const std::pair<shape::ColumnId, shape::RowId>& cr: placement_cells) {
                            shape::ColumnId bc = col + cr.first - data.col_shift;
                            shape::RowId br = row + cr.second - data.row_shift;
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
    mathoptsolverscmake::MilpModel model;
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

    // Copy constraints: for each item type, total placements across all bins <= copies - copies_fixed.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemPos remaining_copies = item_type.copies - item_type.copies_fixed;
        model.constraints_starts.push_back(model.elements_variables.size());
        if (instance.objective() == Objective::Feasibility) {
            model.constraints_lower_bounds.push_back(remaining_copies);
        } else {
            model.constraints_lower_bounds.push_back(0.0);
        }
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
        Solution solution(instance);
        for (BinPos bin_pos = 0; bin_pos < number_of_bins; ++bin_pos) {
            BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
            const BinTypeData& data = bin_type_data[bin_type_id];
            BinPos solution_bin_pos = solution.add_bin(bin_type_id, 1);
            for (PlacementId placement_id = 0;
                    placement_id < (PlacementId)data.base_placements.size();
                    ++placement_id) {
                if (milp_solution[global_offset[bin_pos] + placement_id] < 0.5)
                    continue;
                const BasePlacement& bp = data.base_placements[placement_id];
                Point bl_corner;
                bl_corner.x = (bp.col * cell_size - bp.aabb_x_min) / scale;
                bl_corner.y = (bp.row * cell_size - bp.aabb_y_min) / scale;
                solution.add_item(solution_bin_pos, bp.item_type_id, bl_corner, bp.angle, bp.mirror);
            }
        }
        // Add fixed items to each bin up to the last bin with fixed items.
        for (BinPos bin_pos = 0;
                bin_pos <= instance.last_bin_with_fixed_items();
                ++bin_pos) {
            if (bin_pos >= solution.number_of_bins()) {
                BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
                solution.add_bin(bin_type_id, 1);
            }
            BinTypeId bin_type_id = instance.bin_type_id(bin_pos);
            const BinType& bin_type = instance.bin_type(bin_type_id);
            for (const FixedItem& fixed_item: bin_type.fixed_items) {
                solution.add_item(
                        bin_pos,
                        fixed_item.item_type_id,
                        fixed_item.bl_corner,
                        fixed_item.angle,
                        fixed_item.mirror,
                        true);
            }
        }
        return solution;
    };

    // Solve.
    std::vector<double> milp_solution;
    if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
        Highs highs;
        mathoptsolverscmake::reduce_printout(highs);
        mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
        mathoptsolverscmake::load(highs, model);
        highs.setCallback([
                &instance,
                &parameters,
                &output,
                &algorithm_formatter,
                &retrieve_solution](
                    const int,
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
                        if (!shape::strictly_greater(highs_output->mip_dual_bound, output.solution_pool.best().profit()))
                            highs_input->user_interrupt = 1;
                    }

                    // Check end.
                    if (parameters.timer.needs_to_end())
                        highs_input->user_interrupt = 1;
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

    const std::vector<std::vector<ItemTypeRotation>> item_type_rotations = compute_item_type_rotations(instance);

    for (; ; cell_size /= 2) {
        if (parameters.timer.needs_to_end())
            break;
        Solution solution = solve_milp_raster_for_cell_size(
                instance,
                item_type_rotations,
                parameters,
                output,
                algorithm_formatter,
                cell_size,
                instance.number_of_bins());
        algorithm_formatter.update_solution(solution, "milp-raster cs=" + std::to_string(cell_size));
        if (algorithm_formatter.end_boolean())
            break;
    }

    algorithm_formatter.end();
    return output;
}

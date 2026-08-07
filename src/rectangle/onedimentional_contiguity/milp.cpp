#include "rectangle/onedimentional_contiguity/milp.hpp"

#include "algorithms/meet_in_the_middle.hpp"

#include "mathoptsolverscmake/mathopt.hpp"
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif

using namespace packingsolver;
using namespace packingsolver::rectangle;
using namespace packingsolver::rectangle::onedimentional_contiguity;

UnitsAndCandidates packingsolver::rectangle::onedimentional_contiguity::build_units_and_candidates(
        const Instance& instance,
        const BinType& bin_type)
{
    UnitsAndCandidates result;
    result.candidates_by_column.assign(bin_type.rect.x, {});
    result.candidates_by_item_type.assign(instance.number_of_item_types(), {});
    result.candidates_by_item_type_and_copy.assign(instance.number_of_item_types(), {});

    // One unit per (item type, copy); 'unit_widths[u]'/'unit_rotations[u]'
    // list every one of its allowed rotations, in parallel.
    std::vector<std::pair<ItemTypeId, ItemPos>> unit_item_type_and_copy;
    std::vector<std::vector<Length>> unit_widths;
    std::vector<std::vector<UnitRotation>> unit_rotations;

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        result.candidates_by_item_type_and_copy[item_type_id].assign(item_type.copies, {});

        for (ItemPos copy = 0; copy < item_type.copies; ++copy) {
            std::vector<Length> widths;
            std::vector<UnitRotation> rotations;

            // One orientation, or two if the item type may rotate.
            std::vector<std::pair<bool, std::pair<Length, Length>>> orientations;
            orientations.push_back({false, {item_type.rect.x, item_type.rect.y}});
            if (!item_type.oriented)
                orientations.push_back({true, {item_type.rect.y, item_type.rect.x}});

            for (const auto& orientation: orientations) {
                bool rotate = orientation.first;
                Length width = orientation.second.first;
                Length height = orientation.second.second;
                if (width > bin_type.rect.x || height > bin_type.rect.y)
                    continue;
                widths.push_back(width);
                rotations.push_back({rotate, height});
            }

            unit_item_type_and_copy.push_back({item_type_id, copy});
            unit_widths.push_back(std::move(widths));
            unit_rotations.push_back(std::move(rotations));
        }
    }

    std::vector<std::vector<std::vector<Length>>> unit_positions
        = minimal_mim_patterns(unit_widths, bin_type.rect.x);

    for (size_t unit_id = 0; unit_id < unit_item_type_and_copy.size(); ++unit_id) {
        ItemTypeId item_type_id = unit_item_type_and_copy[unit_id].first;
        ItemPos copy = unit_item_type_and_copy[unit_id].second;
        for (size_t rotation_id = 0; rotation_id < unit_rotations[unit_id].size(); ++rotation_id) {
            bool rotate = unit_rotations[unit_id][rotation_id].rotate;
            Length width = unit_widths[unit_id][rotation_id];
            Length height = unit_rotations[unit_id][rotation_id].height;
            for (Length x: unit_positions[unit_id][rotation_id]) {
                size_t candidate_id = result.candidates.size();
                result.candidates.push_back({item_type_id, copy, x, rotate, width, height});
                result.candidates_by_item_type_and_copy[item_type_id][copy].push_back(candidate_id);
                result.candidates_by_item_type[item_type_id].push_back(candidate_id);
                for (Length c = x; c < x + width; ++c)
                    result.candidates_by_column[c].push_back(candidate_id);
            }
        }
    }
    return result;
}

NoGoodCut packingsolver::rectangle::onedimentional_contiguity::build_positional_no_good_cut(
        const std::vector<size_t>& selected_candidate_ids)
{
    NoGoodCut cut;
    cut.candidate_ids = selected_candidate_ids;
    cut.upper_bound = (ItemPos)selected_candidate_ids.size() - 1;
    return cut;
}

NoGoodCut packingsolver::rectangle::onedimentional_contiguity::build_covering_cut(
        const Instance& instance,
        const std::vector<Candidate>& selected_units)
{
    UnitsAndCandidates units_and_candidates = build_units_and_candidates(instance, instance.bin_type(0));
    NoGoodCut cut;
    for (const Candidate& selected: selected_units) {
        const std::vector<size_t>& unit_candidates
            = units_and_candidates.candidates_by_item_type_and_copy
                [selected.item_type_id][selected.copy];
        cut.candidate_ids.insert(
                cut.candidate_ids.end(), unit_candidates.begin(), unit_candidates.end());
    }
    cut.upper_bound = (ItemPos)selected_units.size() - 1;
    return cut;
}

void packingsolver::rectangle::onedimentional_contiguity::add_resource_constraints(
        const Instance& instance,
        const UnitsAndCandidates& units_and_candidates,
        mathoptsolverscmake::MathOptModel& model)
{
    const BinType& bin_type = instance.bin_type(0);
    const std::vector<std::vector<std::vector<size_t>>>& candidates_by_item_type_and_copy
        = units_and_candidates.candidates_by_item_type_and_copy;

    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);

        if (resource.penalize) {
            if (instance.objective() != Objective::Knapsack)
                continue;
            if (resource.capacity != 1.0)
                throw std::invalid_argument(FUNC_SIGNATURE);

            // Every (item type, copy) unit the resource involves,
            // flattened - same validation/extraction as
            // 'onedimensional::add_penalize_resource_constraints'.
            std::vector<std::pair<ItemTypeId, ItemPos>> resource_units;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                if (resource.item_consumption(item_type_id, 0) == 0.0)
                    continue;
                ItemPos copies_bound = instance.item_type(item_type_id).copies;
                ItemPos threshold = 0;
                while (threshold <= copies_bound
                        && resource.item_consumption(item_type_id, threshold) == 1.0) {
                    ++threshold;
                }
                if (resource.item_consumption(item_type_id, threshold) != 0.0)
                    throw std::invalid_argument(FUNC_SIGNATURE);
                for (ItemPos copy = 0; copy < threshold; ++copy)
                    resource_units.push_back({item_type_id, copy});
            }
            if (resource_units.size() < 2)
                continue;

            // Every resource unit's own list of candidates (its
            // "presence"), or an empty one if it has none at all here
            // (excluded entirely, e.g. by not fitting the bin in any
            // orientation) - such a unit can never be selected, so every
            // pair involving it is vacuous and simply skipped below.
            std::vector<const std::vector<size_t>*> presence_candidates;
            for (const auto& unit: resource_units) {
                presence_candidates.push_back(
                        &candidates_by_item_type_and_copy[unit.first][unit.second]);
            }

            // psi variable.
            int psi_variable_id = (int)model.variables_lower_bounds.size();
            model.variables_lower_bounds.push_back(0.0);
            model.variables_upper_bounds.push_back(1.0);
            model.variables_types.push_back(mathoptsolverscmake::VariableType::Binary);
            model.objective_coefficients.push_back(-resource.penalty);

            // Pairwise constraints.
            for (size_t a = 0; a < presence_candidates.size(); ++a) {
                if (presence_candidates[a]->empty())
                    continue;
                for (size_t b = a + 1; b < presence_candidates.size(); ++b) {
                    if (presence_candidates[b]->empty())
                        continue;
                    model.constraints_starts.push_back((int)model.elements_variables.size());
                    for (size_t candidate_id: *presence_candidates[a]) {
                        model.elements_variables.push_back((int)candidate_id);
                        model.elements_coefficients.push_back(1.0);
                    }
                    for (size_t candidate_id: *presence_candidates[b]) {
                        model.elements_variables.push_back((int)candidate_id);
                        model.elements_coefficients.push_back(1.0);
                    }
                    model.elements_variables.push_back(psi_variable_id);
                    model.elements_coefficients.push_back(-1.0);
                    model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
                    model.constraints_upper_bounds.push_back(1.0);
                }
            }
        } else {
            // Hard-capacity resource.
            model.constraints_starts.push_back((int)model.elements_variables.size());
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const std::vector<std::vector<size_t>>& unit_candidates_by_copy
                    = candidates_by_item_type_and_copy[item_type_id];
                for (ItemPos copy = 0; copy < (ItemPos)unit_candidates_by_copy.size(); ++copy) {
                    double consumption = resource.item_consumption(item_type_id, copy);
                    if (consumption == 0.0)
                        continue;
                    for (size_t candidate_id: unit_candidates_by_copy[copy]) {
                        model.elements_variables.push_back((int)candidate_id);
                        model.elements_coefficients.push_back(consumption);
                    }
                }
            }
            model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            model.constraints_upper_bounds.push_back(resource.capacity);
        }
    }
}

OnedimensionalContiguityResult packingsolver::rectangle::onedimentional_contiguity::milp(
        const Instance& instance,
        const std::vector<NoGoodCut>& cuts,
        const MilpParameters& parameters)
{
    const BinType& bin_type = instance.bin_type(0);
    UnitsAndCandidates units_and_candidates = build_units_and_candidates(instance, bin_type);
    const std::vector<Candidate>& candidates = units_and_candidates.candidates;
    const std::vector<std::vector<std::vector<size_t>>>& candidates_by_item_type_and_copy
        = units_and_candidates.candidates_by_item_type_and_copy;
    const std::vector<std::vector<size_t>>& candidates_by_item_type = units_and_candidates.candidates_by_item_type;
    const std::vector<std::vector<size_t>>& candidates_by_column = units_and_candidates.candidates_by_column;

    Length bin_height = bin_type.rect.y;

    // An item type that must be packed ('copies_min > 0', e.g. every item
    // type for 'Feasibility') but has no candidate placement at all (it
    // does not fit the bin in any orientation) makes the relaxation
    // trivially infeasible - checked explicitly rather than relying on the
    // MILP solver to detect it, since the resulting row ('0 >= copies_min',
    // with every other row it could combine with just as vacuously true)
    // can end up with literally zero decision variables anywhere in the
    // model, an edge case not every solver reliably reports as infeasible.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (instance.item_type(item_type_id).copies_min > 0
                && candidates_by_item_type[item_type_id].empty()) {
            OnedimensionalContiguityResult result;
            result.infeasible = true;
            return result;
        }
    }

    mathoptsolverscmake::MathOptModel model((int)candidates.size(), 0, 0);
    model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;
    for (size_t candidate_id = 0; candidate_id < candidates.size(); ++candidate_id) {
        model.variables_lower_bounds[candidate_id] = 0.0;
        model.variables_upper_bounds[candidate_id] = 1.0;
        model.variables_types[candidate_id] = mathoptsolverscmake::VariableType::Binary;
        const ItemType& item_type = instance.item_type(candidates[candidate_id].item_type_id);
        model.objective_coefficients[candidate_id]
            = (instance.objective() == Objective::Knapsack)? item_type.profit: 0.0;
    }

    // Each unit is selected at most once.
    for (const std::vector<std::vector<size_t>>& item_type_candidates_by_copy: candidates_by_item_type_and_copy) {
        for (const std::vector<size_t>& unit_candidates: item_type_candidates_by_copy) {
            model.constraints_starts.push_back((int)model.elements_variables.size());
            for (size_t candidate_id: unit_candidates) {
                model.elements_variables.push_back((int)candidate_id);
                model.elements_coefficients.push_back(1.0);
            }
            model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            model.constraints_upper_bounds.push_back(1.0);
        }
    }

    // Symmetry breaking: copy 'copy' of an item type is only selected if
    // copy 'copy - 1' is too. Units of the same item type are fully
    // interchangeable, so among every subset of a given size, one using a
    // "prefix" of copies (0, 1, ..., k - 1) is always exactly as good as
    // any other - this never excludes an optimal solution, only equivalent
    // permutations of one, and shrinks the search space accordingly.
    for (const std::vector<std::vector<size_t>>& item_type_candidates_by_copy: candidates_by_item_type_and_copy) {
        for (ItemPos copy = 1;
                copy < (ItemPos)item_type_candidates_by_copy.size();
                ++copy) {
            model.constraints_starts.push_back((int)model.elements_variables.size());
            for (size_t candidate_id: item_type_candidates_by_copy[copy]) {
                model.elements_variables.push_back((int)candidate_id);
                model.elements_coefficients.push_back(1.0);
            }
            for (size_t candidate_id: item_type_candidates_by_copy[copy - 1]) {
                model.elements_variables.push_back((int)candidate_id);
                model.elements_coefficients.push_back(-1.0);
            }
            model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            model.constraints_upper_bounds.push_back(0.0);
        }
    }

    // Each item type reaches at least its 'copies_min' (skip when 0, the
    // common case: 'Feasibility' always sets 'copies_min == copies', so
    // combined with the per-unit '<= 1' rows above, this forces every one
    // of its units to be selected).
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        ItemPos copies_min = instance.item_type(item_type_id).copies_min;
        if (copies_min <= 0)
            continue;
        model.constraints_starts.push_back((int)model.elements_variables.size());
        for (size_t candidate_id: candidates_by_item_type[item_type_id]) {
            model.elements_variables.push_back((int)candidate_id);
            model.elements_coefficients.push_back(1.0);
        }
        model.constraints_lower_bounds.push_back((double)copies_min);
        model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
    }

    // Height stacked in every column stays within the bin height (the
    // relaxation's only geometric constraint - see the .hpp comment).
    for (const std::vector<size_t>& column_candidates: candidates_by_column) {
        model.constraints_starts.push_back((int)model.elements_variables.size());
        for (size_t candidate_id: column_candidates) {
            model.elements_variables.push_back((int)candidate_id);
            model.elements_coefficients.push_back((double)candidates[candidate_id].height);
        }
        model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
        model.constraints_upper_bounds.push_back((double)bin_height);
    }

    // No-good cuts.
    for (const NoGoodCut& cut: cuts) {
        model.constraints_starts.push_back((int)model.elements_variables.size());
        for (size_t candidate_id: cut.candidate_ids) {
            model.elements_variables.push_back((int)candidate_id);
            model.elements_coefficients.push_back(1.0);
        }
        model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
        model.constraints_upper_bounds.push_back((double)cut.upper_bound);
    }

    // Resources.
    add_resource_constraints(instance, units_and_candidates, model);

    OnedimensionalContiguityResult result;
    if (parameters.solver != mathoptsolverscmake::SolverName::Highs)
        throw std::invalid_argument(FUNC_SIGNATURE);

    // Only retrieving raw results from HiGHS here (whether it proved the
    // model infeasible, and its solution vector if not); interpreting them
    // into 'result' happens below, after '#endif', so that logic is not
    // duplicated (or silently skipped) when HiGHS is not available.
    bool proven_infeasible = false;
    std::vector<double> solution;
#ifdef HIGHS_FOUND
    Highs highs;
    mathoptsolverscmake::reduce_printout(highs);
    mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
    mathoptsolverscmake::load(highs, model);
    mathoptsolverscmake::solve(highs);
    proven_infeasible =
        (highs.getModelStatus() == HighsModelStatus::kInfeasible
         || highs.getModelStatus() == HighsModelStatus::kUnboundedOrInfeasible);
    if (!proven_infeasible)
        solution = mathoptsolverscmake::get_solution(highs);
#else
    throw std::invalid_argument(FUNC_SIGNATURE);
#endif

    if (proven_infeasible) {
        result.infeasible = true;
        return result;
    }
    if (solution.empty()) {
        // Timed out before finding any solution; treat as inconclusive by
        // returning the (feasible, trivially empty) "select nothing"
        // candidate - never sound to report 'infeasible' without a proof.
        return result;
    }
    for (size_t candidate_id = 0; candidate_id < candidates.size(); ++candidate_id) {
        if (solution[candidate_id] > 0.5)
            result.selected_units.push_back(candidates[candidate_id]);
    }
    result.objective_value = model.evaluate_objective(solution);
    return result;
}

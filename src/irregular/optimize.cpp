#include "packingsolver/irregular/optimize.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/branching_scheme.hpp"
#include "algorithms/dichotomic_search.hpp"
#include "algorithms/sequential_value_correction.hpp"
#include "algorithms/column_generation.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

#include "treesearchsolver/iterative_beam_search_2.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

void optimize_onedimensional_bound(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    onedimensional::InstanceBuilder onedim_instance_builder;
    onedim_instance_builder.set_objective(instance.objective());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        onedim_instance_builder.add_bin_type(
                std::ceil(bin_type.area_scaled),
                bin_type.cost,
                bin_type.copies,
                bin_type.copies_min);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        onedim_instance_builder.add_item_type(
                std::floor(item_type.area_scaled),
                item_type.profit,
                item_type.copies);
    }
    onedimensional::Instance onedim_instance = onedim_instance_builder.build();

    // Solve the instance.
    onedimensional::OptimizeParameters onedim_parameters;
    onedim_parameters.verbosity_level = 0;
    onedim_parameters.timer = parameters.timer;
    onedim_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    onedim_parameters.optimization_mode = OptimizationMode::NotAnytime;
    auto onedim_output = optimize(onedim_instance, onedim_parameters);

    std::stringstream ss("1D");
    switch (instance.objective()) {
    case Objective::BinPacking: {
        algorithm_formatter.update_bin_packing_bound(
                onedim_output.bin_packing_bound);
        break;
    } case Objective::Knapsack: {
        algorithm_formatter.update_knapsack_bound(
                onedim_output.knapsack_bound);
        break;
    } case Objective::VariableSizedBinPacking: {
        algorithm_formatter.update_variable_sized_bin_packing_bound(
                onedim_output.variable_sized_bin_packing_bound);
        break;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "objective \""
            << instance.objective() << "\" not supported.";
        throw std::logic_error(ss.str());
    }
    }
}

void optimize_tree_search_worker(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        BranchingScheme::Parameters branching_scheme_parameters,
        treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme> ibs_parameters)
{
    Counter queue_size = 1;
    double maximum_approximation_ratio = parameters.initial_maximum_approximation_ratio;
    std::shared_ptr<BranchingScheme::Node> cutoff = nullptr;
    for (Counter iteration = 0;
            ;
            ++iteration) {

        if (parameters.optimization_mode != OptimizationMode::Anytime) {
            queue_size = parameters.not_anytime_tree_search_queue_size;
            maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        }

        // Run tree search.
        branching_scheme_parameters.maximum_approximation_ratio = maximum_approximation_ratio;
        ibs_parameters.cutoff = cutoff;
        ibs_parameters.minimum_size_of_the_queue = queue_size;
        ibs_parameters.maximum_size_of_the_queue = queue_size;
        if (!parameters.json_search_tree_path.empty()) {
            ibs_parameters.json_search_tree_path = parameters.json_search_tree_path
                + "_guide_" + std::to_string(branching_scheme_parameters.guide_id)
                + "_d_" + std::to_string((int)branching_scheme_parameters.direction);
        }
        BranchingScheme branching_scheme(instance, branching_scheme_parameters);
        auto ibs_output = treesearchsolver::iterative_beam_search_2<BranchingScheme>(
                branching_scheme,
                ibs_parameters);

        // Check end.
        if (ibs_output.optimal)
            break;
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            break;

        if (cutoff != nullptr)
            ibs_output.solution_pool.add(cutoff);
        cutoff = ibs_output.solution_pool.best();

        // Update beam size.
        queue_size = std::max(
                queue_size + 1,
                (NodeId)(queue_size * 1.5));
        // Update maximum approximation ratio.
        maximum_approximation_ratio *= parameters.maximum_approximation_ratio_factor;
    }
}

void optimize_tree_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    std::vector<GuideId> guides;
    if (!parameters.tree_search_guides.empty()) {
        guides = parameters.tree_search_guides;
    } else if (instance.objective() == Objective::Knapsack) {
        guides = {4, 5};
    } else {
        guides = {0, 1};
    }
    //guides = {4};

    std::vector<BranchingScheme::Direction> directions;
    if (instance.objective() == Objective::OpenDimensionX) {
        directions = {
            BranchingScheme::Direction::LeftToRightThenBottomToTop,
            BranchingScheme::Direction::LeftToRightThenTopToBottom,
        };
    } else if (instance.objective() == Objective::OpenDimensionY) {
        directions = {
            BranchingScheme::Direction::BottomToTopThenLeftToRight,
            BranchingScheme::Direction::BottomToTopThenRightToLeft,
        };
    } else if (instance.number_of_bin_types() == 1) {
        if (instance.objective() == Objective::BinPackingWithLeftovers) {
            directions = {
                BranchingScheme::Direction::LeftToRightThenBottomToTop,
                BranchingScheme::Direction::BottomToTopThenLeftToRight,
                BranchingScheme::Direction::LeftToRightThenTopToBottom,
                BranchingScheme::Direction::BottomToTopThenRightToLeft,
            };
        } else {
            directions = {
                BranchingScheme::Direction::LeftToRightThenBottomToTop,
                BranchingScheme::Direction::BottomToTopThenLeftToRight,
                BranchingScheme::Direction::RightToLeftThenTopToBottom,
                BranchingScheme::Direction::TopToBottomThenRightToLeft,
            };
        }
    } else {
        directions = {BranchingScheme::Direction::Any};
    }

    // If all items have free rotations and all bins are squared, we consdier
    // a single direction.
    bool all_items_full_rotation = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.allowed_rotations != std::vector<std::pair<Angle, Angle>>{{0, 360}}) {
            all_items_full_rotation = false;
            break;
        }
    }
    bool all_bins_squared = true;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (!bin_type.shape_scaled.is_square()) {
            all_bins_squared = false;
            break;
        }
    }
    if (all_items_full_rotation && all_bins_squared) {
        directions = {BranchingScheme::Direction::LeftToRightThenBottomToTop};
    }

    std::vector<double> growth_factors = {1.5};
    if (guides.size() * directions.size() * 2 <= 4)
        growth_factors = {1.33, 1.5};
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        growth_factors = {1.5};
    //growth_factors = {1.5};

    std::vector<BranchingScheme::Parameters> branching_scheme_parameters_list;
    std::vector<BranchingScheme> branching_schemes;
    std::vector<treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme>> ibs_parameters_list;
    std::vector<irregular::Output> outputs;
    for (double growth_factor: growth_factors) {
        for (GuideId guide_id: guides) {
            for (BranchingScheme::Direction direction: directions) {
                //std::cout << growth_factor << " " << guide_id << " " << direction << std::endl;
                BranchingScheme::Parameters branching_scheme_parameters;
                branching_scheme_parameters.guide_id = guide_id;
                branching_scheme_parameters.direction = direction;
                if (parameters.optimization_mode == OptimizationMode::Anytime) {
                    branching_scheme_parameters.maximum_approximation_ratio
                        = parameters.initial_maximum_approximation_ratio;
                } else {
                    branching_scheme_parameters.maximum_approximation_ratio
                        = parameters.not_anytime_maximum_approximation_ratio;
                }
                branching_scheme_parameters_list.push_back(branching_scheme_parameters);
                branching_schemes.push_back(BranchingScheme(instance, branching_scheme_parameters));
                treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme> ibs_parameters;
                ibs_parameters.verbosity_level = 0;
                ibs_parameters.timer = parameters.timer;
                ibs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                ibs_parameters.growth_factor = growth_factor;
                if (parameters.optimization_mode != OptimizationMode::Anytime) {
                    ibs_parameters.minimum_size_of_the_queue
                        = parameters.not_anytime_tree_search_queue_size;
                    ibs_parameters.maximum_size_of_the_queue
                        = parameters.not_anytime_tree_search_queue_size;
                }
                ibs_parameters_list.push_back(ibs_parameters);
                outputs.push_back(irregular::Output(instance));
            }
        }
    }

    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>>> ibs_outputs(branching_schemes.size(), nullptr);
    std::forward_list<std::exception_ptr> exception_ptr_list;
    for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeDeterministic) {
            ibs_parameters_list[i].new_solution_callback
                = [&algorithm_formatter, &branching_schemes, i](
                        const treesearchsolver::Output<BranchingScheme>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>&>(tss_output);
                    Solution solution = branching_schemes[i].to_solution(
                            tssibs_output.solution_pool.best());
                    std::stringstream ss;
                    ss << "TS g " << branching_schemes[i].parameters().guide_id
                        << " d " << (int)branching_schemes[i].parameters().direction
                        << " q " << tssibs_output.maximum_size_of_the_queue;
                    algorithm_formatter.update_solution(solution, ss.str());
                };
        } else {
            ibs_parameters_list[i].new_solution_callback
                = [&outputs, &branching_schemes, i](
                        const treesearchsolver::Output<BranchingScheme>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>&>(tss_output);
                    Solution solution = branching_schemes[i].to_solution(
                            tssibs_output.solution_pool.best());
                    outputs[i].solution_pool.add(solution);
                };
        }
        exception_ptr_list.push_front(std::exception_ptr());
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential) {
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_tree_search_worker), optimize_tree_search_worker>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter),
                        branching_scheme_parameters_list[i],
                        ibs_parameters_list[i]));
        } else {
            try {
                optimize_tree_search_worker(
                        instance,
                        parameters,
                        algorithm_formatter,
                        branching_scheme_parameters_list[i],
                        ibs_parameters_list[i]);
            } catch (...) {
                exception_ptr_list.front() = std::current_exception();
            }
        }
    }
    for (Counter i = 0; i < (Counter)threads.size(); ++i)
        threads[i].join();
    for (const std::exception_ptr& exception_ptr: exception_ptr_list)
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);
    if (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic) {
        for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
            std::stringstream ss;
            ss << "TS g " << branching_schemes[i].parameters().guide_id
                << " d " << (int)branching_schemes[i].parameters().direction;
            algorithm_formatter.update_solution(outputs[i].solution_pool.best(), ss.str());
        }
    }
}

void optimize_sequential_single_knapsack(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        Counter queue_size_max = -1)
{
    double maximum_approximation_ratio = parameters.initial_maximum_approximation_ratio;
    for (Counter queue_size = 1;;) {

        if (parameters.optimization_mode != OptimizationMode::Anytime) {
            queue_size = parameters.not_anytime_sequential_single_knapsack_subproblem_queue_size;
            maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        }

        if (queue_size_max != -1
                && queue_size > queue_size_max) {
            break;
        }

        SequentialValueCorrectionFunction<Instance, Solution> kp_solve
            = [&algorithm_formatter, &parameters, queue_size, maximum_approximation_ratio](const Instance& kp_instance)
            {
                OptimizeParameters kp_parameters;
                kp_parameters.verbosity_level = 0;
                kp_parameters.timer = parameters.timer;
                kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                kp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytimeDeterministic;
                kp_parameters.not_anytime_maximum_approximation_ratio = maximum_approximation_ratio;
                kp_parameters.not_anytime_tree_search_queue_size = queue_size;
                auto kp_output = optimize(kp_instance, kp_parameters);
                return kp_output.solution_pool;
            };
        SequentialValueCorrectionParameters<Instance, Solution> svc_parameters;
        svc_parameters.verbosity_level = 0;
        svc_parameters.timer = parameters.timer;
        svc_parameters.maximum_number_of_iterations = 1;
        svc_parameters.new_solution_callback = [
            &algorithm_formatter, &queue_size](
                    const packingsolver::Output<Instance, Solution>& ps_output)
            {
                const SequentialValueCorrectionOutput<Instance, Solution>& pssvc_output
                    = static_cast<const SequentialValueCorrectionOutput<Instance, Solution>&>(ps_output);
                std::stringstream ss;
                ss << "SSK q " << queue_size;
                algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
            };
        sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, kp_solve, svc_parameters);

        // Check end.
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            break;

        // Update beam size.
        queue_size = std::max(
                queue_size + 1,
                (NodeId)(queue_size * 2));
        // Update maximum approximation ratio.
        maximum_approximation_ratio *= parameters.maximum_approximation_ratio_factor;
    }
}

void optimize_sequential_value_correction(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    if (parameters.optimization_mode == OptimizationMode::Anytime) {
        optimize_sequential_single_knapsack(
                instance,
                parameters,
                algorithm_formatter,
                parameters.sequential_value_correction_subproblem_queue_size - 1);
    }

    SequentialValueCorrectionFunction<Instance, Solution> kp_solve
        = [&algorithm_formatter, &parameters](const Instance& kp_instance)
        {
            OptimizeParameters kp_parameters;
            kp_parameters.verbosity_level = 0;
            kp_parameters.timer = parameters.timer;
            kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            kp_parameters.optimization_mode
                = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytimeDeterministic;
            kp_parameters.not_anytime_maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
            kp_parameters.not_anytime_tree_search_queue_size
                = parameters.sequential_value_correction_subproblem_queue_size;
            auto kp_output = optimize(kp_instance, kp_parameters);
            return kp_output.solution_pool;
        };
    SequentialValueCorrectionParameters<Instance, Solution> svc_parameters;
    svc_parameters.verbosity_level = 0;
    svc_parameters.timer = parameters.timer;
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        svc_parameters.maximum_number_of_iterations = parameters.not_anytime_sequential_value_correction_number_of_iterations;
    svc_parameters.new_solution_callback = [&algorithm_formatter](
            const packingsolver::Output<Instance, Solution>& ps_output)
    {
        const SequentialValueCorrectionOutput<Instance, Solution>& pssvc_output
            = static_cast<const SequentialValueCorrectionOutput<Instance, Solution>&>(ps_output);
        std::stringstream ss;
        ss << "SVC it " << pssvc_output.number_of_iterations;
        algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
    };
    sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, kp_solve, svc_parameters);
}

void optimize_dichotomic_search(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    double waste_percentage_upper_bound = std::numeric_limits<double>::infinity();
    double maximum_approximation_ratio = parameters.initial_maximum_approximation_ratio;
    for (Counter queue_size = 1;;) {

        if (parameters.optimization_mode != OptimizationMode::Anytime) {
            queue_size = parameters.not_anytime_dichotomic_search_subproblem_queue_size;
            maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        }

        DichotomicSearchFunction<Instance, Solution> bpp_solve
            = [&algorithm_formatter, &parameters, queue_size, maximum_approximation_ratio](const Instance& bpp_instance)
            {
                OptimizeParameters bpp_parameters;
                bpp_parameters.verbosity_level = 0;
                bpp_parameters.timer = parameters.timer;
                bpp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                bpp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytimeDeterministic;
                bpp_parameters.not_anytime_maximum_approximation_ratio = maximum_approximation_ratio;
                bpp_parameters.use_tree_search = 1;
                bpp_parameters.not_anytime_tree_search_queue_size = queue_size;
                auto bpp_output = optimize(bpp_instance, bpp_parameters);
                return bpp_output.solution_pool;
            };
        DichotomicSearchParameters<Instance, Solution> ds_parameters;
        ds_parameters.verbosity_level = 0;
        ds_parameters.timer = parameters.timer;
        ds_parameters.initial_waste_percentage_upper_bound = waste_percentage_upper_bound;
        ds_parameters.new_solution_callback = [
            &algorithm_formatter, &queue_size](
                    const packingsolver::Output<Instance, Solution>& ps_output)
            {
                const DichotomicSearchOutput<Instance, Solution>& psds_output
                    = static_cast<const DichotomicSearchOutput<Instance, Solution>&>(ps_output);
                std::stringstream ss;
                ss << "DS q " << queue_size
                    << " w " << psds_output.waste_percentage;
                algorithm_formatter.update_solution(psds_output.solution_pool.best(), ss.str());
            };
        auto ds_output = dichotomic_search<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, bpp_solve, ds_parameters);

        // Check end.
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        if (parameters.optimization_mode != OptimizationMode::Anytime)
            break;

        // Update beam size.
        queue_size = std::max(
                queue_size + 1,
                (NodeId)(queue_size * 2));
        // Update maximum approximation ratio.
        maximum_approximation_ratio *= parameters.maximum_approximation_ratio_factor;

        waste_percentage_upper_bound = ds_output.waste_percentage_upper_bound;
    }
}

void optimize_column_generation(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    ColumnGenerationPricingFunction<Instance, InstanceBuilder, Solution> pricing_function
        = [&algorithm_formatter, &parameters](const Instance& kp_instance)
        {
            OptimizeParameters kp_parameters;
            kp_parameters.verbosity_level = 0;
            kp_parameters.timer = parameters.timer;
            kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            kp_parameters.optimization_mode
                = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytimeDeterministic;
            kp_parameters.not_anytime_maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
            kp_parameters.not_anytime_tree_search_queue_size
                = parameters.column_generation_subproblem_queue_size;
            return optimize(kp_instance, kp_parameters);
        };

    columngenerationsolver::Model cgs_model = get_model<Instance, InstanceBuilder, Solution>(instance, pricing_function);
    columngenerationsolver::LimitedDiscrepancySearchParameters cgslds_parameters;
    cgslds_parameters.verbosity_level = 0;
    cgslds_parameters.timer = parameters.timer;
    cgslds_parameters.internal_diving = 1;
    cgslds_parameters.dummy_column_objective_coefficient = 2 * (double)instance.largest_item_copies();
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        cgslds_parameters.automatic_stop = true;
    cgslds_parameters.new_solution_callback = [&instance, &algorithm_formatter](
            const columngenerationsolver::Output& cgs_output)
    {
        const columngenerationsolver::LimitedDiscrepancySearchOutput& cgslds_output
            = static_cast<const columngenerationsolver::LimitedDiscrepancySearchOutput&>(cgs_output);
        if (cgslds_output.solution.feasible()) {
            Solution solution(instance);
            for (const auto& pair: cgslds_output.solution.columns()) {
                const Column& column = *(pair.first);
                BinPos value = std::round(pair.second);
                if (value < 0.5)
                    continue;
                solution.append(
                        *std::static_pointer_cast<Solution>(column.extra),
                        0,
                        value);
            }
            std::stringstream ss;
            ss << "CG n " << cgslds_output.number_of_nodes;
            algorithm_formatter.update_solution(solution, ss.str());
        }
    };
    cgslds_parameters.column_generation_parameters.solver_name
        = parameters.linear_programming_solver_name;
    columngenerationsolver::limited_discrepancy_search(cgs_model, cgslds_parameters);
}

void optimize_open_dimension_sequential(
        const Instance& instance,
        const OptimizeParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    AreaDbl a = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        auto mm = item_type.compute_min_max();
        LengthDbl dx = mm.second.x - mm.first.x;
        LengthDbl dy = mm.second.y - mm.first.y;
        a += dx * dy * item_type.copies;
    }
    LengthDbl x = std::sqrt(a / instance.parameters().open_dimension_xy_aspect_ratio);

    for (Counter it = 0;; ++it) {
        LengthDbl y = x * instance.parameters().open_dimension_xy_aspect_ratio;

        // Build an instance with a single bin with dimensions (x, y).
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::BinPacking);
        sub_instance_builder.set_parameters(instance.parameters());
        sub_instance_builder.add_bin_type(shape::build_rectangle(x, y));
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            sub_instance_builder.add_item_type(
                    item_type,
                    item_type.profit,
                    item_type.copies);
        }
        Instance sub_instance = sub_instance_builder.build();

        // Solve the instance.
        OptimizeParameters sub_parameters;
        sub_parameters.verbosity_level = 0;
        sub_parameters.timer = parameters.timer;
        sub_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        sub_parameters.optimization_mode = parameters.optimization_mode;
        sub_parameters.not_anytime_maximum_approximation_ratio
            = parameters.not_anytime_maximum_approximation_ratio;
        sub_parameters.not_anytime_tree_search_queue_size
            = parameters.not_anytime_tree_search_queue_size;
        auto sub_output = optimize(sub_instance, sub_parameters);

        // If no solution has been found, break.
        if (!sub_output.solution_pool.best().full())
            break;
        Solution solution(instance);
        solution.append(
                sub_output.solution_pool.best(),
                0,  // bin_pos
                1,  // copies
                {0});  // bin_types_ids
        std::stringstream ss;
        ss << "ODS it " << it;
        algorithm_formatter.update_solution(solution, ss.str());

        x = 0.99 * (std::max)(
                solution.x_max() - solution.x_min(),
                solution.y_max() - solution.y_min());
        AreaDbl a = (solution.x_max() - solution.x_min()) * (solution.y_max() - solution.y_min());
        LengthDbl x_cur = std::sqrt(a / instance.parameters().open_dimension_xy_aspect_ratio);
        if (x > x_cur)
            x = x_cur;
    }
}

}

packingsolver::irregular::Output packingsolver::irregular::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Select algorithms to run.
    ItemPos mean_number_of_items_in_bins
        = largest_bin_space(instance) / mean_item_space(instance);
    bool use_tree_search = parameters.use_tree_search;
    bool use_sequential_single_knapsack = parameters.use_sequential_single_knapsack;
    bool use_sequential_value_correction = parameters.use_sequential_value_correction;
    bool use_dichotomic_search = parameters.use_dichotomic_search;
    bool use_column_generation = parameters.use_column_generation;
    bool use_open_dimension_sequential = parameters.use_open_dimension_sequential;
    if (instance.objective() == Objective::OpenDimensionXY) {
        use_tree_search = false;
        use_sequential_single_knapsack = false;
        use_sequential_value_correction = false;
        use_dichotomic_search = false;
        use_column_generation = false;
        use_open_dimension_sequential = true;
    } else if (instance.number_of_bins() <= 1) {
        use_tree_search = true;
        use_sequential_single_knapsack = false;
        use_sequential_value_correction = false;
        use_dichotomic_search = false;
        use_column_generation = false;
        use_open_dimension_sequential = false;
    } else if (instance.objective() == Objective::Knapsack) {
        // Disable algorithms which are not available for this objective.
        use_dichotomic_search = false;
        use_open_dimension_sequential = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_column_generation) {
            if (mean_item_type_copies(instance)
                    > parameters.many_item_type_copies_factor
                    * mean_number_of_items_in_bins) {
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    use_column_generation = true;
                }
            } else {
                use_tree_search = true;
                use_column_generation = true;
            }
        }
    } else if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers) {
        // Disable algorithms which are not available for this objective.
        if (instance.number_of_bin_types() > 1)
            use_column_generation = false;
        use_dichotomic_search = false;
        use_open_dimension_sequential = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_column_generation) {
            if (mean_item_type_copies(instance)
                    > parameters.many_item_type_copies_factor
                    * mean_number_of_items_in_bins) {
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    if (instance.number_of_bin_types() == 1)
                        use_column_generation = true;
                }
            } else {
                use_tree_search = true;
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    if (instance.number_of_bin_types() == 1)
                        use_column_generation = true;
                }
            }
        }
    } else if (instance.objective() == Objective::VariableSizedBinPacking) {
        // Disable algorithms which are not available for this objective.
        if (instance.number_of_bin_types() == 1) {
            if (use_dichotomic_search) {
                use_dichotomic_search = false;
                use_tree_search = true;
            }
        } else {
            use_tree_search = false;
        }
        use_open_dimension_sequential = false;
        // Automatic selection.
        if (!use_tree_search
                && !use_sequential_single_knapsack
                && !use_sequential_value_correction
                && !use_dichotomic_search
                && !use_column_generation) {
            if (mean_item_type_copies(instance)
                    > parameters.many_item_type_copies_factor
                    * mean_number_of_items_in_bins) {
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                } else {
                    use_sequential_value_correction = true;
                    use_column_generation = true;
                }
            } else {
                if (mean_number_of_items_in_bins
                        > parameters.many_items_in_bins_threshold) {
                    use_sequential_single_knapsack = true;
                    if (instance.number_of_bin_types() > 1) {
                        use_dichotomic_search = true;
                    } else {
                        use_tree_search = true;
                    }
                } else {
                    use_sequential_value_correction = true;
                    use_column_generation = true;
                }
            }
        }
    }

    if (instance.objective() == Objective::Knapsack)
        algorithm_formatter.update_knapsack_bound(instance.item_profit());

    if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::Knapsack
            || instance.objective() == Objective::VariableSizedBinPacking) {
        optimize_onedimensional_bound(
                instance,
                parameters,
                algorithm_formatter);
    }

    if (algorithm_formatter.end_boolean()) {
        algorithm_formatter.end();
        return output;
    }
    if (parameters.timer.needs_to_end()) {
        algorithm_formatter.end();
        return output;
    }

    int last_algorithm =
        (use_open_dimension_sequential)? 5:
        (use_column_generation)? 4:
        (use_dichotomic_search)? 3:
        (use_sequential_value_correction)? 2:
        (use_sequential_single_knapsack)? 1:
        (use_tree_search)? 0:
        -1;

    // Run selected algorithms.
    std::vector<std::thread> threads;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    // Tree search.
    if (use_tree_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential
                && last_algorithm != 0) {
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_tree_search), optimize_tree_search>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        } else {
            try {
                optimize_tree_search(
                        instance,
                        parameters,
                        algorithm_formatter);
            } catch (...) {
                exception_ptr_list.front() = std::current_exception();
            }
        }
    }
    // Sequential single knapsack.
    if (use_sequential_single_knapsack) {
        exception_ptr_list.push_front(std::exception_ptr());
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential
                && last_algorithm != 1) {
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_sequential_single_knapsack), optimize_sequential_single_knapsack>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter),
                        -1));
        } else {
            try {
                optimize_sequential_single_knapsack(
                        instance,
                        parameters,
                        algorithm_formatter);
            } catch (...) {
                exception_ptr_list.front() = std::current_exception();
            }
        }
    }
    // Sequential value correction.
    if (use_sequential_value_correction) {
        exception_ptr_list.push_front(std::exception_ptr());
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential
                && last_algorithm != 2) {
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_sequential_value_correction), optimize_sequential_value_correction>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        } else {
            try {
                optimize_sequential_value_correction(
                        instance,
                        parameters,
                        algorithm_formatter);
            } catch (...) {
                exception_ptr_list.front() = std::current_exception();
            }
        }
    }
    // Dichotomic search.
    if (use_dichotomic_search) {
        exception_ptr_list.push_front(std::exception_ptr());
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential
                && last_algorithm != 3) {
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_dichotomic_search), optimize_dichotomic_search>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        } else {
            try {
                optimize_dichotomic_search(
                        instance,
                        parameters,
                        algorithm_formatter);
            } catch (...) {
                exception_ptr_list.front() = std::current_exception();
            }
        }
    }
    // Column generation.
    if (use_column_generation) {
        exception_ptr_list.push_front(std::exception_ptr());
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential
                && last_algorithm != 4) {
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_column_generation), optimize_column_generation>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        } else {
            try {
                optimize_column_generation(
                        instance,
                        parameters,
                        algorithm_formatter);
            } catch (...) {
                exception_ptr_list.front() = std::current_exception();
            }
        }
    }
    // Open dimension sequential.
    if (use_open_dimension_sequential) {
        exception_ptr_list.push_front(std::exception_ptr());
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential
                && last_algorithm != 5) {
            threads.push_back(std::thread(
                        wrapper<decltype(&optimize_open_dimension_sequential), optimize_open_dimension_sequential>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(instance),
                        std::ref(parameters),
                        std::ref(algorithm_formatter)));
        } else {
            try {
                optimize_open_dimension_sequential(
                        instance,
                        parameters,
                        algorithm_formatter);
            } catch (...) {
                exception_ptr_list.front() = std::current_exception();
            }
        }
    }
    for (Counter i = 0; i < (Counter)threads.size(); ++i)
        threads[i].join();
    for (std::exception_ptr exception_ptr: exception_ptr_list)
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);

    const Solution& solution_best = output.solution_pool.best();
    if (instance.objective() == Objective::BinPackingWithLeftovers
            && parameters.optimization_mode != OptimizationMode::Anytime
            && parameters.tree_search_guides != std::vector<GuideId>({2, 3})
            && solution_best.number_of_bins() > 0
            && solution_best.bin(solution_best.number_of_different_bins() - 1).copies == 1) {

        InstanceBuilder last_bin_instance_builder;
        last_bin_instance_builder.set_objective(Objective::BinPackingWithLeftovers);
        last_bin_instance_builder.set_parameters(instance.parameters());

        // Add bin types.
        const SolutionBin& last_bin = solution_best.bin(solution_best.number_of_different_bins() - 1);
        last_bin_instance_builder.add_bin_type(instance.bin_type(last_bin.bin_type_id), 1);

        // Add item types.
        std::vector<ItemPos> last_bin_item_copies(instance.number_of_item_types(), 0);
        for (const SolutionItem& solution_item: last_bin.items)
            last_bin_item_copies[solution_item.item_type_id]++;

        std::vector<ItemTypeId> last_bin_to_orig;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            if (last_bin_item_copies[item_type_id] > 0) {
                last_bin_instance_builder.add_item_type(
                        item_type,
                        item_type.profit,
                        last_bin_item_copies[item_type_id]);
                last_bin_to_orig.push_back(item_type_id);
            }
        }

        // Build instance.
        Instance last_bin_instance = last_bin_instance_builder.build();

        // Solve instance.
        OptimizeParameters last_bin_parameters;
        last_bin_parameters.verbosity_level = 0;
        last_bin_parameters.timer = parameters.timer;
        last_bin_parameters.optimization_mode = parameters.optimization_mode;
        last_bin_parameters.not_anytime_maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        last_bin_parameters.not_anytime_tree_search_queue_size = parameters.not_anytime_tree_search_queue_size;
        last_bin_parameters.tree_search_guides = {2, 3};
        auto last_bin_output = optimize(last_bin_instance, last_bin_parameters);

        if (last_bin_output.solution_pool.best().full()) {

            // Retrieve solution.
            Solution solution(instance);
            // Add first bins from current best solution.
            for (BinPos bin_pos = 0;
                    bin_pos < solution_best.number_of_different_bins() - 2;
                    ++bin_pos) {
                const SolutionBin& solution_bin = solution_best.bin(bin_pos);
                solution.append(solution_best, bin_pos, solution_bin.copies);
            }
            // Add last optimized bin.
            solution.append(
                    last_bin_output.solution_pool.best(),
                    0,
                    last_bin.copies,
                    {last_bin.bin_type_id},
                    last_bin_to_orig);

            // Update best solution.
            std::stringstream ss;
            ss << "post-process";
            algorithm_formatter.update_solution(solution, ss.str());

        }
    }

    algorithm_formatter.end();
    return output;
}

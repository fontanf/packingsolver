#include "packingsolver/boxstacks/optimize.hpp"

#include "packingsolver/boxstacks/algorithm_formatter.hpp"
#include "packingsolver/boxstacks/instance_builder.hpp"
#include "boxstacks/branching_scheme.hpp"
#include "boxstacks/sequential_onedimensional_rectangle.hpp"

#include "algorithms/sequential_value_correction.hpp"

#include "treesearchsolver/iterative_beam_search_2.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::boxstacks;

const packingsolver::boxstacks::Output packingsolver::boxstacks::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();
    auto logger = parameters.get_logger();

    if (instance.number_of_bins() == 1) {

        auto sor_begin = std::chrono::steady_clock::now();

        SequentialOneDimensionalRectangleParameters sor_parameters;
        sor_parameters.verbosity_level = 0;
        sor_parameters.timer = parameters.timer;
        sor_parameters.logger = logger;
        sor_parameters.onedimensional_parameters.linear_programming_solver = parameters.linear_programming_solver;
        //sor_parameters.info.set_verbosity_level(2);
        sor_parameters.new_solution_callback = [
            &algorithm_formatter](
                    const packingsolver::Output<Instance, Solution>& ps_output)
            {
                const SequentialOneDimensionalRectangleOutput& sor_output
                    = static_cast<const SequentialOneDimensionalRectangleOutput&>(ps_output);
                std::stringstream ss;
                ss << "SOR it " << sor_output.number_of_iterations;
                algorithm_formatter.update_solution(sor_output.solution_pool.best(), ss.str());
            };

        auto sor_output = sequential_onedimensional_rectangle(instance, sor_parameters);

        std::stringstream sor_ss;
        sor_ss << "SOR";
        algorithm_formatter.update_solution(sor_output.solution_pool.best(), sor_ss.str());
        //std::cout << "dec " << sor_output.solution_pool.best().item_weight() << std::endl;
        //std::cout << output.solution_pool.best().item_weight() << std::endl;
        output.sequential_onedimensional_rectangle_number_of_items = sor_output.maximum_number_of_items;
        output.sequential_onedimensional_rectangle_profit = sor_output.solution_pool.best().profit();
        //std::cout << output.sequential_onedimensional_rectangle_number_of_items << std::endl;

        auto sor_end = std::chrono::steady_clock::now();
        std::chrono::duration<double> sor_time_span
            = std::chrono::duration_cast<std::chrono::duration<double>>(sor_end - sor_begin);
        output.sequential_onedimensional_rectangle_time = sor_time_span.count();
        output.sequential_onedimensional_rectangle_onedimensional_time += sor_output.onedimensional_time;
        output.sequential_onedimensional_rectangle_rectangle_time += sor_output.rectangle_time;
        output.number_of_sequential_onedimensional_rectangle_calls++;
        if (!sor_output.solution_pool.best().full())
            output.sequential_onedimensional_rectangle_failed = sor_output.failed;

        // The boxstacks branching scheme is significantly more expensive than the
        // sequential_onedimensional_rectangle algorithm. Therefore, we only use it
        // if it looks promising enough.
        // We don't run the boxstacks branching scheme if:
        // - The best solution already contains all the items
        // - The solution of the sequential_onedimensional_rectangle algorithm
        //   never violated the axle weight constraints.
        // - The weight of the smallest unpacked item is greater than the remaining
        //   capacity.
        // - TODO The items need at least i bins to be all packed, and more than
        //   0.8 * 1 / i + 0.2 * 1 / (i - 1)
        //   fraction of the volume and weight have been packed.
        bool run_boxstacks_branching_scheme = true;
        if (output.solution_pool.best().number_of_items() == instance.number_of_items())
            run_boxstacks_branching_scheme = false;
        if (!sor_output.failed)
            run_boxstacks_branching_scheme = false;
        bool no_lighter_item = true;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (output.solution_pool.best().item_copies(item_type_id)
                    == instance.item_type(item_type_id).copies)
                continue;
            if (output.solution_pool.best().item_weight()
                    + instance.item_type(item_type_id).weight
                    <= instance.bin_weight())
                no_lighter_item = false;
        }
        if (no_lighter_item)
            run_boxstacks_branching_scheme = false;
        //BinPos minimum_number_of_bins = std::max(
        //        std::ceil((double)instance.item_volume() / instance.bin_volume()),
        //        std::ceil((double)instance.item_weight() / instance.bin_weight()));
        //if (minimum_number_of_bins > 1) {
        //    double minimum_fraction = 0.8 / minimum_number_of_bins + 0.2 / (minimum_number_of_bins - 1);
        //    if (output.solution_pool.best().item_volume() >= minimum_fraction * instance.item_volume()
        //            && output.solution_pool.best().item_weight() >= minimum_fraction * instance.item_weight())
        //        run_boxstacks_branching_scheme = false;
        //}

        if (output.solution_pool.best().number_of_items() == instance.number_of_items()) {
            output.number_of_sequential_onedimensional_rectangle_perfect++;
        } else if (!run_boxstacks_branching_scheme) {
            output.number_of_sequential_onedimensional_rectangle_good++;
        }

        if (run_boxstacks_branching_scheme) {
            auto bs_begin = std::chrono::steady_clock::now();

            std::vector<GuideId> guides;
            if (!parameters.tree_search_guides.empty()) {
                guides = parameters.tree_search_guides;
            } else if (instance.objective() == Objective::Knapsack) {
                guides = {4, 5};
            } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
                guides = {0, 1};
            } else {
                guides = {0, 2};
            }
            guides = {4};

            std::vector<Direction> directions;
            if (instance.objective() == Objective::OpenDimensionX) {
                directions = {Direction::X};
            } else if (instance.objective() == Objective::OpenDimensionY) {
                directions = {Direction::Y};
            } else if (instance.unloading_constraint() == rectangle::UnloadingConstraint::IncreasingX
                    || instance.unloading_constraint() == rectangle::UnloadingConstraint::OnlyXMovements) {
                directions = {Direction::X};
            } else if (instance.unloading_constraint() == rectangle::UnloadingConstraint::IncreasingY
                    || instance.unloading_constraint() == rectangle::UnloadingConstraint::OnlyYMovements) {
                directions = {Direction::Y};
            } else if (instance.number_of_bin_types() == 1) {
                directions = {Direction::X, Direction::Y};
            } else {
                directions = {Direction::Any};
            }

            std::vector<BranchingScheme> branching_schemes;
            std::vector<treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme>> ibs_parameters_list;
            for (GuideId guide_id: guides) {
                for (Direction direction: directions) {
                    //std::cout << growth_factor << " " << guide_id << " " << direction << std::endl;
                    BranchingScheme::Parameters branching_scheme_parameters;
                    branching_scheme_parameters.guide_id = guide_id;
                    branching_scheme_parameters.direction = direction;
                    branching_scheme_parameters.maximum_number_of_selected_items = output.sequential_onedimensional_rectangle_number_of_items;
                    branching_schemes.push_back(BranchingScheme(instance, branching_scheme_parameters));
                    treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme> ibs_parameters;
                    ibs_parameters.verbosity_level = 0;
                    ibs_parameters.timer = parameters.timer;
                    if (parameters.optimization_mode != OptimizationMode::Anytime) {
                        ibs_parameters.minimum_size_of_the_queue
                            = parameters.not_anytime_tree_search_queue_size;
                        ibs_parameters.maximum_size_of_the_queue
                            = parameters.not_anytime_tree_search_queue_size;
                    }
                    //ibs_parameters.info.set_verbosity_level(1);
                    ibs_parameters_list.push_back(ibs_parameters);
                }
            }

            std::vector<std::thread> threads;
            std::forward_list<std::exception_ptr> exception_ptr_list;
            for (Counter i = 0; i < (Counter)branching_schemes.size(); ++i) {
                ibs_parameters_list[i].new_solution_callback
                    = [&algorithm_formatter, &branching_schemes, i](
                            const treesearchsolver::Output<BranchingScheme>& tss_output)
                    {
                        const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>& tssibs_output
                            = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>&>(tss_output);
                        Solution solution = branching_schemes[i].to_solution(
                                tssibs_output.solution_pool.best());
                        std::stringstream ss;
                        ss << branching_schemes[i].parameters().guide_id
                            << " " << branching_schemes[i].parameters().direction
                            << " " << tssibs_output.maximum_size_of_the_queue;
                        algorithm_formatter.update_solution(solution, ss.str());
                    };
                if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential) {
                    exception_ptr_list.push_front(std::exception_ptr());
                    threads.push_back(std::thread(
                                wrapper<decltype(&treesearchsolver::iterative_beam_search_2<BranchingScheme>), treesearchsolver::iterative_beam_search_2<BranchingScheme>>,
                                std::ref(exception_ptr_list.front()),
                                std::ref(branching_schemes[i]),
                                ibs_parameters_list[i]));
                } else {
                    treesearchsolver::iterative_beam_search_2<BranchingScheme>(
                            branching_schemes[i],
                            ibs_parameters_list[i]);
                }
            }
            for (Counter i = 0; i < (Counter)threads.size(); ++i)
                threads[i].join();
            for (const std::exception_ptr& exception_ptr: exception_ptr_list)
                if (exception_ptr)
                    std::rethrow_exception(exception_ptr);

            auto bs_end = std::chrono::steady_clock::now();
            std::chrono::duration<double> bs_time_span
                = std::chrono::duration_cast<std::chrono::duration<double>>(bs_end - bs_begin);
            output.tree_search_time = bs_time_span.count();
            output.number_of_tree_search_calls++;
            if (output.solution_pool.best().number_of_items() == instance.number_of_items()) {
                output.number_of_tree_search_perfect++;
            } else if (output.solution_pool.best().profit() > sor_output.solution_pool.best().profit()) {
                output.number_of_tree_search_better++;
            }
        }

        //for (ItemTypeId j = 0; j < instance.number_of_item_types(); ++j) {
        //    const ItemType& item_type = instance.item_type(j);
        //    ItemPos c = output.solution_pool.best().item_copies(j);
        //    if (c != item_type.copies)
        //        std::cout << "j " << j << std::endl;
        //}

    } else {

        SequentialValueCorrectionFunction<Instance, Solution> kp_solve
            = [&parameters, &output](const Instance& kp_instance)
            {
                OptimizeParameters kp_parameters;
                kp_parameters.verbosity_level = 0;
                kp_parameters.timer = parameters.timer;
                kp_parameters.optimization_mode
                    = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                    OptimizationMode::NotAnytimeSequential:
                    OptimizationMode::NotAnytime;
                kp_parameters.linear_programming_solver = parameters.linear_programming_solver;
                kp_parameters.not_anytime_tree_search_queue_size
                    = parameters.sequential_value_correction_subproblem_queue_size;
                //kp_parameters.sequential_onedimensional_rectangle_parameters.rectangle_queue_size = parameters.sequential_value_correction_queue_size;
                auto kp_output = optimize(kp_instance, kp_parameters);

                // Update output.
                output.sequential_onedimensional_rectangle_time += kp_output.sequential_onedimensional_rectangle_time;
                output.sequential_onedimensional_rectangle_rectangle_time += kp_output.sequential_onedimensional_rectangle_rectangle_time;
                output.sequential_onedimensional_rectangle_onedimensional_time += kp_output.sequential_onedimensional_rectangle_onedimensional_time;
                output.tree_search_time += kp_output.tree_search_time;
                output.number_of_sequential_onedimensional_rectangle_calls += kp_output.number_of_sequential_onedimensional_rectangle_calls;
                output.number_of_sequential_onedimensional_rectangle_perfect += kp_output.number_of_sequential_onedimensional_rectangle_perfect;
                output.number_of_sequential_onedimensional_rectangle_good += kp_output.number_of_sequential_onedimensional_rectangle_good;
                output.number_of_tree_search_calls += kp_output.number_of_tree_search_calls;
                output.number_of_tree_search_perfect += kp_output.number_of_tree_search_perfect;
                output.number_of_tree_search_better += kp_output.number_of_tree_search_better;
                return kp_output.solution_pool;
            };
        SequentialValueCorrectionParameters<Instance, Solution> svc_parameters;
        svc_parameters.verbosity_level = 0;
        svc_parameters.timer = parameters.timer;
        svc_parameters.new_solution_callback = [&algorithm_formatter](
                const packingsolver::Output<Instance, Solution>& ps_output)
        {
            const SequentialValueCorrectionOutput<Instance, Solution>& pssvc_output
                = static_cast<const SequentialValueCorrectionOutput<Instance, Solution>&>(ps_output);
            std::stringstream ss;
            ss << "iteration " << pssvc_output.number_of_iterations;
            algorithm_formatter.update_solution(pssvc_output.solution_pool.best(), ss.str());
        };
        auto svc_output = sequential_value_correction<Instance, InstanceBuilder, Solution, AlgorithmFormatter>(instance, kp_solve, svc_parameters);

    }

    algorithm_formatter.end();
    return output;
}

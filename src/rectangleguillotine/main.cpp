#include "packingsolver/rectangleguillotine/optimize.hpp"
#include "packingsolver/rectangleguillotine/post_process.hpp"
#include "packingsolver/rectangleguillotine/instance_builder.hpp"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

void read_args(
        packingsolver::Parameters<Instance, Solution>& parameters,
        const po::variables_map& vm)
{
    parameters.timer.set_sigint_handler();
    parameters.messages_to_stdout = true;
    if (vm.count("time-limit"))
        parameters.timer.set_time_limit(vm["time-limit"].as<double>());
    if (vm.count("verbosity-level"))
        parameters.verbosity_level = vm["verbosity-level"].as<int>();
    if (vm.count("log"))
        parameters.log_path = vm["log"].as<std::string>();
    parameters.log_to_stderr = vm.count("log-to-stderr");
    if (vm.count("json-search-tree"))
        parameters.json_search_tree_path = vm["json-search-tree"].as<std::string>();
    if (vm.count("output"))
        parameters.write_json_output = true;
    bool only_write_at_the_end = vm.count("only-write-at-the-end");
    if (!only_write_at_the_end) {

        std::string certificate_path = "";
        if (vm.count("certificate"))
            certificate_path = vm["certificate"].as<std::string>();

        std::string json_output_path = "";
        if (vm.count("output"))
            json_output_path = vm["output"].as<std::string>();

        parameters.new_solution_callback = [
            json_output_path,
            certificate_path](
                    const packingsolver::Output<Instance, Solution>& output)
        {
            if (!json_output_path.empty())
                output.write_json_output(json_output_path);
            if (!certificate_path.empty())
                output.solution_pool.best().write(certificate_path);
        };
    }
}

int main(int argc, char *argv[])
{
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
            (",h", "Produce help message")

            ("items,i", po::value<std::string>()->required(), "Items path")
            ("bins,b", po::value<std::string>(), "Bins path")
            ("defects,d", po::value<std::string>(), "Defects path")
            ("parameters", po::value<std::string>(), "Parameters path")

            ("bin-infinite-x", "")
            ("bin-infinite-y", "")
            ("bin-infinite-copies", "")
            ("bin-unweighted", "")
            ("item-infinite-copies", "")
            ("item-multiply-copies", po::value<ItemPos>(), "")
            ("unweighted", "")
            ("no-item-rotation", "")

            ("objective,f", po::value<Objective>(), "Objective")

            ("predefined,p", po::value<std::string>(), "")

            ("number-of-stages,", po::value<Counter>(), "")
            ("number-of-stages-unlimited,", "")
            ("cut-type,", po::value<rectangleguillotine::CutType>(), "")
            ("first-stage-orientation,", po::value<rectangleguillotine::CutOrientation>(), "")
            ("min1cut,", po::value<Length>(), "")
            ("minimum-distance-1-cuts,", po::value<Length>(), "")
            ("max1cut,", po::value<Length>(), "")
            ("maximum-distance-1-cuts,", po::value<Length>(), "")
            ("min2cut,", po::value<Length>(), "")
            ("minimum-distance-2-cuts,", po::value<Length>(), "")
            ("max2cut,", po::value<Length>(), "")
            ("maximum-distance-2-cuts,", po::value<Length>(), "")
            ("min-waste,", po::value<Length>(), "")
            ("minimum-waste-length,", po::value<Length>(), "")
            ("maximum-number-1-cuts,", po::value<Counter>(), "")
            ("maximum-number-2-cuts,", po::value<Counter>(), "")
            ("cut-through-defects", po::value<bool>(), "")
            ("cut-thickness", po::value<Length>(), "")
            ("fixed-cutting-costs", po::value<std::vector<Profit>>()->multitoken(), "")
            ("variable-cutting-costs", po::value<std::vector<Profit>>()->multitoken(), "")

            ("output,o", po::value<std::string>(), "Output path")
            ("certificate,c", po::value<std::string>(), "Certificate path")
            ("log,l", po::value<std::string>(), "Log path")
            ("time-limit,t", po::value<double>(), "Time limit in seconds")
            ("seed,s", po::value<Seed>(), "Seed (not used)")
            ("only-write-at-the-end,e", "Only write output and certificate files at the end")
            ("verbosity-level,v", po::value<int>(), "Verbosity level")
            ("log2stderr,w", "Write log in stderr")
            ("json-search-tree", po::value<std::string>(), "JSON search tree path")

            ("memory-limit,", po::value<Megabytes>(), "Memory limit in mebibytes (default: unlimited)")

            ("linear-programming-solver,", po::value<columngenerationsolver::SolverName>(), "set linear programming solver")
            ("optimization-mode,", po::value<OptimizationMode>(), "set optimization mode")
            ("use-tree-search,", po::value<bool>(), "enable tree search algorithm")
            ("use-tree-search-maximal-spaces,", po::value<bool>(), "enable tree search maximal spaces algorithm")
            ("use-column-generation-strips,", po::value<bool>(), "enable column generation strips algorithm")
            ("use-sequential-strips-onedimensional,", po::value<bool>(), "enable sequential strips onedimensional algorithm")
            ("use-dynamic-programming-infinite-copies-array,", po::value<bool>(), "enable dynamic programming (infinite copies, array) algorithm")
            ("use-labeling,", po::value<bool>(), "enable labeling algorithm")
            ("use-sequential-single-knapsack,", po::value<bool>(), "enable sequential-single-knapsack")
            ("use-sequential-value-correction,", po::value<bool>(), "enable sequential-value-correction")
            ("use-column-generation,", po::value<bool>(), "enable column-generation")
            ("use-dichotomic-search,", po::value<bool>(), "enable dichotomic search")
            ("sequential-value-correction-subproblem-tree-search-queue-size,", po::value<NodeId>(), "set sequential value correction subproblem queue size")
            ("sequential-value-correction-subproblem-tree-search-maximal-spaces-queue-size,", po::value<NodeId>(), "set sequential value correction subproblem maximal spaces queue size")
            ("column-generation-subproblem-tree-search-queue-size,", po::value<NodeId>(), "set column generation subproblem queue size")
            ("column-generation-subproblem-tree-search-maximal-spaces-queue-size,", po::value<NodeId>(), "set column generation subproblem maximal spaces queue size")
            ("not-anytime-tree-search-queue-size,", po::value<Counter>(), "")
            ("not-anytime-tree-search-maximal-spaces-queue-size,", po::value<Counter>(), "")
            ("not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size,", po::value<Counter>(), "")
            ("not-anytime-sequential-single-knapsack-subproblem-tree-search-maximal-spaces-queue-size,", po::value<Counter>(), "")
            ("not-anytime-sequential-value-correction-number-of-iterations,", po::value<Counter>(), "")
            ("not-anytime-dichotomic-search-subproblem-tree-search-queue-size,", po::value<Counter>(), "")

            ("minimize-number-of-stages,", po::value<bool>(), "")
            ("group-identical-bins,", po::value<bool>(), "")
            ("sort-subplates,", po::value<bool>(), "")
            ;
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << std::endl;;
            return 1;
        }
        try {
            po::notify(vm);
        } catch (const po::required_option& e) {
            std::cout << desc << std::endl;;
            return 1;
        }

        // Build instance.

        InstanceBuilder instance_builder;
        std::string instance_path = vm["items"].as<std::string>();
        if (fs::is_regular_file(instance_path)) {
            instance_path = "";
        } else if (fs::is_regular_file(instance_path + "_items.csv")) {
            instance_path = instance_path + "_";
        } else if (fs::is_regular_file(instance_path + "items.csv")) {
            instance_path = instance_path;
        } else if (fs::is_regular_file(instance_path + "/items.csv")) {
            instance_path = instance_path + "/";
        } else {
            throw std::invalid_argument("");
        }

        if (instance_path.empty()) {
            instance_builder.read_item_types(vm["items"].as<std::string>());
        } else {
            instance_builder.read_item_types(instance_path + "items.csv");
        }

        if (vm.count("bins")) {
            instance_builder.read_bin_types(vm["bins"].as<std::string>());
        } else {
            instance_builder.read_bin_types(instance_path + "bins.csv");
        }

        if (vm.count("defects")) {
            instance_builder.read_defects(vm["defects"].as<std::string>());
        } else if (fs::is_regular_file(instance_path + "defects.csv")) {
            instance_builder.read_defects(instance_path + "defects.csv");
        }

        if (vm.count("item-multiply-copies"))
            instance_builder.multiply_item_types_copies(vm["item-multiply-copies"].as<ItemPos>());
        if (vm.count("bin-infinite-x"))
            instance_builder.set_bin_types_infinite_x();
        if (vm.count("bin-infinite-y"))
            instance_builder.set_bin_types_infinite_y();
        if (vm.count("bin-infinite-copies"))
            instance_builder.set_bin_types_infinite_copies();
        if (vm.count("no-item-rotation"))
            instance_builder.set_item_types_oriented();
        if (vm.count("item-infinite-copies"))
            instance_builder.set_item_types_infinite_copies();
        if (vm.count("unweighted"))
            instance_builder.set_item_types_unweighted();
        if (vm.count("bin-unweighted"))
            instance_builder.set_bin_types_unweighted();

        if (vm.count("parameters")) {
            instance_builder.read_parameters(vm["parameters"].as<std::string>());
        } else if (fs::is_regular_file(instance_path + "parameters.csv")) {
            instance_builder.read_parameters(instance_path + "parameters.csv");
        }

        if (vm.count("objective"))
            instance_builder.set_objective(vm["objective"].as<Objective>());
        if (vm.count("predefined"))
            instance_builder.set_predefined(vm["predefined"].as<std::string>());

        if (vm.count("number-of-stages"))
            instance_builder.set_number_of_stages(vm["number-of-stages"].as<Counter>());
        if (vm.count("number-of-stages-unlimited"))
            instance_builder.set_number_of_stages_unlimited();
        if (vm.count("cut-type"))
            instance_builder.set_cut_type(vm["cut-type"].as<CutType>());
        if (vm.count("first-stage-orientation"))
            instance_builder.set_first_stage_orientation(vm["first-stage-orientation"].as<CutOrientation>());
        if (vm.count("min1cut"))
            instance_builder.set_minimum_distance_1_cuts(vm["min1cut"].as<Length>());
        if (vm.count("minimum-distance-1-cuts"))
            instance_builder.set_minimum_distance_1_cuts(vm["minimum-distance-1-cuts"].as<Length>());
        if (vm.count("max1cut"))
            instance_builder.set_maximum_distance_1_cuts(vm["max1cut"].as<Length>());
        if (vm.count("maximum-distance-1-cuts"))
            instance_builder.set_maximum_distance_1_cuts(vm["maximum-distance-1-cuts"].as<Length>());
        if (vm.count("min2cut"))
            instance_builder.set_minimum_distance_2_cuts(vm["min2cut"].as<Length>());
        if (vm.count("minimum-distance-2-cuts"))
            instance_builder.set_minimum_distance_2_cuts(vm["minimum-distance-2-cuts"].as<Length>());
        if (vm.count("max2cut"))
            instance_builder.set_maximum_distance_2_cuts(vm["max2cut"].as<Length>());
        if (vm.count("maximum-distance-2-cuts"))
            instance_builder.set_maximum_distance_2_cuts(vm["maximum-distance-2-cuts"].as<Length>());
        if (vm.count("min-waste"))
            instance_builder.set_minimum_waste_length(vm["min-waste"].as<Length>());
        if (vm.count("minimum-waste-length"))
            instance_builder.set_minimum_waste_length(vm["minimum-waste-length"].as<Length>());
        if (vm.count("maximum-number-1-cuts"))
            instance_builder.set_maximum_number_1_cuts(vm["maximum-number-1-cuts"].as<Counter>());
        if (vm.count("maximum-number-2-cuts"))
            instance_builder.set_maximum_number_2_cuts(vm["maximum-number-2-cuts"].as<Counter>());
        if (vm.count("cut-through-defects"))
            instance_builder.set_cut_through_defects(vm["cut-through-defects"].as<bool>());
        if (vm.count("cut-thickness"))
            instance_builder.set_cut_thickness(vm["cut-thickness"].as<Length>());
        if (vm.count("fixed-cutting-costs")) {
            const auto& fixed_cutting_costs = vm["fixed-cutting-costs"].as<std::vector<Profit>>();
            for (Counter stage_id = 0;
                    stage_id < (Counter)fixed_cutting_costs.size();
                    ++stage_id) {
                instance_builder.set_fixed_cutting_cost(stage_id, fixed_cutting_costs[stage_id]);
            }
        }
        if (vm.count("variable-cutting-costs")) {
            const auto& variable_cutting_costs = vm["variable-cutting-costs"].as<std::vector<Profit>>();
            for (Counter stage_id = 0;
                    stage_id < (Counter)variable_cutting_costs.size();
                    ++stage_id) {
                instance_builder.set_variable_cutting_cost(stage_id, variable_cutting_costs[stage_id]);
            }
        }

        Instance instance = instance_builder.build();

        // Read algorithm parameters.

        OptimizeParameters parameters;
        read_args(parameters, vm);
        if (vm.count("linear-programming-solver"))
            parameters.linear_programming_solver_name = vm["linear-programming-solver"].as<columngenerationsolver::SolverName>();
        if (vm.count("optimization-mode"))
            parameters.optimization_mode = vm["optimization-mode"].as<OptimizationMode>();
        if (vm.count("memory-limit"))
            parameters.memory_limit_megabytes = vm["memory-limit"].as<Megabytes>();

        if (vm.count("use-tree-search"))
            parameters.use_tree_search = vm["use-tree-search"].as<bool>();
        if (vm.count("use-tree-search-maximal-spaces"))
            parameters.use_tree_search_maximal_spaces = vm["use-tree-search-maximal-spaces"].as<bool>();
        if (vm.count("use-column-generation-strips"))
            parameters.use_column_generation_strips = vm["use-column-generation-strips"].as<bool>();
        if (vm.count("use-sequential-strips-onedimensional"))
            parameters.use_sequential_strips_onedimensional = vm["use-sequential-strips-onedimensional"].as<bool>();
        if (vm.count("use-dynamic-programming-infinite-copies-array"))
            parameters.use_dynamic_programming_infinite_copies_array = vm["use-dynamic-programming-infinite-copies-array"].as<bool>();
        if (vm.count("use-labeling"))
            parameters.use_labeling = vm["use-labeling"].as<bool>();
        if (vm.count("use-sequential-single-knapsack"))
            parameters.use_sequential_single_knapsack = vm["use-sequential-single-knapsack"].as<bool>();
        if (vm.count("use-sequential-value-correction"))
            parameters.use_sequential_value_correction = vm["use-sequential-value-correction"].as<bool>();
        if (vm.count("use-column-generation"))
            parameters.use_column_generation = vm["use-column-generation"].as<bool>();
        if (vm.count("use-dichotomic-search"))
            parameters.use_dichotomic_search = vm["use-dichotomic-search"].as<bool>();

        if (vm.count("sequential-value-correction-subproblem-tree-search-queue-size"))
            parameters.sequential_value_correction_subproblem_tree_search_queue_size = vm["sequential-value-correction-subproblem-tree-search-queue-size"].as<NodeId>();
        if (vm.count("sequential-value-correction-subproblem-tree-search-maximal-spaces-queue-size"))
            parameters.sequential_value_correction_subproblem_tree_search_maximal_spaces_queue_size = vm["sequential-value-correction-subproblem-tree-search-maximal-spaces-queue-size"].as<NodeId>();
        if (vm.count("column-generation-subproblem-tree-search-queue-size"))
            parameters.column_generation_subproblem_tree_search_queue_size = vm["column-generation-subproblem-tree-search-queue-size"].as<NodeId>();
        if (vm.count("column-generation-subproblem-tree-search-maximal-spaces-queue-size"))
            parameters.column_generation_subproblem_tree_search_maximal_spaces_queue_size = vm["column-generation-subproblem-tree-search-maximal-spaces-queue-size"].as<NodeId>();
        if (vm.count("not-anytime-tree-search-queue-size"))
            parameters.not_anytime_tree_search_queue_size = vm["not-anytime-tree-search-queue-size"].as<Counter>();
        if (vm.count("not-anytime-tree-search-maximal-spaces-queue-size"))
            parameters.not_anytime_tree_search_maximal_spaces_queue_size = vm["not-anytime-tree-search-maximal-spaces-queue-size"].as<Counter>();
        if (vm.count("not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size"))
            parameters.not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size = vm["not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size"].as<Counter>();
        if (vm.count("not-anytime-sequential-single-knapsack-subproblem-tree-search-maximal-spaces-queue-size"))
            parameters.not_anytime_sequential_single_knapsack_subproblem_tree_search_maximal_spaces_queue_size = vm["not-anytime-sequential-single-knapsack-subproblem-tree-search-maximal-spaces-queue-size"].as<Counter>();
        if (vm.count("not-anytime-sequential-value-correction-number-of-iterations"))
            parameters.not_anytime_sequential_value_correction_number_of_iterations = vm["not-anytime-sequential-value-correction-number-of-iterations"].as<Counter>();
        if (vm.count("not-anytime-dichotomic-search-subproblem-tree-search-queue-size"))
            parameters.not_anytime_dichotomic_search_subproblem_tree_search_queue_size = vm["not-anytime-dichotomic-search-subproblem-tree-search-queue-size"].as<Counter>();
        const rectangleguillotine::Output output = optimize(instance, parameters);

        if (vm.count("output"))
            output.write_json_output(vm["output"].as<std::string>());

        Solution solution = output.solution_pool.best();
        if (vm.count("minimize-number-of-stages")
                && vm["minimize-number-of-stages"].as<bool>()) {
            solution = minimize_number_of_stages(solution);
        }
        if (vm.count("group-identical-bins")
                && vm["group-identical-bins"].as<bool>()) {
            GroupIdenticalBinsOutput gib_output = group_identical_bins(solution);
            solution = gib_output.solution_pool.best();
        }
        if (vm.count("sort-subplates")
                && vm["sort-subplates"].as<bool>()) {
            solution = sort_subplates(solution);
        }

        if (vm.count("certificate"))
            solution.write(vm["certificate"].as<std::string>());

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

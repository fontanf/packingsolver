#include "packingsolver/irregular/optimize.hpp"
#include "packingsolver/irregular/post_process.hpp"
#include "packingsolver/irregular/instance_builder.hpp"

#include <boost/program_options.hpp>

using namespace packingsolver;
using namespace packingsolver::irregular;
namespace po = boost::program_options;

void read_args(
        packingsolver::Parameters<Instance, Solution, irregular::Output>& parameters,
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
        // Parse program options
        po::options_description desc("Allowed options");
        desc.add_options()
            (",h", "Produce help message")

            ("input,i", po::value<std::string>()->required(), "Input path")

            ("objective,f", po::value<Objective>(), "Objective")

            ("item-bin-minimum-spacing", po::value<LengthDbl>(), "Item-bin minimum spacing")
            ("item-item-minimum-spacing", po::value<LengthDbl>(), "Item-item minimum spacing")
            ("leftover-mode", po::value<LeftoverMode>(), "set leftover mode")
            ("leftover-all-bins", po::value<bool>(), "also compare the leftover values of all the bins")
            ("bin-unweighted", "")
            ("unweighted", "")
            ("continuous-rotations", "")

            ("output,o", po::value<std::string>(), "Output path")
            ("certificate,c", po::value<std::string>(), "Certificate path")
            ("log,l", po::value<std::string>(), "Log path")
            ("time-limit,t", po::value<double>(), "Time limit in seconds")
            ("seed,s", po::value<Seed>(), "Seed")
            ("only-write-at-the-end,e", "Only write output and certificate files at the end")
            ("verbosity-level,v", po::value<int>(), "Verbosity level")
            ("log2stderr,w", "Write log in stderr")
            ("json-search-tree", po::value<std::string>(), "JSON search tree path")

            ("memory-limit,", po::value<Megabytes>(), "Memory limit in mebibytes (default: unlimited)")

            ("linear-programming-solver,", po::value<columngenerationsolver::SolverName>(), "set linear programming solver")
            ("optimization-mode,", po::value<OptimizationMode>(), "set optimization mode")
            ("use-tree-search,", po::value<bool>(), "enable tree search algorithm")
            ("use-tree-search-periodic-packing,", po::value<bool>(), "enable tree search periodic packing algorithm")
            ("use-tree-search-maximal-spaces,", po::value<bool>(), "enable tree search maximal spaces algorithm")
            ("use-milp-raster,", po::value<bool>(), "enable MILP raster algorithm")
            ("use-local-search-linear-programming,", po::value<bool>(), "enable local search algorithm with the linear programming engine")
            ("use-local-search-nonlinear-programming,", po::value<bool>(), "enable local search algorithm with the nonlinear programming engine (requires Knitro)")
            ("use-local-search-direct-trigonometry,", po::value<bool>(), "enable local search algorithm with the direct trigonometry engine")
            ("use-local-search-black-box-optimization,", po::value<bool>(), "enable local search algorithm with the black-box optimization engine (requires Knitro)")
            ("local-search-weighting-scheme,", po::value<LocalSearchWeightingScheme>(), "set the weighting scheme of the local search: multiplicative, additive")
            ("use-sequential-single-knapsack,", po::value<bool>(), "enable sequential-single-knapsack")
            ("use-sequential-value-correction,", po::value<bool>(), "enable sequential-value-correction")
            ("use-column-generation,", po::value<bool>(), "enable column-generation")
            ("use-benders-decomposition,", po::value<bool>(), "enable Benders decomposition algorithm")
            ("benders-decomposition-use-tree-search-relative-positions,", po::value<bool>(), "use the tree search on relative positions in the subproblems of the Benders decomposition algorithm")
            ("use-tree-search-relative-positions,", po::value<bool>(), "use the tree search on relative positions (Feasibility objective with a single bin)")
            ("use-dichotomic-search,", po::value<bool>(), "enable dichotomic search")
            ("reduce,", po::value<bool>(), "enable/disable instance reduction (preprocessing)")
            ("initial-maximum-approximation-ratio,", po::value<double>(), "initial maximum approximation ratio")
            ("maximum-approximation-ratio-factor,", po::value<double>(), "decrease factor for the maximum approximation ratio")
            ("sequential-value-correction-subproblem-tree-search-queue-size,", po::value<NodeId>(), "set sequential value correction subproblem queue size")
            ("anytime-sequential-value-correction-growing-queue-maximum-number-of-iterations,", po::value<Counter>(), "")
            ("sequential-value-correction-restrict-bin-types,", po::value<bool>(), "")
            ("sequential-value-correction-scarce-item-types-profit-factor-exponent,", po::value<double>(), "")
            ("column-generation-subproblem-tree-search-queue-size,", po::value<NodeId>(), "set column generation subproblem queue size")
            ("post-process-last-bin-maximum-number-of-items,", po::value<ItemPos>(), "re-optimize the last bin in the post-process only if it contains fewer items")
            ("not-anytime-maximum-approximation-ratio,", po::value<double>(), "")
            ("not-anytime-tree-search-queue-size,", po::value<Counter>(), "")
            ("not-anytime-tree-search-periodic-packing-queue-size,", po::value<Counter>(), "")
            ("not-anytime-tree-search-maximal-spaces-queue-size,", po::value<Counter>(), "")
            ("not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size,", po::value<Counter>(), "")
            ("not-anytime-sequential-value-correction-number-of-iterations,", po::value<Counter>(), "")
            ("not-anytime-local-search-maximum-number-of-failed-separations,", po::value<Counter>(), "")
            ("not-anytime-local-search-maximum-number-of-moves-factor,", po::value<Counter>(), "")
            ("anytime-sequential-single-knapsack-subproblem-tree-search-initial-queue-size,", po::value<NodeId>(), "")
            ("anytime-sequential-single-knapsack-subproblem-tree-search-periodic-packing-initial-queue-size,", po::value<NodeId>(), "")
            ("anytime-sequential-single-knapsack-subproblem-local-search-initial-maximum-number-of-moves-factor,", po::value<Counter>(), "")
            ("not-anytime-sequential-single-knapsack-subproblem-tree-search-periodic-packing-queue-size,", po::value<NodeId>(), "")
            ("not-anytime-sequential-single-knapsack-subproblem-local-search-maximum-number-of-moves-factor,", po::value<Counter>(), "")
            ("not-anytime-dichotomic-search-subproblem-tree-search-queue-size,", po::value<Counter>(), "")

            ("group-identical-bins,", po::value<bool>(), "")

            ("anchor,", po::value<bool>(), "")
            ("anchor-x-weight,", po::value<double>(), "")
            ("anchor-y-weight,", po::value<double>(), "")
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

        InstanceBuilder instance_builder;

        instance_builder.read(vm["input"].as<std::string>());

        if (vm.count("objective"))
            instance_builder.set_objective(vm["objective"].as<Objective>());
        if (vm.count("item-item-minimum-spacing"))
            instance_builder.set_item_item_minimum_spacing(vm["item-item-minimum-spacing"].as<LengthDbl>());
        if (vm.count("leftover-mode"))
            instance_builder.set_leftover_mode(vm["leftover-mode"].as<LeftoverMode>());
        if (vm.count("leftover-all-bins"))
            instance_builder.set_leftover_all_bins(vm["leftover-all-bins"].as<bool>());
        if (vm.count("unweighted"))
            instance_builder.set_item_types_unweighted();
        if (vm.count("bin-unweighted"))
            instance_builder.set_bin_types_unweighted();
        if (vm.count("continuous-rotations"))
            instance_builder.set_item_types_continuous_rotations();

        Instance instance = instance_builder.build();

        //for (ItemTypeId item_type_id = 0;
        //        item_type_id < instance.number_of_item_types();
        //        ++item_type_id) {
        //    instance.item_type(item_type_id).write_svg("item_type_" + std::to_string(item_type_id) + ".svg");
        //}
        //instance.write(vm["input"].as<std::string>() + "_tmp.json");

        OptimizeParameters parameters;
        read_args(parameters, vm);
        if (vm.count("linear-programming-solver"))
            parameters.linear_programming_solver_name = vm["linear-programming-solver"].as<columngenerationsolver::SolverName>();
        if (vm.count("optimization-mode"))
            parameters.optimization_mode = vm["optimization-mode"].as<OptimizationMode>();
        if (vm.count("seed"))
            parameters.seed = vm["seed"].as<Seed>();
        if (vm.count("memory-limit"))
            parameters.memory_limit_megabytes = vm["memory-limit"].as<Megabytes>();

        if (vm.count("use-tree-search"))
            parameters.use_tree_search = vm["use-tree-search"].as<bool>();
        if (vm.count("use-tree-search-periodic-packing"))
            parameters.use_tree_search_periodic_packing = vm["use-tree-search-periodic-packing"].as<bool>();
        if (vm.count("use-tree-search-maximal-spaces"))
            parameters.use_tree_search_maximal_spaces = vm["use-tree-search-maximal-spaces"].as<bool>();
        if (vm.count("use-milp-raster"))
            parameters.use_milp_raster = vm["use-milp-raster"].as<bool>();
        if (vm.count("use-local-search-linear-programming"))
            parameters.use_local_search_linear_programming = vm["use-local-search-linear-programming"].as<bool>();
        if (vm.count("use-local-search-nonlinear-programming"))
            parameters.use_local_search_nonlinear_programming = vm["use-local-search-nonlinear-programming"].as<bool>();
        if (vm.count("use-local-search-direct-trigonometry"))
            parameters.use_local_search_direct_trigonometry = vm["use-local-search-direct-trigonometry"].as<bool>();
        if (vm.count("use-local-search-black-box-optimization"))
            parameters.use_local_search_black_box_optimization = vm["use-local-search-black-box-optimization"].as<bool>();
        if (vm.count("use-sequential-single-knapsack"))
            parameters.use_sequential_single_knapsack = vm["use-sequential-single-knapsack"].as<bool>();
        if (vm.count("use-sequential-value-correction"))
            parameters.use_sequential_value_correction = vm["use-sequential-value-correction"].as<bool>();
        if (vm.count("use-column-generation"))
            parameters.use_column_generation = vm["use-column-generation"].as<bool>();
        if (vm.count("use-benders-decomposition"))
            parameters.use_benders_decomposition = vm["use-benders-decomposition"].as<bool>();
        if (vm.count("benders-decomposition-use-tree-search-relative-positions"))
            parameters.benders_decomposition_use_tree_search_relative_positions = vm["benders-decomposition-use-tree-search-relative-positions"].as<bool>();
        if (vm.count("use-tree-search-relative-positions"))
            parameters.use_tree_search_relative_positions = vm["use-tree-search-relative-positions"].as<bool>();
        if (vm.count("use-dichotomic-search"))
            parameters.use_dichotomic_search = vm["use-dichotomic-search"].as<bool>();
        if (vm.count("reduce"))
            parameters.reduction_parameters.reduce = vm["reduce"].as<bool>();

        if (vm.count("initial-maximum-approximation-ratio"))
            parameters.initial_maximum_approximation_ratio = vm["initial-maximum-approximation-ratio"].as<double>();
        if (vm.count("maximum-approximation-ratio-factor"))
            parameters.maximum_approximation_ratio_factor = vm["maximum-approximation-ratio-factor"].as<double>();
        if (vm.count("sequential-value-correction-subproblem-tree-search-queue-size"))
            parameters.sequential_value_correction_subproblem_tree_search_queue_size = vm["sequential-value-correction-subproblem-tree-search-queue-size"].as<NodeId>();
        if (vm.count("anytime-sequential-value-correction-growing-queue-maximum-number-of-iterations"))
            parameters.anytime_sequential_value_correction_growing_queue_maximum_number_of_iterations = vm["anytime-sequential-value-correction-growing-queue-maximum-number-of-iterations"].as<Counter>();
        if (vm.count("sequential-value-correction-restrict-bin-types"))
            parameters.sequential_value_correction_restrict_bin_types = vm["sequential-value-correction-restrict-bin-types"].as<bool>();
        if (vm.count("sequential-value-correction-scarce-item-types-profit-factor-exponent"))
            parameters.sequential_value_correction_scarce_item_types_profit_factor_exponent = vm["sequential-value-correction-scarce-item-types-profit-factor-exponent"].as<double>();
        if (vm.count("column-generation-subproblem-tree-search-queue-size"))
            parameters.column_generation_subproblem_tree_search_queue_size = vm["column-generation-subproblem-tree-search-queue-size"].as<NodeId>();
        if (vm.count("post-process-last-bin-maximum-number-of-items"))
            parameters.post_process_last_bin_maximum_number_of_items = vm["post-process-last-bin-maximum-number-of-items"].as<ItemPos>();
        if (vm.count("not-anytime-maximum-approximation-ratio"))
            parameters.not_anytime_maximum_approximation_ratio = vm["not-anytime-maximum-approximation-ratio"].as<double>();
        if (vm.count("not-anytime-tree-search-queue-size"))
            parameters.not_anytime_tree_search_queue_size = vm["not-anytime-tree-search-queue-size"].as<Counter>();
        if (vm.count("not-anytime-tree-search-periodic-packing-queue-size"))
            parameters.not_anytime_tree_search_periodic_packing_queue_size = vm["not-anytime-tree-search-periodic-packing-queue-size"].as<Counter>();
        if (vm.count("not-anytime-tree-search-maximal-spaces-queue-size"))
            parameters.not_anytime_tree_search_maximal_spaces_queue_size = vm["not-anytime-tree-search-maximal-spaces-queue-size"].as<Counter>();
        if (vm.count("not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size"))
            parameters.not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size = vm["not-anytime-sequential-single-knapsack-subproblem-tree-search-queue-size"].as<Counter>();
        if (vm.count("not-anytime-sequential-value-correction-number-of-iterations"))
            parameters.not_anytime_sequential_value_correction_number_of_iterations = vm["not-anytime-sequential-value-correction-number-of-iterations"].as<Counter>();
        if (vm.count("local-search-weighting-scheme"))
            parameters.local_search_weighting_scheme = vm["local-search-weighting-scheme"].as<LocalSearchWeightingScheme>();
        if (vm.count("not-anytime-local-search-maximum-number-of-failed-separations"))
            parameters.not_anytime_local_search_maximum_number_of_failed_separations = vm["not-anytime-local-search-maximum-number-of-failed-separations"].as<Counter>();
        if (vm.count("not-anytime-local-search-maximum-number-of-moves-factor"))
            parameters.not_anytime_local_search_maximum_number_of_moves_factor = vm["not-anytime-local-search-maximum-number-of-moves-factor"].as<Counter>();
        if (vm.count("anytime-sequential-single-knapsack-subproblem-tree-search-initial-queue-size"))
            parameters.anytime_sequential_single_knapsack_subproblem_tree_search_initial_queue_size = vm["anytime-sequential-single-knapsack-subproblem-tree-search-initial-queue-size"].as<NodeId>();
        if (vm.count("anytime-sequential-single-knapsack-subproblem-tree-search-periodic-packing-initial-queue-size"))
            parameters.anytime_sequential_single_knapsack_subproblem_tree_search_periodic_packing_initial_queue_size = vm["anytime-sequential-single-knapsack-subproblem-tree-search-periodic-packing-initial-queue-size"].as<NodeId>();
        if (vm.count("anytime-sequential-single-knapsack-subproblem-local-search-initial-maximum-number-of-moves-factor"))
            parameters.anytime_sequential_single_knapsack_subproblem_local_search_initial_maximum_number_of_moves_factor = vm["anytime-sequential-single-knapsack-subproblem-local-search-initial-maximum-number-of-moves-factor"].as<Counter>();
        if (vm.count("not-anytime-sequential-single-knapsack-subproblem-tree-search-periodic-packing-queue-size"))
            parameters.not_anytime_sequential_single_knapsack_subproblem_tree_search_periodic_packing_queue_size = vm["not-anytime-sequential-single-knapsack-subproblem-tree-search-periodic-packing-queue-size"].as<NodeId>();
        if (vm.count("not-anytime-sequential-single-knapsack-subproblem-local-search-maximum-number-of-moves-factor"))
            parameters.not_anytime_sequential_single_knapsack_subproblem_local_search_maximum_number_of_moves_factor = vm["not-anytime-sequential-single-knapsack-subproblem-local-search-maximum-number-of-moves-factor"].as<Counter>();
        if (vm.count("not-anytime-dichotomic-search-subproblem-tree-search-queue-size"))
            parameters.not_anytime_dichotomic_search_subproblem_tree_search_queue_size = vm["not-anytime-dichotomic-search-subproblem-tree-search-queue-size"].as<Counter>();
        const irregular::Output output = optimize(instance, parameters);

        if (vm.count("output"))
            output.write_json_output(vm["output"].as<std::string>());

        Solution solution = output.solution_pool.best();
        if (vm.count("group-identical-bins")) {
            GroupIdenticalBinsOutput gib_output = group_identical_bins(solution);
            solution = gib_output.solution_pool.best();
        }
        if (vm.count("anchor") && vm["anchor"].as<bool>()) {
            double anchor_x_weight = 1.0;
            double anchor_y_weight = 1.0;
            if (vm.count("anchor-x-weight"))
                anchor_x_weight = vm["anchor-x-weight"].as<double>();
            if (vm.count("anchor-y-weight"))
                anchor_y_weight = vm["anchor-y-weight"].as<double>();
            AnchorParameters atc_parameters;
            AnchorOutput atc_output = anchor(
                    solution,
                    anchor_x_weight,
                    anchor_y_weight,
                    atc_parameters);
            solution = atc_output.solution_pool.best();
            //solution.format(std::cout, 1);
        }

        if (vm.count("certificate"))
            solution.write(vm["certificate"].as<std::string>());

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

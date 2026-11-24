/**
 * WebAssembly module: JavaScript API of PackingSolver.
 *
 * Problem types: "rectangleguillotine", "rectangle", "box", "boxstacks",
 * "onedimensional" and "irregular". Instances are in the JSON format of
 * PackingSolver.
 *
 * Results are JSON strings:
 *
 *     {
 *         "output": ...,      // 'Output::to_json()'
 *         "certificate": ...  // the solution, in the CSV certificate format
 *     }
 *
 * or '{"error": ...}'.
 *
 * Two APIs:
 * - 'solve(problemType, instanceJson, parametersJson)' solves the instance
 *   and returns the result. It blocks the calling thread: in a browser, call
 *   it from a Web Worker (the main thread can't block).
 * - 'Session' solves an instance in a thread of its own, without blocking:
 *   'start(problemType, instanceJson, parametersJson)' starts the
 *   optimization, 'poll()' returns the latest update since the previous call
 *   and, at the end, the result, and 'stop()' stops the optimization. There
 *   is an update each time the best solution or a bound improves, but only
 *   the latest one since the previous call is returned: an update may have
 *   the same solution as the previous one, or an empty solution.
 *   'startReoptimizeBinLeftovers(problemType, instanceJson, certificate,
 *   binPos, parametersJson)' re-optimizes instead the leftovers of the bin
 *   'binPos' of a solution, given by its certificate (problem types
 *   "rectangle", "box", "boxstacks" and "irregular"). There is an update,
 *   in the format of the result, each time a solution with better leftovers
 *   is found; the result is:
 *
 *       {
 *           "solution": ...,     // 'Solution::to_json()'
 *           "certificate": ...,
 *           "improved": ...      // whether the leftovers are better
 *       }
 *
 *   The solution is the given one if the leftovers aren't better.
 *
 * The threads of the algorithms run in a pool of Web Workers started when
 * the module is loaded.
 *
 * Parameters (all optional):
 * - "optimization_mode": "anytime" (default), "not-anytime",
 *   "not-anytime-deterministic" or "not-anytime-sequential" (single-threaded);
 *   in "anytime" mode, the algorithms run until the time limit, unless the
 *   solution is proven optimal or the session is stopped;
 * - "time_limit": time limit in seconds;
 * - "verbosity_level": verbosity level, printed on the console.
 */

#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/optimize.hpp"
#include "packingsolver/boxstacks/instance_builder.hpp"
#include "packingsolver/boxstacks/optimize.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"
#include "packingsolver/rectangle/post_process.hpp"
#include "packingsolver/box/post_process.hpp"
#include "packingsolver/boxstacks/post_process.hpp"
#include "packingsolver/irregular/post_process.hpp"
#include "rectangle/solution_builder.hpp"
#include "box/solution_builder.hpp"
#include "boxstacks/solution_builder.hpp"
#include "irregular/solution_builder.hpp"

#include <emscripten/bind.h>

#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <type_traits>

using namespace packingsolver;

namespace
{

template <typename OptimizeParameters>
OptimizeParameters read_parameters(const std::string& parameters_json)
{
    OptimizeParameters parameters;
    parameters.verbosity_level = 0;
    // Only HiGHS is available in the module.
    parameters.linear_programming_solver_name = columngenerationsolver::SolverName::Highs;

    if (parameters_json.empty())
        return parameters;
    nlohmann::json json = nlohmann::json::parse(parameters_json);
    if (json.contains("optimization_mode")) {
        std::stringstream ss(json["optimization_mode"].get<std::string>());
        ss >> parameters.optimization_mode;
        if (ss.fail()) {
            throw std::invalid_argument(
                    "solve: invalid optimization mode: "
                    + json["optimization_mode"].dump() + ".");
        }
    }
    if (json.contains("time_limit"))
        parameters.timer.set_time_limit(json["time_limit"].get<double>());
    if (json.contains("verbosity_level"))
        parameters.verbosity_level = json["verbosity_level"].get<int>();
    return parameters;
}

template <typename Output>
nlohmann::json to_result(const Output& output)
{
    std::stringstream certificate;
    output.solution_pool.best().write(certificate);
    nlohmann::json result;
    result["output"] = output.to_json();
    result["certificate"] = certificate.str();
    return result;
}

/**
 * Read an instance. It is shared: the results of the updates, computed
 * later, need it.
 */
template <typename InstanceBuilder>
auto read_instance(const std::string& instance_json)
{
    InstanceBuilder instance_builder;
    std::istringstream instance_stream(instance_json);
    instance_builder.read(instance_stream);
    using Instance = typename std::decay<decltype(instance_builder.build())>::type;
    return std::make_shared<const Instance>(instance_builder.build());
}

/**
 * Called each time the best solution or a bound improves, with a function
 * returning the result.
 *
 * The result is only computed when needed: the algorithms may find many
 * solutions, and writing each of them would slow them down.
 */
using SolutionCallback = std::function<void(const std::function<nlohmann::json()>&)>;

/**
 * Read an instance of a problem type, solve it and return the result.
 *
 * 'end', if not null, stops the optimization when set to 'true'.
 */
template <typename InstanceBuilder, typename OptimizeParameters, typename OptimizeFunction>
nlohmann::json run(
        OptimizeFunction optimize_function,
        const std::string& instance_json,
        const std::string& parameters_json,
        const SolutionCallback& solution_callback,
        bool* end)
{
    const auto instance = read_instance<InstanceBuilder>(instance_json);
    OptimizeParameters parameters = read_parameters<OptimizeParameters>(parameters_json);
    // The timer starts when the parameters are constructed; start it now.
    parameters.timer.reset_time();
    if (end != nullptr)
        parameters.timer.add_end_boolean(end);
    if (solution_callback) {
        parameters.new_solution_callback = [&solution_callback, instance](const auto& output)
        {
            using OutputType = typename std::decay<decltype(output)>::type;
            std::shared_ptr<const OutputType> output_copy
                = std::make_shared<const OutputType>(output);
            // The instance is kept until the result is computed: it may be
            // computed after the end of the optimization.
            solution_callback([output_copy, instance]() { return to_result(*output_copy); });
        };
    }
    const auto output = optimize_function(*instance, parameters);
    return to_result(output);
}

nlohmann::json run(
        const std::string& problem_type,
        const std::string& instance_json,
        const std::string& parameters_json,
        const SolutionCallback& solution_callback = nullptr,
        bool* end = nullptr)
{
#define PACKINGSOLVER_WASM_RUN(problem_type_namespace) \
    if (problem_type == #problem_type_namespace) { \
        return run<problem_type_namespace::InstanceBuilder, problem_type_namespace::OptimizeParameters>( \
                [](const problem_type_namespace::Instance& instance, \
                    const problem_type_namespace::OptimizeParameters& parameters) \
                { \
                    return problem_type_namespace::optimize(instance, parameters); \
                }, \
                instance_json, \
                parameters_json, \
                solution_callback, \
                end); \
    }
    PACKINGSOLVER_WASM_RUN(rectangleguillotine)
    PACKINGSOLVER_WASM_RUN(rectangle)
    PACKINGSOLVER_WASM_RUN(box)
    PACKINGSOLVER_WASM_RUN(boxstacks)
    PACKINGSOLVER_WASM_RUN(onedimensional)
    PACKINGSOLVER_WASM_RUN(irregular)
#undef PACKINGSOLVER_WASM_RUN
    throw std::invalid_argument(
            "solve: unsupported problem type: \"" + problem_type + "\".");
}

/** Result of a re-optimization of the leftovers of a bin. */
template <typename Solution>
nlohmann::json reoptimize_result(
        const Solution& solution,
        bool improved)
{
    std::stringstream certificate;
    solution.write(certificate);
    nlohmann::json result;
    result["solution"] = solution.to_json();
    result["certificate"] = certificate.str();
    result["improved"] = improved;
    return result;
}

/**
 * As in the leftover post-process: a solution is better if it is not worse
 * and if its leftovers are better.
 */
template <typename Solution>
bool better_leftovers(
        const Solution& solution,
        const Solution& reference)
{
    return !(solution < reference) && solution.leftovers_better(reference);
}

/**
 * Read an instance and a solution of a problem type, re-optimize the
 * leftovers of a bin of the solution and return the result.
 *
 * 'solution_callback', if set, is called each time a solution better than
 * the given one and than the previous ones is found.
 *
 * 'end', if not null, stops the re-optimization when set to 'true'.
 */
template <typename InstanceBuilder, typename SolutionBuilder, typename OptimizeParameters, typename ReoptimizeFunction>
nlohmann::json reoptimize_bin_leftovers(
        ReoptimizeFunction reoptimize_function,
        const std::string& instance_json,
        const std::string& certificate,
        int bin_pos,
        const std::string& parameters_json,
        const SolutionCallback& solution_callback,
        bool* end)
{
    const auto instance = read_instance<InstanceBuilder>(instance_json);
    SolutionBuilder solution_builder(*instance);
    std::istringstream certificate_stream(certificate);
    solution_builder.read(certificate_stream);
    const auto solution = solution_builder.build();
    using Solution = typename std::decay<decltype(solution)>::type;
    if (bin_pos < 0 || bin_pos >= solution.number_of_different_bins()) {
        throw std::invalid_argument(
                "reoptimize_bin_leftovers: invalid bin: "
                + std::to_string(bin_pos) + ".");
    }
    OptimizeParameters parameters = read_parameters<OptimizeParameters>(parameters_json);
    parameters.timer.reset_time();
    if (end != nullptr)
        parameters.timer.add_end_boolean(end);

    // The best solution reported, the given one at first.
    std::mutex best_mutex;
    std::shared_ptr<const Solution> best = std::make_shared<const Solution>(solution);
    if (solution_callback) {
        parameters.new_solution_callback = [&solution_callback, &best_mutex, &best, instance](
                const auto& output)
        {
            const Solution& new_solution = output.solution_pool.best();
            std::lock_guard<std::mutex> lock(best_mutex);
            if (!better_leftovers(new_solution, *best))
                return;
            best = std::make_shared<const Solution>(new_solution);
            std::shared_ptr<const Solution> best_copy = best;
            // The instance is kept until the result is computed: it may be
            // computed after the end of the re-optimization.
            solution_callback([best_copy, instance]() { return reoptimize_result(*best_copy, true); });
        };
    }

    const Solution new_solution = reoptimize_function(solution, bin_pos, parameters);
    bool improved = better_leftovers(new_solution, solution);
    return reoptimize_result((improved)? new_solution: solution, improved);
}

nlohmann::json reoptimize_bin_leftovers(
        const std::string& problem_type,
        const std::string& instance_json,
        const std::string& certificate,
        int bin_pos,
        const std::string& parameters_json,
        const SolutionCallback& solution_callback,
        bool* end)
{
#define PACKINGSOLVER_WASM_REOPTIMIZE(problem_type_namespace) \
    if (problem_type == #problem_type_namespace) { \
        return reoptimize_bin_leftovers< \
                problem_type_namespace::InstanceBuilder, \
                problem_type_namespace::SolutionBuilder, \
                problem_type_namespace::OptimizeParameters>( \
                [](const problem_type_namespace::Solution& solution, \
                    BinPos bin_pos, \
                    const problem_type_namespace::OptimizeParameters& parameters) \
                { \
                    return problem_type_namespace::reoptimize_bin_leftovers( \
                            solution, bin_pos, parameters); \
                }, \
                instance_json, \
                certificate, \
                bin_pos, \
                parameters_json, \
                solution_callback, \
                end); \
    }
    PACKINGSOLVER_WASM_REOPTIMIZE(rectangle)
    PACKINGSOLVER_WASM_REOPTIMIZE(box)
    PACKINGSOLVER_WASM_REOPTIMIZE(boxstacks)
    PACKINGSOLVER_WASM_REOPTIMIZE(irregular)
#undef PACKINGSOLVER_WASM_REOPTIMIZE
    throw std::invalid_argument(
            "reoptimize_bin_leftovers: unsupported problem type: \""
            + problem_type + "\".");
}

nlohmann::json error_result(const std::exception& e)
{
    nlohmann::json result;
    result["error"] = e.what();
    return result;
}

/**
 * Errors are returned as JSON: C++ exceptions only reach JavaScript as
 * opaque pointers.
 */
std::string solve(
        const std::string& problem_type,
        const std::string& instance_json,
        const std::string& parameters_json)
{
    try {
        return run(problem_type, instance_json, parameters_json).dump();
    } catch (const std::exception& e) {
        return error_result(e).dump();
    }
}

/**
 * Optimization running in a thread of its own.
 *
 * The new solutions are found in the threads of the algorithms, which can't
 * call JavaScript functions of the thread owning the session: they are
 * queued, and returned by 'poll'.
 */
class Session
{

public:

    ~Session()
    {
        stop();
        if (thread_.joinable())
            thread_.join();
    }

    /**
     * Start solving an instance.
     *
     * Returns an empty string, or an error as JSON if the session was
     * already started.
     */
    std::string start(
            const std::string& problem_type,
            const std::string& instance_json,
            const std::string& parameters_json)
    {
        return start_task([this, problem_type, instance_json, parameters_json]()
        {
            return run(
                    problem_type,
                    instance_json,
                    parameters_json,
                    queue_solution(),
                    &end_);
        });
    }

    /**
     * Start re-optimizing the leftovers of the bin 'bin_pos' of a solution,
     * given by its certificate.
     *
     * Returns an empty string, or an error as JSON if the session was
     * already started.
     */
    std::string start_reoptimize_bin_leftovers(
            const std::string& problem_type,
            const std::string& instance_json,
            const std::string& certificate,
            int bin_pos,
            const std::string& parameters_json)
    {
        return start_task([this, problem_type, instance_json, certificate, bin_pos, parameters_json]()
        {
            return reoptimize_bin_leftovers(
                    problem_type,
                    instance_json,
                    certificate,
                    bin_pos,
                    parameters_json,
                    queue_solution(),
                    &end_);
        });
    }

    /**
     * Return, as JSON:
     * - "solutions": the latest update since the previous call, if any (the
     *   best solution or a bound improved);
     * - "done": 'true' once the optimization has ended;
     * - "result": then, the final result (or "error").
     */
    std::string poll()
    {
        nlohmann::json poll_result;
        poll_result["solutions"] = nlohmann::json::array();
        bool done = false;
        std::function<nlohmann::json()> latest_solution;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            latest_solution = latest_solution_;
            latest_solution_ = nullptr;
            done = done_;
            if (done)
                poll_result["result"] = result_;
        }
        // Computed outside of the lock, not to block the algorithms.
        if (latest_solution)
            poll_result["solutions"].push_back(latest_solution());
        poll_result["done"] = done;
        if (done && thread_.joinable())
            thread_.join();
        return poll_result.dump();
    }

    /** Stop the optimization: it then returns its best solution. */
    void stop()
    {
        end_ = true;
    }

private:

    /** Callback queuing an update, returned by 'poll'. */
    SolutionCallback queue_solution()
    {
        return [this](const std::function<nlohmann::json()>& solution)
        {
            // Only the latest update is kept: the page only shows the
            // latest one.
            std::lock_guard<std::mutex> lock(mutex_);
            latest_solution_ = solution;
        };
    }

    /** Run 'task' in the thread of the session; its result ends it. */
    std::string start_task(const std::function<nlohmann::json()>& task)
    {
        if (started_) {
            nlohmann::json result;
            result["error"] = "Session.start: the session was already started.";
            return result.dump();
        }
        started_ = true;
        thread_ = std::thread([this, task]()
        {
            nlohmann::json result;
            try {
                result = task();
            } catch (const std::exception& e) {
                result = error_result(e);
            }
            std::lock_guard<std::mutex> lock(mutex_);
            result_ = result;
            done_ = true;
        });
        return "";
    }

    /** Thread of the optimization. */
    std::thread thread_;

    /** 'true' once 'start' has been called. */
    bool started_ = false;

    /** Set to 'true' to stop the optimization. */
    bool end_ = false;

    /** Protects the members below. */
    std::mutex mutex_;

    /** Latest update, not returned by 'poll' yet. */
    std::function<nlohmann::json()> latest_solution_;

    /** 'true' once the optimization has ended. */
    bool done_ = false;

    /** Final result. */
    nlohmann::json result_;

};

}

EMSCRIPTEN_BINDINGS(packingsolver)
{
    emscripten::function("solve", &solve);
    emscripten::class_<Session>("Session")
        .constructor<>()
        .function("start", &Session::start)
        .function("startReoptimizeBinLeftovers", &Session::start_reoptimize_bin_leftovers)
        .function("poll", &Session::poll)
        .function("stop", &Session::stop);
}

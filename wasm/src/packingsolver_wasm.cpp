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
 *   optimization, 'poll()' returns the updates since the previous call and,
 *   at the end, the result, and 'stop()' stops the optimization. There is an
 *   update each time the best solution or a bound improves: an update may
 *   have the same solution as the previous one, or an empty solution.
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

#include <emscripten/bind.h>

#include <functional>
#include <mutex>
#include <sstream>
#include <thread>

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
 * Called with the result each time the best solution or a bound improves.
 */
using SolutionCallback = std::function<void(const nlohmann::json&)>;

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
    InstanceBuilder instance_builder;
    std::istringstream instance_stream(instance_json);
    instance_builder.read(instance_stream);
    const auto instance = instance_builder.build();
    OptimizeParameters parameters = read_parameters<OptimizeParameters>(parameters_json);
    // The timer starts when the parameters are constructed; start it now.
    parameters.timer.reset_time();
    if (end != nullptr)
        parameters.timer.add_end_boolean(end);
    if (solution_callback) {
        parameters.new_solution_callback = [&solution_callback](const auto& output)
        {
            solution_callback(to_result(output));
        };
    }
    const auto output = optimize_function(instance, parameters);
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
        if (started_) {
            nlohmann::json result;
            result["error"] = "Session.start: the session was already started.";
            return result.dump();
        }
        started_ = true;
        thread_ = std::thread([this, problem_type, instance_json, parameters_json]()
        {
            nlohmann::json result;
            try {
                result = run(
                        problem_type,
                        instance_json,
                        parameters_json,
                        [this](const nlohmann::json& solution)
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            solutions_.push_back(solution);
                        },
                        &end_);
            } catch (const std::exception& e) {
                result = error_result(e);
            }
            std::lock_guard<std::mutex> lock(mutex_);
            result_ = result;
            done_ = true;
        });
        return "";
    }

    /**
     * Return, as JSON:
     * - "solutions": the updates since the previous call (each time the best
 *   solution or a bound improved);
     * - "done": 'true' once the optimization has ended;
     * - "result": then, the final result (or "error").
     */
    std::string poll()
    {
        nlohmann::json poll_result;
        poll_result["solutions"] = nlohmann::json::array();
        bool done = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (nlohmann::json& solution: solutions_)
                poll_result["solutions"].push_back(std::move(solution));
            solutions_.clear();
            done = done_;
            if (done)
                poll_result["result"] = result_;
        }
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

    /** Thread of the optimization. */
    std::thread thread_;

    /** 'true' once 'start' has been called. */
    bool started_ = false;

    /** Set to 'true' to stop the optimization. */
    bool end_ = false;

    /** Protects the members below. */
    std::mutex mutex_;

    /** Updates, not returned by 'poll' yet. */
    std::vector<nlohmann::json> solutions_;

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
        .function("poll", &Session::poll)
        .function("stop", &Session::stop);
}

/**
 * WebAssembly module: JavaScript API of PackingSolver.
 *
 * 'solve(problemType, instanceJson, parametersJson)' reads an instance in the
 * JSON format of PackingSolver, solves it and returns a JSON string:
 *
 *     {
 *         "output": ...,      // 'Output::to_json()'
 *         "certificate": ...  // the solution, in the CSV certificate format
 *     }
 *
 * 'solve' blocks the calling thread: in a browser, call it from a Web Worker
 * (the main thread can't block). The threads of the algorithms run in a pool
 * of Web Workers started when the module is loaded.
 *
 * Parameters (all optional):
 * - "optimization_mode": "anytime" (default), "not-anytime",
 *   "not-anytime-deterministic" or "not-anytime-sequential" (single-threaded);
 *   in "anytime" mode, the algorithms run until the time limit, unless the
 *   solution is proven optimal;
 * - "time_limit": time limit in seconds;
 * - "verbosity_level": verbosity level, printed on the console.
 */

#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"

#include <emscripten/bind.h>

#include <sstream>

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
std::string to_result(const Output& output)
{
    std::stringstream certificate;
    output.solution_pool.best().write(certificate);
    nlohmann::json result;
    result["output"] = output.to_json();
    result["certificate"] = certificate.str();
    return result.dump();
}

std::string solve_rectangle(
        const std::string& instance_json,
        const std::string& parameters_json)
{
    rectangle::InstanceBuilder instance_builder;
    std::istringstream instance_stream(instance_json);
    instance_builder.read(instance_stream);
    const rectangle::Instance instance = instance_builder.build();
    const rectangle::OptimizeParameters parameters
        = read_parameters<rectangle::OptimizeParameters>(parameters_json);
    const rectangle::Output output = rectangle::optimize(instance, parameters);
    return to_result(output);
}

std::string solve(
        const std::string& problem_type,
        const std::string& instance_json,
        const std::string& parameters_json)
{
    if (problem_type == "rectangle")
        return solve_rectangle(instance_json, parameters_json);
    throw std::invalid_argument(
            "solve: unsupported problem type: \"" + problem_type + "\".");
}

/**
 * Wrapper returning errors as a JSON string: C++ exceptions only reach
 * JavaScript as opaque pointers.
 */
std::string solve_or_error(
        const std::string& problem_type,
        const std::string& instance_json,
        const std::string& parameters_json)
{
    try {
        return solve(problem_type, instance_json, parameters_json);
    } catch (const std::exception& e) {
        nlohmann::json result;
        result["error"] = e.what();
        return result.dump();
    }
}

}

EMSCRIPTEN_BINDINGS(packingsolver)
{
    emscripten::function("solve", &solve_or_error);
}

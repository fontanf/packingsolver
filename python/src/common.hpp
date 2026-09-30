#pragma once

/**
 * Helpers shared by the Python bindings of every problem type.
 *
 * Each problem type 'X' is exposed in its own submodule 'packingsolver.X'
 * with the same shape as its C++ namespace: 'InstanceBuilder' -> 'Instance'
 * -> 'optimize(instance, parameters)' -> 'Output' (-> 'Solution').
 *
 * Lifetimes: a C++ 'Solution' only stores a pointer to its 'Instance', so on
 * the Python side instances are held by 'std::shared_ptr' and every 'Output'
 * is wrapped in an 'OutputHandle' that keeps its instance alive; 'Solution'
 * objects are only ever handed out as references into an 'OutputHandle'.
 */

#include "packingsolver/algorithms/common.hpp"

#include "columngenerationsolver/commons.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <chrono>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

namespace packingsolver
{
namespace python
{

namespace nb = nanobind;

void bind_common(nb::module_& m);

/** Raise 'IndexError' unless '0 <= index < size'. */
inline void check_index(int64_t index, int64_t size, const char* name)
{
    if (index < 0 || index >= size)
        throw nb::index_error((std::string(name) + " out of range.").c_str());
}

/** Convert a JSON value into the equivalent Python object. */
nb::object json_to_python(const nlohmann::json& json);

/** An 'Output' together with the instance its solutions point to. */
template <typename Instance, typename Output>
struct OutputHandle
{
    std::shared_ptr<const Instance> instance;
    Output output;
};

/**
 * Holder for a Python callable stored in a C++ 'std::function'.
 *
 * The callable is kept behind a 'shared_ptr' so that copying the
 * 'std::function' (which the algorithms do freely, from threads not
 * holding the GIL) never touches the Python reference count; the last copy
 * releases the callable with the GIL acquired.
 */
struct PythonCallable
{
    std::shared_ptr<nb::object> function;

    explicit PythonCallable(nb::object function_object):
        function(
                new nb::object(std::move(function_object)),
                [](nb::object* p) { nb::gil_scoped_acquire acquire; delete p; }) { }
};

/**
 * Functor stored in 'Parameters::new_solution_callback' when the user sets
 * a Python callback. 'run_optimize' replaces it with the actual C++ ->
 * Python bridge, since that bridge needs the 'shared_ptr' to the instance.
 */
template <typename Output>
struct PythonNewSolutionCallback: PythonCallable
{
    using PythonCallable::PythonCallable;

    void operator()(const Output&) const
    {
        throw std::logic_error(
                "packingsolver: Python new_solution_callback called outside of 'optimize'.");
    }
};

/** 'Solution.write(certificate_path)'. */
template <typename Solution>
void solution_write_path(
        const Solution& solution,
        const std::string& certificate_path)
{
    solution.write(certificate_path);
}

/**
 * 'Solution.write(stream)': write the certificate to a Python text stream
 * (any object with a 'write(str)' method, e.g. 'io.StringIO'). Path-like
 * objects (e.g. 'pathlib.Path') are written to the file they designate.
 */
template <typename Solution>
void solution_write_stream(
        const Solution& solution,
        nb::handle stream)
{
    if (nb::hasattr(stream, "__fspath__")) {
        nb::object path = nb::module_::import_("os").attr("fspath")(stream);
        solution.write(nb::cast<std::string>(path));
        return;
    }
    if (!nb::hasattr(stream, "write"))
        throw nb::type_error("expected a path or a text stream with a 'write' method.");
    std::ostringstream ss;
    solution.write(ss);
    stream.attr("write")(ss.str());
}

/** Bind a read-only 'Output' field of an 'OutputHandle'. */
template <typename Handle, typename Output, typename T>
void def_output_field(
        nb::class_<Handle>& cls,
        const char* name,
        T Output::* member)
{
    cls.def_prop_ro(name, [member](const Handle& handle) { return handle.output.*member; });
}

/** Bind the members shared by every problem type's 'Output'. */
template <typename Instance, typename Solution, typename Output>
nb::class_<OutputHandle<Instance, Output>> bind_output(
        nb::module_& m)
{
    using Handle = OutputHandle<Instance, Output>;
    nb::class_<Handle> cls(m, "Output");
    cls.def_prop_ro(
            "solution",
            [](const Handle& handle) -> const Solution& { return handle.output.solution_pool.best(); },
            nb::rv_policy::reference_internal,
            "Best solution found.");
    cls.def_prop_ro(
            "solution_label",
            [](const Handle& handle) { return handle.output.solution_pool.best_label(); },
            "Name of the algorithm that found the best solution.");
    cls.def_prop_ro(
            "solutions",
            [](nb::handle self) {
                // Each entry references into 'self', so keep it alive.
                const Handle& handle = nb::cast<const Handle&>(self);
                nb::list list;
                for (const auto& entry: handle.output.solution_pool.solutions())
                    list.append(nb::cast(&entry.solution, nb::rv_policy::reference_internal, self));
                return list;
            },
            "Solutions of the solution pool, best first (see 'maximum_size_of_the_solution_pool').");
    cls.def_prop_ro("time", [](const Handle& handle) { return handle.output.time; });
    cls.def("is_proven_optimal", [](const Handle& handle) { return handle.output.is_proven_optimal(); });
    cls.def(
            "to_json",
            [](const Handle& handle) { return json_to_python(handle.output.to_json()); },
            "Return the output as a dict (same content as the CLI '--output' file).");
    cls.def(
            "__repr__",
            [](const Handle& handle) {
                std::stringstream ss;
                handle.output.format(ss);
                return ss.str();
            });
    return cls;
}

/**
 * Default 'OptimizeParameters' of the Python module, used both by the Python
 * constructor and as the default argument of 'optimize'.
 */
template <typename OptimizeParameters>
OptimizeParameters default_optimize_parameters()
{
    OptimizeParameters parameters;
#if !PACKINGSOLVER_PYTHON_USE_CLP
    // The C++ default, CLP, is not available in the Python module.
    parameters.linear_programming_solver_name = columngenerationsolver::SolverName::Highs;
#endif
    return parameters;
}

/**
 * Garbage collector support of 'OptimizeParameters': it can own a Python
 * callable (the new solution callback), which usually references the
 * parameters object back through its globals. Without these slots, the
 * garbage collector can't see that reference, and such cycles are never
 * collected.
 */
template <typename Output, typename OptimizeParameters>
int parameters_tp_traverse(PyObject* self, visitproc visit, void* arg)
{
    Py_VISIT(Py_TYPE(self));
    if (!nb::inst_ready(self))
        return 0;
    const OptimizeParameters* parameters = nb::inst_ptr<OptimizeParameters>(self);
    auto* callback = parameters->new_solution_callback.template target<PythonNewSolutionCallback<Output>>();
    if (callback != nullptr)
        Py_VISIT(callback->function->ptr());
    return 0;
}

template <typename Output, typename OptimizeParameters>
int parameters_tp_clear(PyObject* self)
{
    OptimizeParameters* parameters = nb::inst_ptr<OptimizeParameters>(self);
    parameters->new_solution_callback = [](const Output&) { };
    return 0;
}

/**
 * Create a problem type's 'OptimizeParameters' class, with the members shared
 * by every problem type.
 */
template <typename Output, typename OptimizeParameters>
nb::class_<OptimizeParameters> bind_parameters_base(nb::module_& m)
{
    static PyType_Slot slots[] = {
        {Py_tp_traverse, (void*)parameters_tp_traverse<Output, OptimizeParameters>},
        {Py_tp_clear, (void*)parameters_tp_clear<Output, OptimizeParameters>},
        {0, nullptr}};
    nb::class_<OptimizeParameters> cls(m, "OptimizeParameters", nb::type_slots(slots));
    cls.def(
            "__init__",
            [](OptimizeParameters* parameters) {
                new (parameters) OptimizeParameters(default_optimize_parameters<OptimizeParameters>());
            });
    cls.def_prop_rw(
            "time_limit",
            [](const OptimizeParameters& p) { return p.timer.time_limit(); },
            [](OptimizeParameters& p, double time_limit) { p.timer.set_time_limit(time_limit); },
            "Time limit in seconds (default: infinite).");
    cls.def_rw("verbosity_level", &OptimizeParameters::verbosity_level);
    cls.def_rw("messages_to_stdout", &OptimizeParameters::messages_to_stdout);
    cls.def_rw("messages_path", &OptimizeParameters::messages_path);
    cls.def_rw("log_path", &OptimizeParameters::log_path);
    cls.def_rw("log_to_stderr", &OptimizeParameters::log_to_stderr);
    cls.def_rw("maximum_size_of_the_solution_pool", &OptimizeParameters::maximum_size_of_the_solution_pool);
    cls.def_rw("json_search_tree_path", &OptimizeParameters::json_search_tree_path);
    cls.def_rw("mps_prefix", &OptimizeParameters::mps_prefix);
    cls.def_prop_rw(
            "new_solution_callback",
            [](const OptimizeParameters& p) -> nb::object {
                auto* callback = p.new_solution_callback.template target<PythonNewSolutionCallback<Output>>();
                return (callback)? *callback->function: nb::none();
            },
            [](OptimizeParameters& p, nb::object function) {
                if (function.is_none()) {
                    p.new_solution_callback = [](const Output&) { };
                    return;
                }
                if (!PyCallable_Check(function.ptr()))
                    throw nb::type_error("new_solution_callback must be callable or None.");
                p.new_solution_callback = PythonNewSolutionCallback<Output>(function);
            },
            nb::for_setter(nb::arg("function").none()),
            "Callable 'f(output)' called each time a new best solution is found.");
    return cls;
}

/**
 * Run 'optimize_function(*instance, parameters)' from Python.
 *
 * The optimization runs in a worker thread with the GIL released, so that
 * the calling thread can poll for signals: Ctrl+C stops the algorithms (via
 * an end boolean registered in the timer) and raises 'KeyboardInterrupt'.
 * An exception raised by the Python callback also stops the algorithms and
 * is re-raised once they have returned.
 */
template <typename Instance, typename Output, typename OptimizeParameters, typename OptimizeFunction>
OutputHandle<Instance, Output> run_optimize(
        std::shared_ptr<const Instance> instance,
        const OptimizeParameters& parameters_in,
        OptimizeFunction optimize_function)
{
    OptimizeParameters parameters = parameters_in;
    // The timer starts when the parameters are constructed; start it now.
    parameters.timer.reset_time();
    bool end = false;
    parameters.timer.add_end_boolean(&end);

    // Always accessed with the GIL held.
    std::exception_ptr callback_exception;
    auto* python_callback = parameters.new_solution_callback.template target<PythonNewSolutionCallback<Output>>();
    if (python_callback != nullptr) {
        std::shared_ptr<nb::object> function = python_callback->function;
        parameters.new_solution_callback = [function, instance, &end, &callback_exception](
                const Output& output)
        {
            nb::gil_scoped_acquire acquire;
            if (callback_exception)
                return;
            try {
                (*function)(OutputHandle<Instance, Output>{instance, output});
            } catch (...) {
                callback_exception = std::current_exception();
                end = true;
            }
        };
    }

    std::unique_ptr<Output> output;
    std::exception_ptr optimize_exception;
    std::exception_ptr interrupt_exception;
    {
        nb::gil_scoped_release release;
        std::mutex mutex;
        std::condition_variable condition;
        bool done = false;
        std::thread worker([&]() {
            try {
                output.reset(new Output(optimize_function(*instance, parameters)));
            } catch (...) {
                optimize_exception = std::current_exception();
            }
            std::lock_guard<std::mutex> lock(mutex);
            done = true;
            condition.notify_one();
        });
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mutex);
                if (condition.wait_for(lock, std::chrono::milliseconds(100), [&done]() { return done; }))
                    break;
            }
            if (interrupt_exception)
                continue;
            nb::gil_scoped_acquire acquire;
            if (PyErr_CheckSignals() != 0) {
                interrupt_exception = std::make_exception_ptr(nb::python_error());
                end = true;
            }
        }
        worker.join();
    }

    if (interrupt_exception)
        std::rethrow_exception(interrupt_exception);
    if (callback_exception)
        std::rethrow_exception(callback_exception);
    if (optimize_exception)
        std::rethrow_exception(optimize_exception);
    return OutputHandle<Instance, Output>{instance, std::move(*output)};
}

}
}

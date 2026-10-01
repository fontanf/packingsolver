#include "common.hpp"

#include "columngenerationsolver/commons.hpp"

using namespace packingsolver;
using namespace packingsolver::python;

nb::object packingsolver::python::json_to_python(const nlohmann::json& json)
{
    switch (json.type()) {
    case nlohmann::json::value_t::null:
    case nlohmann::json::value_t::discarded:
        return nb::none();
    case nlohmann::json::value_t::boolean:
        return nb::bool_(json.get<bool>());
    case nlohmann::json::value_t::number_integer:
        return nb::int_(json.get<int64_t>());
    case nlohmann::json::value_t::number_unsigned:
        return nb::int_(json.get<uint64_t>());
    case nlohmann::json::value_t::number_float:
        return nb::float_(json.get<double>());
    case nlohmann::json::value_t::string:
        return nb::str(json.get_ref<const std::string&>().c_str());
    case nlohmann::json::value_t::array: {
        nb::list list;
        for (const nlohmann::json& value: json)
            list.append(json_to_python(value));
        return list;
    } case nlohmann::json::value_t::object: {
        nb::dict dict;
        for (auto it = json.begin(); it != json.end(); ++it)
            dict[nb::str(it.key().c_str())] = json_to_python(it.value());
        return dict;
    } default:
        throw std::logic_error("packingsolver: unsupported JSON value type.");
    }
}

void packingsolver::python::bind_common(nb::module_& m)
{
    nb::enum_<Objective>(m, "Objective")
        .value("BinPacking", Objective::BinPacking)
        .value("BinPackingWithLeftovers", Objective::BinPackingWithLeftovers)
        .value("OpenDimensionX", Objective::OpenDimensionX)
        .value("OpenDimensionY", Objective::OpenDimensionY)
        .value("OpenDimensionZ", Objective::OpenDimensionZ)
        .value("OpenDimensionXY", Objective::OpenDimensionXY)
        .value("Knapsack", Objective::Knapsack)
        .value("VariableSizedBinPacking", Objective::VariableSizedBinPacking)
        .value("SequentialOneDimensionalRectangleSubproblem", Objective::SequentialOneDimensionalRectangleSubproblem)
        .value("Feasibility", Objective::Feasibility)
        .value("BinPackingCuttingCost", Objective::BinPackingCuttingCost);

    nb::enum_<OptimizationMode>(m, "OptimizationMode")
        .value("Anytime", OptimizationMode::Anytime)
        .value("NotAnytime", OptimizationMode::NotAnytime)
        .value("NotAnytimeDeterministic", OptimizationMode::NotAnytimeDeterministic)
        .value("NotAnytimeSequential", OptimizationMode::NotAnytimeSequential);

    nb::enum_<InstanceFormat>(m, "InstanceFormat")
        .value("Csv", InstanceFormat::Csv)
        .value("Json", InstanceFormat::Json);

    nb::enum_<columngenerationsolver::SolverName>(m, "LinearProgrammingSolver")
        .value("CLP", columngenerationsolver::SolverName::CLP)
        .value("Highs", columngenerationsolver::SolverName::Highs)
        .value("CPLEX", columngenerationsolver::SolverName::CPLEX)
        .value("Xpress", columngenerationsolver::SolverName::Xpress)
        .value("Knitro", columngenerationsolver::SolverName::Knitro);
}

#pragma once

#include "irregular/branching_scheme.hpp"

namespace packingsolver
{
namespace irregular
{

std::vector<TrapezoidSet> trapezoid_sets_simplification(
        const Instance& instance,
        const std::vector<TrapezoidSet>& trapezoid_sets);

}
}
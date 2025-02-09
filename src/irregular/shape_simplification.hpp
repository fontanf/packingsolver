#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

Instance shape_simplification(
        const Instance& instance,
        double maximum_approximation_ratio);

}
}

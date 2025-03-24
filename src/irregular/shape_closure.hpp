#pragma once

#include "packingsolver/irregular/shape.hpp"

namespace packingsolver
{
namespace irregular
{

Shape close_inflated_elements(const std::vector<ShapeElement>& inflated_elements, bool is_deflating = false);

}
}

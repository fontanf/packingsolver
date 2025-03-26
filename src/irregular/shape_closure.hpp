#pragma once

#include "packingsolver/irregular/shape.hpp"

namespace packingsolver
{
namespace irregular
{

Shape close_inflated_elements(
        const std::vector<ShapeElement>& inflated_elements, 
        const std::vector<std::pair<ShapeElement, ShapeElement>>& original_to_inflated_mapping = {},
        bool is_deflating = false);

}
}

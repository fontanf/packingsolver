#include "packingsolver/algorithms/resource.hpp"

using namespace packingsolver;

bool packingsolver::same_copy_dependent_last_bin_copies(
        const std::vector<std::vector<bool>>& copy_dependent_resource_consumptions,
        BinTypeId bin_type_id,
        const std::vector<ItemPos>& last_bin_item_number_of_copies_1,
        const std::vector<ItemPos>& last_bin_item_number_of_copies_2)
{
    // The copies are only tracked when the bin type has resources.
    if (last_bin_item_number_of_copies_1.empty()
            || last_bin_item_number_of_copies_2.empty()) {
        return true;
    }
    const std::vector<bool>& copy_dependent = copy_dependent_resource_consumptions[bin_type_id];
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)copy_dependent.size();
            ++item_type_id) {
        if (copy_dependent[item_type_id]
                && last_bin_item_number_of_copies_1[item_type_id]
                != last_bin_item_number_of_copies_2[item_type_id]) {
            return false;
        }
    }
    return true;
}

Profit packingsolver::negative_penalty_sum(const std::vector<Resource>& resources)
{
    Profit sum = 0.0;
    for (const Resource& resource: resources)
        if (resource.penalize && resource.penalty < 0.0)
            sum -= resource.penalty;
    return sum;
}

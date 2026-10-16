#pragma once

#include "packingsolver/algorithms/common.hpp"

#include "multiplechoicesubsetsumsolver/instance_builder.hpp"
#include "multiplechoicesubsetsumsolver/algorithms/dynamic_programming_bellman.hpp"

#include <algorithm>
#include <vector>

namespace packingsolver
{

/**
 * The lengths a group of identical item copies may take along an axis: one
 * value per orientation the copies may be packed with, and the number of
 * copies.
 */
struct SubsetSumBoundItem
{
    /** Possible lengths of each copy along the axis. */
    std::vector<Length> lengths;

    /** Number of copies. */
    ItemPos copies = 0;
};

/**
 * Upper bound on the largest total length, not exceeding 'capacity', of a
 * subset of the copies of 'items', each copy taken with one of its own
 * lengths.
 *
 * Computed with 'multiplechoicesubsetsumsolver', one group per copy. For
 * large capacities, the memory of the dynamic programming would be too
 * large: 'capacity' itself is returned instead, a weaker but still valid
 * bound.
 */
inline Length subset_sum_bound(
        const std::vector<SubsetSumBoundItem>& items,
        Length capacity)
{
    if (capacity <= 0)
        return 0;

    multiplechoicesubsetsumsolver::InstanceBuilder mcss_instance_builder;
    mcss_instance_builder.set_capacity(capacity);
    multiplechoicesubsetsumsolver::GroupId mcss_group_id = 0;
    Length length_sum = 0;
    for (const SubsetSumBoundItem& item: items) {
        if (item.copies <= 0)
            continue;
        std::vector<Length> lengths;
        for (Length length: item.lengths) {
            if (length <= 0 || length > capacity)
                continue;
            if (std::find(lengths.begin(), lengths.end(), length) == lengths.end())
                lengths.push_back(length);
        }
        if (lengths.empty())
            continue;
        Length length_min = *std::min_element(lengths.begin(), lengths.end());
        Length length_max = *std::max_element(lengths.begin(), lengths.end());
        // No subset within 'capacity' can use more copies than this.
        ItemPos copies = (std::min)(item.copies, (ItemPos)(capacity / length_min));
        for (ItemPos copy = 0; copy < copies; ++copy) {
            for (Length length: lengths)
                mcss_instance_builder.add_item(mcss_group_id, length);
            ++mcss_group_id;
        }
        length_sum += copies * length_max;
        if (length_sum >= capacity)
            length_sum = capacity;
    }

    // Every copy fits with its largest length.
    if (length_sum < capacity)
        return length_sum;
    if (capacity > 100000000)
        return capacity;

    multiplechoicesubsetsumsolver::Instance mcss_instance = mcss_instance_builder.build();
    multiplechoicesubsetsumsolver::Parameters mcss_parameters;
    mcss_parameters.verbosity_level = 0;
    auto mcss_output = multiplechoicesubsetsumsolver::dynamic_programming_bellman_word_ram(
            mcss_instance,
            mcss_parameters);
    return mcss_output.bound;
}

}

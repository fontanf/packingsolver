#include "packingsolver/rectangle/reduction.hpp"

#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/solution_builder.hpp"

#include "multiplechoicesubsetsumsolver/instance_builder.hpp"
#include "multiplechoicesubsetsumsolver/algorithms/dynamic_programming_bellman.hpp"

#include <algorithm>

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

/** Parameters for a companion-bin (or companion-strip) feasibility check. */
OptimizeParameters build_check_parameters(
        const ReductionParameters& parameters,
        Solution* fixed_items)
{
    OptimizeParameters check_parameters;
    check_parameters.verbosity_level = 0;
    check_parameters.timer = parameters.timer;
    check_parameters.optimization_mode = OptimizationMode::NotAnytimeDeterministic;
    check_parameters.use_tree_search = true;
    check_parameters.not_anytime_tree_search_queue_size = parameters.subproblem_queue_size;
    check_parameters.fixed_items = fixed_items;
    // Never reduce the check sub-instance itself: 'fixed_items' (when set)
    // references its item type ids directly, which a nested reduction
    // could renumber, and there is nothing to gain from reducing an
    // already-tiny companion-bin check anyway.
    check_parameters.reduction_parameters.reduce = false;
    return check_parameters;
}

}

bool Reduction::has_validated_companions(
        const std::vector<std::vector<CompanionItem>>& companions_by_copy)
{
    for (const std::vector<CompanionItem>& companions: companions_by_copy) {
        if (!companions.empty())
            return true;
    }
    return false;
}

std::vector<std::vector<Reduction::CompanionItem>> Reduction::extract_companions(
        ReductionItemType& item,
        ItemPos copies_to_consume)
{
    // Callers always index the result as '[0, copies_to_consume)', so a
    // never-enlarged item (empty 'companions_by_copy') still needs
    // 'copies_to_consume' (empty) placeholder entries here, not a
    // genuinely empty vector.
    if (item.companions_by_copy.empty())
        return std::vector<std::vector<CompanionItem>>(copies_to_consume);
    std::vector<std::vector<CompanionItem>> extracted(
            item.companions_by_copy.begin(),
            item.companions_by_copy.begin() + copies_to_consume);
    item.companions_by_copy.erase(
            item.companions_by_copy.begin(),
            item.companions_by_copy.begin() + copies_to_consume);
    return extracted;
}

bool Reduction::try_reduce_group(
        std::vector<ReductionItemType>& reduction_item_types,
        const ReductionParameters& parameters,
        EnlargementCase enlargement_case,
        Length bin_w,
        Length bin_h,
        const std::vector<ItemTypeId>& candidate_big_item_ids)
{
    // Big items whose companion bin has zero (or negative) usable size
    // already have their target dimension - no action needed at all, and
    // no feasibility check either. Only the remaining ("checked") big
    // items - genuine, positive-area companion bins - are eligible for
    // enlargement below, whether via a real companion or (if nothing at
    // all fits) the "empty group" case.
    std::vector<ItemTypeId> checked_big_item_ids;
    for (ItemTypeId big_item_type_id: candidate_big_item_ids) {
        Rectangle bin_dims = companion_bin_dimensions(enlargement_case, reduction_item_types[big_item_type_id], bin_w, bin_h);
        if (bin_dims.x > 0 && bin_dims.y > 0)
            checked_big_item_ids.push_back(big_item_type_id);
    }

    // Candidate companion items (R): not removed, not one of the
    // candidate big items themselves, and worth offering to at least one
    // checked big item (a necessary, not sufficient, pre-filter; see
    // 'could_fit'). An already-enlarged item type is *not* excluded: it
    // is a perfectly legitimate companion, geometrically - excluding it
    // would mean concluding "nothing could fit here" using a narrower
    // candidate pool than the original problem actually offers, which is
    // unsound (an item that only happens to already be spoken for
    // elsewhere in *this* reduction's own bookkeeping could still have
    // been the thing a true optimal solution shares this space with). If
    // absorbed here, its own real companions (if any) are captured
    // directly into
    // 'CompanionItem::nested_companions' in the "apply" step below,
    // rather than orphaned - see that field's own doc comment for why
    // this must happen at the moment of absorption, not via a later
    // lookup.
    std::vector<ItemTypeId> candidate_r_ids;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        if (std::find(
                    candidate_big_item_ids.begin(),
                    candidate_big_item_ids.end(),
                    item_type_id) != candidate_big_item_ids.end()) {
            continue;
        }
        bool fits_any = false;
        for (ItemTypeId big_item_type_id: checked_big_item_ids) {
            if (could_fit(enlargement_case, reduction_item_types[big_item_type_id], item_type_id, item, bin_w, bin_h)) {
                fits_any = true;
                break;
            }
        }
        if (fits_any)
            candidate_r_ids.push_back(item_type_id);
    }

    if (checked_big_item_ids.empty())
        return false;

    // For each checked big item, the resulting (per-copy) companion
    // assignments, filled in below either trivially (empty) or from the
    // sub-solve.
    std::vector<std::vector<std::vector<CompanionItem>>> companions_by_big_item(
            checked_big_item_ids.size());

    // 'true' iff every checked big item here was proven, via the
    // candidate scan above (not an actual solve), to have *nothing at
    // all* that could ever share its companion bin - the "empty group"
    // case of equation (8) ("enlarging the items"), Côté, Haouari & Iori
    // 2019/2021 Section 4.1. Every checked big item is then safe to
    // enlarge with zero companions unconditionally below, bypassing
    // 'has_validated_companions' (which otherwise exists precisely to
    // tell a genuine "nothing at all fits" apart from "this specific big
    // item just wasn't assigned anything in *this* group's particular
    // solve", the latter of which is a weaker claim - some other
    // candidate in this same group absorbed the shared R-items instead,
    // which says nothing about this big item's own strip in isolation,
    // so it must still be left untouched, not enlarged).
    bool trivially_feasible = false;

    if (!candidate_r_ids.empty()) {
        // Build the check sub-instance: one bin type (companion bin) per
        // checked big item, 'copies' instances each; items = candidate R
        // items.
        InstanceBuilder check_instance_builder;
        check_instance_builder.set_objective(Objective::Feasibility);
        check_instance_builder.set_parameters(original_instance_->parameters());

        std::vector<BinTypeId> check_bin_type_ids;
        for (ItemTypeId big_item_type_id: checked_big_item_ids) {
            const ReductionItemType& big_item = reduction_item_types[big_item_type_id];
            ItemPos big_item_copies = big_item.copies;
            Rectangle bin_dims = companion_bin_dimensions(enlargement_case, big_item, bin_w, bin_h);
            BinTypeId check_bin_type_id = check_instance_builder.add_bin_type(bin_dims.x, bin_dims.y);
            check_instance_builder.set_bin_type_copies(check_bin_type_id, big_item_copies);
            check_instance_builder.set_bin_type_copies_min(check_bin_type_id, 0);
            check_bin_type_ids.push_back(check_bin_type_id);
        }

        std::vector<ItemTypeId> check_r_item_type_ids;
        for (ItemTypeId r_item_type_id: candidate_r_ids) {
            const ReductionItemType& item = reduction_item_types[r_item_type_id];
            const ItemType& original_item_type = original_instance_->item_type(r_item_type_id);
            ItemTypeId check_item_type_id = check_instance_builder.add_item_type(
                    item.rect.x, item.rect.y, original_item_type.oriented);
            check_instance_builder.set_item_type_copies(check_item_type_id, item.copies);
            check_r_item_type_ids.push_back(check_item_type_id);
        }

        Instance check_instance = check_instance_builder.build();

        OptimizeParameters check_parameters = build_check_parameters(parameters, nullptr);
        Output check_output = optimize(check_instance, check_parameters);
        const Solution& check_solution = check_output.solution_pool.best();
        if (!check_solution.full())
            return false;

        // Interpret the check solution: for each distinct companion-bin
        // content pattern found, replicate it 'copies' times (identical
        // bin instances of the same type are merged into one 'SolutionBin'
        // with a 'copies' multiplier) into that big item's per-copy
        // companion list.
        for (BinPos bin_pos = 0;
                bin_pos < check_solution.number_of_different_bins();
                ++bin_pos) {
            const SolutionBin& solution_bin = check_solution.bin(bin_pos);
            auto it = std::find(
                    check_bin_type_ids.begin(),
                    check_bin_type_ids.end(),
                    solution_bin.bin_type_id);
            size_t big_item_pos = it - check_bin_type_ids.begin();
            const ReductionItemType& big_item = reduction_item_types[checked_big_item_ids[big_item_pos]];

            std::vector<CompanionItem> companions;
            for (const SolutionItem& solution_item: solution_bin.items) {
                auto r_it = std::find(
                        check_r_item_type_ids.begin(),
                        check_r_item_type_ids.end(),
                        solution_item.item_type_id);
                size_t r_pos = r_it - check_r_item_type_ids.begin();
                CompanionItem companion;
                companion.item_type_id = candidate_r_ids[r_pos];
                companion.offset = compute_offset(enlargement_case, big_item, solution_item.bl_corner);
                companion.rotate = solution_item.rotate;
                companions.push_back(companion);
            }
            for (BinPos copy = 0; copy < solution_bin.copies; ++copy)
                companions_by_big_item[big_item_pos].push_back(companions);
        }
    } else {
        // No candidate companions at all for any checked big item: the
        // "empty group" case - see 'trivially_feasible''s own doc comment
        // just above.
        trivially_feasible = true;
        for (size_t i = 0; i < checked_big_item_ids.size(); ++i)
            companions_by_big_item[i].assign(
                    reduction_item_types[checked_big_item_ids[i]].copies,
                    std::vector<CompanionItem>{});
    }

    // Apply the reduction: enlarge every candidate big item that actually
    // absorbed at least one companion, plus - if 'trivially_feasible' -
    // every checked big item outright (all of them equally proven
    // companionless by the same candidate scan). A candidate that ends up
    // with nothing to absorb *without* 'trivially_feasible' - some other
    // candidate in this same group's real solve claimed the shared
    // R-items instead - is left completely untouched, so it stays fully
    // eligible for a different sub-case (or a later round) to find real
    // companions for it; recording a no-op "enlargement" would otherwise
    // permanently (and pointlessly) exclude it from reconsideration under
    // this same axis (see 'gather_sorted_big_items').
    //
    // A candidate may already carry companions from an earlier axis (see
    // 'gather_sorted_big_items''s own doc comment): 'companions_by_copy' is
    // appended to, never overwritten, so those stay intact. Both vectors
    // always have exactly 'big_item.copies' entries (one per copy), so
    // appending index-for-index is well-defined regardless of which axis
    // contributed first.
    auto enlarge = [&](ItemTypeId big_item_type_id, std::vector<std::vector<CompanionItem>> companions) {
        ReductionItemType& big_item = reduction_item_types[big_item_type_id];
        switch (enlargement_case) {
        case EnlargementCase::Wide:
            big_item.rect.x = bin_w;
            break;
        case EnlargementCase::Tall:
            big_item.rect.y = bin_h;
            break;
        }
        if (big_item.companions_by_copy.empty()) {
            big_item.companions_by_copy = std::move(companions);
        } else {
            for (ItemPos copy = 0; copy < (ItemPos)big_item.companions_by_copy.size(); ++copy) {
                big_item.companions_by_copy[copy].insert(
                        big_item.companions_by_copy[copy].end(),
                        companions[copy].begin(), companions[copy].end());
            }
        }
    };

    // Capture each newly absorbed companion's own real companions (if any -
    // see 'CompanionItem::nested_companions') and mark it removed, before
    // 'companions_by_big_item[i]' is moved from below. A companion item
    // type can appear more than once here (several of its copies used,
    // possibly across several different big items in this same group
    // search), so its own prior companions are extracted once - all of
    // its copies at once, via 'extract_companions' - the first time it is
    // encountered, then handed out one entry per occurrence in the order
    // encountered (any consistent order works, since copies of the same
    // item type are interchangeable - see 'unreduce_solution''s own
    // comment on this); 'check_solution.full()' having verified every
    // offered candidate_r_id is fully placed guarantees every one of its
    // copies is accounted for by the time this finishes.
    std::vector<std::vector<std::vector<CompanionItem>>> companion_prior_companions(
            reduction_item_types.size());
    std::vector<ItemPos> companion_next_copy_index(reduction_item_types.size(), 0);

    bool any_enlarged = false;
    for (size_t i = 0; i < checked_big_item_ids.size(); ++i) {
        if (!trivially_feasible && !has_validated_companions(companions_by_big_item[i]))
            continue;
        for (std::vector<CompanionItem>& companions: companions_by_big_item[i]) {
            for (CompanionItem& companion: companions) {
                ReductionItemType& companion_item = reduction_item_types[companion.item_type_id];
                if (!companion_item.removed) {
                    companion_prior_companions[companion.item_type_id] =
                        extract_companions(companion_item, companion_item.copies);
                    companion_item.copies = 0;
                    companion_item.removed = true;
                }
                ItemPos copy_index = companion_next_copy_index[companion.item_type_id]++;
                companion.nested_companions =
                    companion_prior_companions[companion.item_type_id][copy_index];
            }
        }
        enlarge(checked_big_item_ids[i], std::move(companions_by_big_item[i]));
        any_enlarged = true;
    }

    return any_enlarged;
}

bool Reduction::is_big(
        EnlargementCase enlargement_case,
        ItemTypeId item_type_id,
        const ReductionItemType& item,
        Length bin_w,
        Length bin_h) const
{
    if (!original_instance_->item_type(item_type_id).oriented)
        return false;
    switch (enlargement_case) {
    case EnlargementCase::Wide:
        return 2 * item.rect.x > bin_w;
    case EnlargementCase::Tall:
        return 2 * item.rect.y > bin_h;
    }
    return false;
}

bool Reduction::size_greater(
        EnlargementCase enlargement_case,
        const ReductionItemType& item_1,
        const ReductionItemType& item_2) const
{
    switch (enlargement_case) {
    case EnlargementCase::Wide:
        if (item_1.rect.x != item_2.rect.x)
            return item_1.rect.x > item_2.rect.x;
        return item_1.rect.y > item_2.rect.y;
    case EnlargementCase::Tall:
        if (item_1.rect.y != item_2.rect.y)
            return item_1.rect.y > item_2.rect.y;
        return item_1.rect.x > item_2.rect.x;
    }
    return false;
}

bool Reduction::could_fit(
        EnlargementCase enlargement_case,
        const ReductionItemType& big_item,
        ItemTypeId item_type_id,
        const ReductionItemType& item,
        Length bin_w,
        Length bin_h) const
{
    // Width (or height) only, matching the paper's R = {j : wj <= w*}
    // exactly (not a full bounding-box check against the strip's other
    // dimension too): any item narrow enough to conceivably need a share
    // of this strip must end up in R, even if it turns out too tall for
    // this specific strip - otherwise it could be silently left out of
    // the packing check while the strip still gets declared fully
    // claimed, permanently hiding a combination that uses that leftover
    // space differently and making the reduction unsound (found by
    // testing against an exact-fit fixture: a narrow-but-tall item that
    // doesn't fit any available strip must make the check - and so the
    // whole reduction attempt - fail, not be silently ignored).
    bool oriented = original_instance_->item_type(item_type_id).oriented;
    switch (enlargement_case) {
    case EnlargementCase::Wide: {
        Length strip_w = bin_w - big_item.rect.x;
        return item.rect.x <= strip_w || (!oriented && item.rect.y <= strip_w);
    }
    case EnlargementCase::Tall: {
        Length strip_h = bin_h - big_item.rect.y;
        return item.rect.y <= strip_h || (!oriented && item.rect.x <= strip_h);
    }
    }
    return false;
}

bool Reduction::could_fit_both(
        ItemTypeId big_item_type_id,
        const ReductionItemType& big_item,
        ItemTypeId item_type_id,
        const ReductionItemType& item,
        Length bin_w,
        Length bin_h) const
{
    // The pairwise-sum condition alone isn't enough: satisfying it via
    // one axis (say the height sum) says nothing about whether the
    // item's *other* dimension (its width, in that example) even fits
    // inside the bin at all - it could independently exceed the bin's
    // own width (this does happen in practice: an item already enlarged
    // by a different sub-case can have one dimension equal to the full
    // bin size). Offering such an item to the companion-bin check would
    // hand it an item larger than the bin itself, which every downstream
    // consumer assumes never happens. So first require the item to fit
    // the bin on its own (in some allowed orientation), then apply the
    // paper's pairwise-sum test on top.
    bool oriented = original_instance_->item_type(item_type_id).oriented;
    bool fits_bin_oriented = item.rect.x <= bin_w && item.rect.y <= bin_h;
    bool fits_bin_rotated = !oriented && item.rect.y <= bin_w && item.rect.x <= bin_h;
    if (!fits_bin_oriented && !fits_bin_rotated)
        return false;

    // The big item may itself be non-oriented (see
    // 'gather_sorted_both_big_items''s own doc comment), which may end up
    // placed in either of its own valid orientations - not necessarily
    // its declared one (the real-solve
    // path in 'try_reduce_both_group' lets the actual solve pick; its
    // own 'trivially_feasible' path picks whichever one fits). This must
    // check the pairwise-sum condition against *every* orientation the
    // big item could actually use, not just its declared form: see this
    // function's own doc comment for why under-checking here would be
    // unsound, not merely less effective.
    bool big_item_oriented = original_instance_->item_type(big_item_type_id).oriented;
    for (int big_item_rotate = 0; big_item_rotate < (big_item_oriented? 1: 2); ++big_item_rotate) {
        Length big_item_w = (big_item_rotate == 0)? big_item.rect.x: big_item.rect.y;
        Length big_item_h = (big_item_rotate == 0)? big_item.rect.y: big_item.rect.x;
        if (big_item_w > bin_w || big_item_h > bin_h)
            continue;
        if (fits_bin_oriented
                && (item.rect.x + big_item_w <= bin_w || item.rect.y + big_item_h <= bin_h)) {
            return true;
        }
        if (fits_bin_rotated
                && (item.rect.y + big_item_w <= bin_w || item.rect.x + big_item_h <= bin_h)) {
            return true;
        }
    }
    return false;
}

Rectangle Reduction::companion_bin_dimensions(
        EnlargementCase enlargement_case,
        const ReductionItemType& big_item,
        Length bin_w,
        Length bin_h) const
{
    switch (enlargement_case) {
    case EnlargementCase::Wide:
        return Rectangle{bin_w - big_item.rect.x, big_item.rect.y};
    case EnlargementCase::Tall:
        return Rectangle{big_item.rect.x, bin_h - big_item.rect.y};
    }
    return Rectangle{0, 0};
}

Point Reduction::compute_offset(
        EnlargementCase enlargement_case,
        const ReductionItemType& big_item,
        Point bl_corner_in_check) const
{
    switch (enlargement_case) {
    case EnlargementCase::Wide:
        return Point{big_item.rect.x + bl_corner_in_check.x, bl_corner_in_check.y};
    case EnlargementCase::Tall:
        return Point{bl_corner_in_check.x, big_item.rect.y + bl_corner_in_check.y};
    }
    return bl_corner_in_check;
}

std::vector<ItemTypeId> Reduction::gather_sorted_big_items(
        const std::vector<ReductionItemType>& reduction_item_types,
        EnlargementCase enlargement_case,
        Length bin_w,
        Length bin_h)
{
    std::vector<ItemTypeId> candidates;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        if (is_big(enlargement_case, item_type_id, item, bin_w, bin_h))
            candidates.push_back(item_type_id);
    }
    std::sort(
            candidates.begin(),
            candidates.end(),
            [&](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2)
            {
                return size_greater(
                        enlargement_case,
                        reduction_item_types[item_type_id_1],
                        reduction_item_types[item_type_id_2]);
            });
    return candidates;
}

std::vector<ItemTypeId> Reduction::gather_sorted_both_big_items(
        const std::vector<ReductionItemType>& reduction_item_types,
        Length bin_w,
        Length bin_h)
{
    std::vector<ItemTypeId> candidates;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        // A non-oriented item is safe to admit here, unlike 'is_big''s
        // own 'Wide'/'Tall' cases: 'try_reduce_both_group' never
        // pre-places or enlarges the big item in place - it captures and
        // replays whatever position/rotation the companion-bin check
        // itself found (see 'BothGroup'), so there is no shared,
        // per-type state that would need a single orientation to stay
        // consistent across every copy.
        bool oriented = original_instance_->item_type(item_type_id).oriented;
        bool both_big = (2 * item.rect.x > bin_w && 2 * item.rect.y > bin_h)
            || (!oriented && 2 * item.rect.y > bin_w && 2 * item.rect.x > bin_h);
        if (both_big)
            candidates.push_back(item_type_id);
    }
    std::sort(
            candidates.begin(),
            candidates.end(),
            [&](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2)
            {
                return reduction_item_types[item_type_id_1].rect.area()
                    > reduction_item_types[item_type_id_2].rect.area();
            });
    return candidates;
}

bool Reduction::reduce_group(
        std::vector<ReductionItemType>& reduction_item_types,
        const ReductionParameters& parameters,
        EnlargementCase enlargement_case,
        Length bin_w,
        Length bin_h)
{
    bool found_any = false;

    // Loop A: singletons.
    for (ItemTypeId big_item_type_id: gather_sorted_big_items(reduction_item_types, enlargement_case, bin_w, bin_h)) {
        if (reduction_item_types[big_item_type_id].removed) {
            // May have been consumed as a companion of an earlier singleton
            // reduction in this same pass.
            continue;
        }
        if (parameters.timer.needs_to_end())
            return found_any;
        if (try_reduce_group(reduction_item_types, parameters, enlargement_case, bin_w, bin_h, {big_item_type_id}))
            found_any = true;
    }

    // Loop B: growing groups, following the paper's search - starting from
    // the two largest remaining candidates, growing by one (the next
    // largest) on failure, restarting from the (new) two largest remaining
    // candidates after every success.
    for (;;) {
        std::vector<ItemTypeId> candidates = gather_sorted_big_items(reduction_item_types, enlargement_case, bin_w, bin_h);
        if (candidates.size() < 2)
            break;
        bool applied = false;
        for (size_t group_size = 2; group_size <= candidates.size(); ++group_size) {
            if (parameters.timer.needs_to_end())
                return found_any;
            std::vector<ItemTypeId> group(candidates.begin(), candidates.begin() + group_size);
            if (try_reduce_group(reduction_item_types, parameters, enlargement_case, bin_w, bin_h, group)) {
                found_any = true;
                applied = true;
                break;
            }
        }
        if (!applied)
            break;
    }

    return found_any;
}

bool Reduction::try_reduce_both_group(
        std::vector<ReductionItemType>& reduction_item_types,
        const ReductionParameters& parameters,
        Length bin_w,
        Length bin_h,
        const std::vector<ItemTypeId>& candidate_big_item_ids)
{
    // Unlike 'try_reduce_group''s wide/tall strips, "both"'s companion bin
    // is always the full 'bin_w'x'bin_h' bin (see
    // 'companion_bin_dimensions') - never degenerate, so every candidate
    // here is genuinely checked; there is no trivial/zero-area shortcut.

    // Candidate companion items (R): same as 'try_reduce_group' - see
    // there for why an already-enlarged item type is *not* excluded (its
    // own real companions, if absorbed here, are captured directly below
    // via 'extract_companions', same as for the big item itself).
    std::vector<ItemTypeId> candidate_r_ids;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        if (std::find(
                    candidate_big_item_ids.begin(),
                    candidate_big_item_ids.end(),
                    item_type_id) != candidate_big_item_ids.end()) {
            continue;
        }
        bool fits_any = false;
        for (ItemTypeId big_item_type_id: candidate_big_item_ids) {
            if (could_fit_both(big_item_type_id, reduction_item_types[big_item_type_id], item_type_id, item, bin_w, bin_h)) {
                fits_any = true;
                break;
            }
        }
        if (fits_any)
            candidate_r_ids.push_back(item_type_id);
    }
    if (candidate_r_ids.empty()) {
        // The "empty group" case of equation (8) ("enlarging the items"):
        // genuinely nothing (from the same unified, 'removed'-only-
        // excluded candidate pool 'could_fit' was just scanned over)
        // could ever share any of these big items' companion bins - no
        // real solve needed to know this, unlike the non-empty case
        // below. Every candidate here is independently, unconditionally
        // eligible for a direct single-item capture (no group solve ever
        // combines several big items into one dedicated bin anyway - see
        // 'BothGroup''s own doc comment), subject to the same bin type
        // copies cap as the non-empty case, and to determining which
        // orientation actually fits (a non-oriented item's declared form
        // need not be the one that does - see 'is_big''s own doc comment
        // for why 'Both' admits that in the first place).
        const BinType& bin_type = original_instance_->bin_type(0);
        bool any_captured = false;
        for (ItemTypeId big_item_type_id: candidate_big_item_ids) {
            ReductionItemType& big_item = reduction_item_types[big_item_type_id];
            bool oriented = original_instance_->item_type(big_item_type_id).oriented;
            bool rotate;
            if (big_item.rect.x <= bin_w && big_item.rect.y <= bin_h) {
                rotate = false;
            } else if (!oriented && big_item.rect.y <= bin_w && big_item.rect.x <= bin_h) {
                rotate = true;
            } else {
                // Neither orientation actually fits the companion bin at
                // all: 'is_big''s own 'Both' condition does not by itself
                // guarantee this (see its own doc comment), so skip
                // defensively rather than ever building a geometrically
                // invalid dedicated bin.
                continue;
            }
            if (bin_type.copies >= 0
                    && reserved_bin_count() + big_item.copies > bin_type.copies) {
                // Not enough bin copies left to reserve dedicated bins
                // for this item type: leave it for the underlying solver
                // instead (sound, just less effective - see
                // 'reduce_perfect_pairs''s identical guard for why).
                continue;
            }
            std::vector<std::vector<CompanionItem>> prior_companions =
                extract_companions(big_item, big_item.copies);
            ItemPos copies = big_item.copies;
            big_item.copies = 0;
            big_item.removed = true;
            for (ItemPos copy = 0; copy < copies; ++copy) {
                both_groups_.push_back(BothGroup{
                        {BothGroup::PlacedItem{big_item_type_id, Point{0, 0}, rotate, prior_companions[copy]}}});
            }
            any_captured = true;
        }
        return any_captured;
    }

    // Build the check sub-instance: a single, shared companion bin type
    // ('bin_w'x'bin_h', 'copies' = the sum of every candidate's own) -
    // items = the big items themselves - each offered as an ordinary
    // item, with its own true 'oriented' flag, free to be placed (and
    // rotated, if non-oriented) however the check likes, since its own
    // position and rotation are captured and replayed directly below,
    // with no shared, per-type state (unlike 'ReductionItemType::rect')
    // that would need to stay consistent across different copies - plus
    // the candidate R items.
    //
    // No eligibility restriction is needed to keep different candidates'
    // big items apart, unlike an earlier version of this function: two
    // "both"-big items (each spanning more than half the companion bin on
    // *both* axes, by definition - see 'gather_sorted_both_big_items')
    // can never fit in the same 'bin_w'x'bin_h' bin together, regardless
    // of where either is placed - any valid non-overlapping placement of
    // two rectangles
    // needs their widths to sum to at most 'bin_w' *or* their heights to
    // sum to at most 'bin_h', and both are already individually violated.
    // So a bin instance can hold at most one candidate's big item by
    // geometry alone, and since every big item is itself mandatory (all
    // 'copies' of it must be placed, exactly like the R-candidates), the
    // interpretation loop below can just read off which big item (if any)
    // a solved bin actually holds directly, rather than needing the check
    // itself to keep candidates artificially apart.
    InstanceBuilder check_instance_builder;
    check_instance_builder.set_objective(Objective::Feasibility);
    check_instance_builder.set_parameters(original_instance_->parameters());

    ItemPos total_big_item_copies = 0;
    for (ItemTypeId big_item_type_id: candidate_big_item_ids)
        total_big_item_copies += reduction_item_types[big_item_type_id].copies;
    BinTypeId check_bin_type_id = check_instance_builder.add_bin_type(bin_w, bin_h);
    check_instance_builder.set_bin_type_copies(check_bin_type_id, total_big_item_copies);
    check_instance_builder.set_bin_type_copies_min(check_bin_type_id, 0);

    std::vector<ItemTypeId> check_big_item_type_ids;
    for (ItemTypeId big_item_type_id: candidate_big_item_ids) {
        const ReductionItemType& big_item = reduction_item_types[big_item_type_id];
        ItemTypeId check_big_item_type_id = check_instance_builder.add_item_type(
                big_item.rect.x, big_item.rect.y,
                original_instance_->item_type(big_item_type_id).oriented);
        check_instance_builder.set_item_type_copies(check_big_item_type_id, big_item.copies);
        check_big_item_type_ids.push_back(check_big_item_type_id);
    }

    std::vector<ItemTypeId> check_r_item_type_ids;
    for (ItemTypeId r_item_type_id: candidate_r_ids) {
        const ReductionItemType& item = reduction_item_types[r_item_type_id];
        const ItemType& original_item_type = original_instance_->item_type(r_item_type_id);
        ItemTypeId check_item_type_id = check_instance_builder.add_item_type(
                item.rect.x, item.rect.y, original_item_type.oriented);
        check_instance_builder.set_item_type_copies(check_item_type_id, item.copies);
        check_r_item_type_ids.push_back(check_item_type_id);
    }

    Instance check_instance = check_instance_builder.build();
    OptimizeParameters check_parameters = build_check_parameters(parameters, nullptr);
    Output check_output = optimize(check_instance, check_parameters);
    const Solution& check_solution = check_output.solution_pool.best();
    if (!check_solution.full())
        return false;

    // Interpret the check's own solution directly: every solved companion
    // bin (one distinct pattern per 'SolutionBin', replicated 'copies'
    // times) becomes its own dedicated bin, holding whichever big item
    // and R-candidates the check placed into it, each at its own
    // check-found position and rotation - no relative-offset computation
    // needed, since the companion bin's dimensions ('bin_w'x'bin_h') are
    // exactly the dedicated bin's own.
    //
    // Grouped by which candidate's big item type is actually found in
    // each solved bin (there is at most one - see the check
    // sub-instance's own doc comment above for why). A bin with no big
    // item at all is not expected to happen (every big item is
    // mandatory, and there are exactly as many bin instances offered as
    // there are big item copies in total, so by pigeonhole every bin
    // instance actually used must hold exactly one once the check
    // succeeds), but is skipped defensively rather than assumed
    // impossible; an item type id belonging to neither a candidate big
    // item nor an R-candidate is treated the same way, declining the
    // whole check rather than risk mis-indexing 'candidate_r_ids' (sound,
    // just less effective - nothing has been mutated in
    // 'reduction_item_types' yet at this point).
    std::vector<std::vector<std::vector<BothGroup::PlacedItem>>> groups_by_big_item(
            candidate_big_item_ids.size());
    for (BinPos bin_pos = 0;
            bin_pos < check_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = check_solution.bin(bin_pos);

        size_t big_item_pos = candidate_big_item_ids.size();
        for (const SolutionItem& solution_item: solution_bin.items) {
            auto it = std::find(
                    check_big_item_type_ids.begin(),
                    check_big_item_type_ids.end(),
                    solution_item.item_type_id);
            if (it != check_big_item_type_ids.end()) {
                big_item_pos = it - check_big_item_type_ids.begin();
                break;
            }
        }
        if (big_item_pos == candidate_big_item_ids.size())
            continue;

        std::vector<BothGroup::PlacedItem> placed_items;
        for (const SolutionItem& solution_item: solution_bin.items) {
            ItemTypeId original_item_type_id;
            if (solution_item.item_type_id == check_big_item_type_ids[big_item_pos]) {
                original_item_type_id = candidate_big_item_ids[big_item_pos];
            } else {
                auto r_it = std::find(
                        check_r_item_type_ids.begin(),
                        check_r_item_type_ids.end(),
                        solution_item.item_type_id);
                if (r_it == check_r_item_type_ids.end())
                    return false;
                size_t r_pos = r_it - check_r_item_type_ids.begin();
                original_item_type_id = candidate_r_ids[r_pos];
            }
            placed_items.push_back(BothGroup::PlacedItem{
                    original_item_type_id, solution_item.bl_corner, solution_item.rotate, {}});
        }
        for (BinPos copy = 0; copy < solution_bin.copies; ++copy)
            groups_by_big_item[big_item_pos].push_back(placed_items);
    }

    // All-or-nothing bin type copies cap check (see this function's own
    // doc comment for why a partial commit isn't straightforward here,
    // unlike 'reduce_full_bin_items'/'reduce_perfect_pairs''s own
    // per-item skip).
    const BinType& bin_type = original_instance_->bin_type(0);
    BinPos total_copies_used = 0;
    for (const auto& groups: groups_by_big_item)
        total_copies_used += (BinPos)groups.size();
    if (total_copies_used == 0)
        return false;
    if (bin_type.copies >= 0
            && reserved_bin_count() + total_copies_used > bin_type.copies) {
        // Not enough bin copies left to reserve dedicated bins for this
        // batch: leave every item type here untouched instead (sound,
        // just less effective - see 'reduce_perfect_pairs''s identical
        // guard for why).
        return false;
    }

    // Apply: every R-candidate offered above is mandatory in the check
    // (unlike the big items' own bin copies, which have 'copies_min' 0),
    // so 'check_solution.full()' already guarantees every one of them was
    // placed somewhere across the groups being applied here - captured
    // and marked removed up front, then distributed one copy at a time,
    // in encounter order, as each of its placements is found below (any
    // consistent order works, since copies of the same item type are
    // interchangeable - see 'unreduce_solution''s own comment on this).
    std::vector<std::vector<std::vector<CompanionItem>>> r_prior_companions(reduction_item_types.size());
    std::vector<ItemPos> r_next_copy_index(reduction_item_types.size(), 0);
    for (ItemTypeId r_item_type_id: candidate_r_ids) {
        ReductionItemType& r_item = reduction_item_types[r_item_type_id];
        r_prior_companions[r_item_type_id] = extract_companions(r_item, r_item.copies);
        r_item.copies = 0;
        r_item.removed = true;
    }

    for (size_t candidate_pos = 0; candidate_pos < candidate_big_item_ids.size(); ++candidate_pos) {
        if (groups_by_big_item[candidate_pos].empty())
            continue;
        ItemTypeId big_item_type_id = candidate_big_item_ids[candidate_pos];
        ReductionItemType& big_item = reduction_item_types[big_item_type_id];
        ItemPos copies_used = (ItemPos)groups_by_big_item[candidate_pos].size();

        std::vector<std::vector<CompanionItem>> big_item_prior_companions =
            extract_companions(big_item, copies_used);
        big_item.copies -= copies_used;
        if (big_item.copies == 0)
            big_item.removed = true;

        for (ItemPos copy = 0; copy < copies_used; ++copy) {
            for (BothGroup::PlacedItem& placed_item: groups_by_big_item[candidate_pos][copy]) {
                if (placed_item.item_type_id == big_item_type_id) {
                    placed_item.companions = big_item_prior_companions[copy];
                } else {
                    ItemPos r_copy_index = r_next_copy_index[placed_item.item_type_id]++;
                    placed_item.companions = r_prior_companions[placed_item.item_type_id][r_copy_index];
                }
            }
            both_groups_.push_back(BothGroup{std::move(groups_by_big_item[candidate_pos][copy])});
        }
    }

    return true;
}

bool Reduction::reduce_both_groups(
        std::vector<ReductionItemType>& reduction_item_types,
        const ReductionParameters& parameters,
        Length bin_w,
        Length bin_h)
{
    bool found_any = false;

    // Loop A: singletons.
    for (ItemTypeId big_item_type_id: gather_sorted_both_big_items(reduction_item_types, bin_w, bin_h)) {
        if (reduction_item_types[big_item_type_id].removed) {
            // May have been consumed as a companion of an earlier singleton
            // reduction in this same pass.
            continue;
        }
        if (parameters.timer.needs_to_end())
            return found_any;
        if (try_reduce_both_group(reduction_item_types, parameters, bin_w, bin_h, {big_item_type_id}))
            found_any = true;
    }

    // Loop B: growing groups (same search strategy as 'reduce_group').
    for (;;) {
        std::vector<ItemTypeId> candidates = gather_sorted_both_big_items(reduction_item_types, bin_w, bin_h);
        if (candidates.size() < 2)
            break;
        bool applied = false;
        for (size_t group_size = 2; group_size <= candidates.size(); ++group_size) {
            if (parameters.timer.needs_to_end())
                return found_any;
            std::vector<ItemTypeId> group(candidates.begin(), candidates.begin() + group_size);
            if (try_reduce_both_group(reduction_item_types, parameters, bin_w, bin_h, group)) {
                found_any = true;
                applied = true;
                break;
            }
        }
        if (!applied)
            break;
    }

    return found_any;
}

bool Reduction::reduce_full_bin_items(
        std::vector<ReductionItemType>& reduction_item_types,
        Length bin_w,
        Length bin_h)
{
    const BinType& bin_type = original_instance_->bin_type(0);

    bool any_reduced = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;

        // Exact match against 'bin_w'x'bin_h' (the *shrunk* bin - see
        // this function's own doc comment), using the item's *current*
        // dimensions (may already be enlarged, companionlessly or with
        // real companions of its own).
        bool oriented = original_instance_->item_type(item_type_id).oriented;
        bool rotate = false;
        bool matches = false;
        if (item.rect.x == bin_w && item.rect.y == bin_h) {
            matches = true;
            rotate = false;
        } else if (!oriented && item.rect.y == bin_w && item.rect.x == bin_h) {
            matches = true;
            rotate = true;
        }
        if (!matches)
            continue;

        ItemPos copies = item.copies;
        if (bin_type.copies >= 0
                && reserved_bin_count() + copies > bin_type.copies) {
            // Not enough bin copies left to reserve dedicated bins for
            // this item type: leave it for the underlying solver instead
            // (sound, just less effective - see 'reduce_perfect_pairs''s
            // identical guard for why).
            continue;
        }

        // Captures this item type's own companions (if 'enlarged' with
        // any - see this function's own doc comment for why they are no
        // longer excluded), instead of leaving them behind to be
        // silently dropped once 'item' is marked 'removed' below.
        std::vector<std::vector<CompanionItem>> companions_by_copy =
            extract_companions(item, copies);
        item.removed = true;
        full_bin_items_.push_back(FullBinItem{
                item_type_id, rotate, copies, std::move(companions_by_copy)});
        any_reduced = true;
    }

    return any_reduced;
}

bool Reduction::reduce_full_span_items(
        std::vector<ReductionItemType>& reduction_item_types)
{
    // If some other dedicated-bin reservation (found earlier this same
    // round - 'reduce_full_bin_items'/'reduce_perfect_pairs' both run
    // before this function) already claimed the bin's sole copy (see
    // 'reduce_full_span_items_applies' - exactly one bin copy is the only
    // case this function ever runs in), there is no bin left for this
    // function's own pins to share - nothing to do. Deliberately checks
    // 'number_of_dedicated_bins()' here, not 'reserved_bin_count()': the
    // latter would already count this function's own prior pins from an
    // earlier round, which must never block it from continuing to pin
    // more into the very bin it already claimed for itself.
    const BinType& bin_type = original_instance_->bin_type(0);
    if (bin_type.copies >= 0 && number_of_dedicated_bins() >= bin_type.copies)
        return false;

    bool any_reduced = false;

    // Repeat within this call too, not just across the constructor's own
    // outer rounds: shrinking 'true_bin_rect_' on one axis can newly make
    // some other item type's *other* axis match as well, within the very
    // same call - see this function's own doc comment in 'reduction.hpp'.
    bool found_this_pass = true;
    while (found_this_pass) {
        found_this_pass = false;

        for (ItemTypeId item_type_id = 0;
                item_type_id < (ItemTypeId)reduction_item_types.size();
                ++item_type_id) {
            ReductionItemType& item = reduction_item_types[item_type_id];
            bool oriented = original_instance_->item_type(item_type_id).oriented;

            // Matches against 'item.rect' - this item type's *current*
            // dimensions, possibly already grown by lifting or companion
            // absorption earlier this same round - deliberately, not its
            // original ones. Both grown forms remain sound to match and
            // consume from 'true_bin_rect_' by their *full* current
            // dimension, for the same underlying reason in each case:
            // whatever the growth added beyond the item's own true
            // footprint is margin already proven permanently unusable by
            // anything else remaining (lifting's own subset-sum argument -
            // see 'lift_item_dimensions_axis' - or a companion-bin
            // feasibility check's own real, placed companions - see
            // 'reduce_group'/'reduce_both_groups'), so treating the whole
            // grown span as spoken for changes nothing about what could
            // ever still fit elsewhere; it was never available to begin
            // with. A companion-enlarged item's own real companions are
            // carried along (via 'extract_companions' below) and placed
            // relative to wherever this pin ends up, exactly as
            // 'place_item_and_companions' already does for every other
            // case in this class - their own relative offsets do not
            // depend on this item's absolute position.
            //
            // Consume every copy of this item type that currently matches,
            // one at a time (each consumption changes 'true_bin_rect_',
            // which - since this item type's own dimensions never change
            // here - can only ever affect whether its *next* copy still
            // matches, never retroactively invalidate this one).
            while (!item.removed && item.copies > 0) {
                bool matched = false;
                bool rotate = false;
                Length matched_width = 0;

                // Height match: spans the bin's current true height, so it
                // can be pinned at the current origin and the bin shrunk
                // on the width axis by its own width.
                if (item.rect.y == true_bin_rect_.y) {
                    matched = true;
                    rotate = false;
                    matched_width = item.rect.x;
                } else if (!oriented && item.rect.x == true_bin_rect_.y) {
                    matched = true;
                    rotate = true;
                    matched_width = item.rect.y;
                }

                Point bl_corner = true_bin_origin_;
                if (matched) {
                    true_bin_origin_.x += matched_width;
                    true_bin_rect_.x -= matched_width;
                } else {
                    // Width match: spans the bin's current true width, so
                    // it can be pinned at the current origin and the bin
                    // shrunk on the height axis by its own height.
                    Length matched_height = 0;
                    if (item.rect.x == true_bin_rect_.x) {
                        matched = true;
                        rotate = false;
                        matched_height = item.rect.y;
                    } else if (!oriented && item.rect.y == true_bin_rect_.x) {
                        matched = true;
                        rotate = true;
                        matched_height = item.rect.x;
                    }
                    if (!matched)
                        break;
                    true_bin_origin_.y += matched_height;
                    true_bin_rect_.y -= matched_height;
                }

                std::vector<std::vector<CompanionItem>> companions_by_copy
                    = extract_companions(item, 1);
                item.copies--;
                if (item.copies == 0)
                    item.removed = true;
                full_span_items_.push_back(FullSpanItem{
                        item_type_id, bl_corner, rotate, std::move(companions_by_copy[0])});
                any_reduced = true;
                found_this_pass = true;

                // A strictly negative 'true_bin_rect_' means the item just
                // placed did not actually fit in whatever room was left
                // before it - infeasible outright, regardless of what else
                // remains (there is nothing "exact" about overflowing).
                // Exactly 0 is different: a perfectly good, exact-fit
                // placement, *unless* something else still needs positive
                // room that no longer exists - Proposition 1's "if there is
                // a solution" premise is only actually violated once a
                // positive-area item remains with no positive room left for
                // it. Only possible for 'Feasibility', matching every other
                // place this class ever sets 'proven_infeasible_' (see
                // 'reduction_to_instance').
                bool overflowed = (true_bin_rect_.x < 0 || true_bin_rect_.y < 0);
                bool exhausted = (true_bin_rect_.x == 0 || true_bin_rect_.y == 0);
                if (overflowed || exhausted) {
                    bool anything_remaining = false;
                    for (const ReductionItemType& other_item: reduction_item_types) {
                        if (!other_item.removed && other_item.copies > 0) {
                            anything_remaining = true;
                            break;
                        }
                    }
                    if (overflowed || anything_remaining) {
                        // Stop immediately: continuing to pin further items
                        // against a non-positive bin is meaningless, and
                        // 'reduction_to_instance' must never be asked to
                        // build a bin type with a non-positive dimension.
                        proven_infeasible_ = true;
                        return any_reduced;
                    }
                }
            }
        }
    }

    return any_reduced;
}

bool Reduction::reduce_perfect_pairs(
        std::vector<ReductionItemType>& reduction_item_types,
        Length bin_w,
        Length bin_h)
{
    const BinType& bin_type = original_instance_->bin_type(0);

    bool any_reduced = false;
    for (ItemTypeId item_type_id_1 = 0;
            item_type_id_1 < (ItemTypeId)reduction_item_types.size();
            ++item_type_id_1) {
        ReductionItemType& item_1 = reduction_item_types[item_type_id_1];
        if (item_1.removed)
            continue;
        // Both sides of a pair must be 'oriented' - see this function's
        // own doc comment for why rotation is unsound here specifically,
        // unlike 'reduce_full_bin_items': matching only *one* bin
        // dimension (not both simultaneously) never forces a non-oriented
        // item's *other* orientation to become geometrically infeasible,
        // so nothing guarantees it couldn't have been placed differently
        // in a true optimal solution - the "slide together" argument
        // below needs that guarantee unconditionally.
        if (!original_instance_->item_type(item_type_id_1).oriented)
            continue;
        const Rectangle& footprint_1 = item_1.rect;

        for (ItemTypeId item_type_id_2 = 0;
                item_type_id_2 < (ItemTypeId)reduction_item_types.size();
                ++item_type_id_2) {
            if (item_type_id_2 == item_type_id_1)
                continue;
            ReductionItemType& item_2 = reduction_item_types[item_type_id_2];
            if (item_2.removed)
                continue;
            if (!original_instance_->item_type(item_type_id_2).oriented)
                continue;
            ItemPos copies = std::min(item_1.copies, item_2.copies);
            if (bin_type.copies >= 0
                    && reserved_bin_count() + copies > bin_type.copies) {
                // Not enough bin copies left to reserve dedicated bins
                // for this pair: leave both item types for the
                // underlying solver instead (sound, just less
                // effective - this keeps the subtraction in
                // 'reduction_to_instance' from ever going negative).
                continue;
            }

            const Rectangle& footprint_2 = item_2.rect;
            bool vertical_split = footprint_1.y == bin_h && footprint_2.y == bin_h
                    && footprint_1.x + footprint_2.x == bin_w;
            bool horizontal_split = !vertical_split
                    && footprint_1.x == bin_w && footprint_2.x == bin_w
                    && footprint_1.y + footprint_2.y == bin_h;
            if (!vertical_split && !horizontal_split)
                continue;

            // Consume 'copies' (the smaller of the two) from each side:
            // fully depleted item types are removed entirely, but a type
            // with more copies than its partner keeps its leftover
            // copies as an ordinary item type in the reduced instance
            // (see this function's own doc comment for the "j1: 2
            // copies, j2: 4 copies" example this generalizes from the
            // old equal-copies-only rule). Either side may already be
            // 'enlarged' with real companions of its own (see this
            // function's own doc comment for why that is no longer
            // excluded) - captured here instead of being silently
            // dropped once 'copies' worth of it is consumed.
            std::vector<std::vector<CompanionItem>> item_1_companions_by_copy =
                extract_companions(item_1, copies);
            item_1.copies -= copies;
            if (item_1.copies == 0)
                item_1.removed = true;
            std::vector<std::vector<CompanionItem>> item_2_companions_by_copy =
                extract_companions(item_2, copies);
            item_2.copies -= copies;
            if (item_2.copies == 0)
                item_2.removed = true;

            Point offset_2 = (vertical_split)?
                Point{footprint_1.x, 0}:
                Point{0, footprint_1.y};
            perfect_pairs_.push_back(PerfectPair{
                    item_type_id_1, item_type_id_2, offset_2, copies,
                    std::move(item_1_companions_by_copy),
                    std::move(item_2_companions_by_copy)});
            any_reduced = true;
            break;
        }
    }

    return any_reduced;
}

BinPos Reduction::number_of_dedicated_bins() const
{
    BinPos total = 0;
    for (const FullBinItem& full_bin_item: full_bin_items_)
        total += full_bin_item.copies;
    for (const PerfectPair& pair: perfect_pairs_)
        total += pair.copies;
    total += (BinPos)both_groups_.size();
    return total;
}

BinPos Reduction::reserved_bin_count() const
{
    return number_of_dedicated_bins() + (full_span_items_.empty()? 0: 1);
}

bool Reduction::items_mergeable(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2) const
{
    const ReductionItemType& item_1 = reduction_item_types[item_type_id_1];
    const ReductionItemType& item_2 = reduction_item_types[item_type_id_2];
    if (item_1.rect.x != item_2.rect.x || item_1.rect.y != item_2.rect.y)
        return false;

    const ItemType& original_item_type_1 = original_instance_->item_type(item_type_id_1);
    const ItemType& original_item_type_2 = original_instance_->item_type(item_type_id_2);
    // Profit is only compared for 'Knapsack': for every other objective
    // this class handles, profit is never the actual objective, and
    // 'unreduce_solution' always restores the true original item type id
    // (and so its true profit) for every placed copy regardless of which
    // one a reduced-instance solve actually used, so a profit mismatch
    // cannot affect correctness there - only, at most, the search guide's
    // scoring while solving the reduced instance. This matters in
    // practice: companion absorption enlarges a "wide"/"tall" item's
    // 'rect' without touching its profit, so two originally different item
    // types absorbed into the same footprint end up identical on every
    // other property here but not on profit.
    //
    // For 'Knapsack', profit *is* the actual objective, and copies are
    // optional (a solve may legitimately leave some unplaced) - merging
    // two different-profit item types would report a single, uniform
    // profit for every copy in the reduced instance (see
    // 'reduction_to_instance', which always takes the survivor's own
    // profit), so the solve itself would optimize against the wrong
    // values and could pick a suboptimal subset of copies to place; no
    // amount of restoring true profits afterwards, in 'unreduce_solution',
    // can undo a choice already made on wrong information. So a profit
    // mismatch there must block the merge outright, not just risk a worse
    // search guide.
    if (original_instance_->objective() == Objective::Knapsack
            && original_item_type_1.profit != original_item_type_2.profit)
        return false;
    if (original_item_type_1.oriented != original_item_type_2.oriented
            || original_item_type_1.weight != original_item_type_2.weight
            || original_item_type_1.group_id != original_item_type_2.group_id
            || original_item_type_1.eligibility_id != original_item_type_2.eligibility_id)
        return false;

    static const std::vector<double> empty_schedule;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = original_instance_->bin_type(bin_type_id);
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const std::vector<std::vector<double>>& item_consumptions
                = bin_type.resource(resource_id).item_consumptions;
            const std::vector<double>& schedule_1
                = (item_type_id_1 < (ItemTypeId)item_consumptions.size())?
                    item_consumptions[item_type_id_1]: empty_schedule;
            const std::vector<double>& schedule_2
                = (item_type_id_2 < (ItemTypeId)item_consumptions.size())?
                    item_consumptions[item_type_id_2]: empty_schedule;
            if (schedule_1 != schedule_2)
                return false;
        }
    }

    // Each candidate's own current copies must be either entirely
    // mandatory or entirely optional, and both candidates must agree on
    // which - see this method's own doc comment in 'reduction.hpp' for
    // the general argument. Concrete counterexample for why a mix is
    // unsound: item type A, 3 copies, 'copies_min' 3 (fully mandatory -
    // only possible for 'Knapsack'), merged with identical-footprint item
    // type B, 5 copies, 'copies_min' 0 (fully optional). The merged
    // survivor would get 8 copies with a combined 'copies_min' of 3 (see
    // 'reduction_to_instance'), which only forces *some* 3 of the 8
    // copies to be placed - nothing ties those specific 3 back to A's own
    // copies, since 'reduction_to_instance''s per-copy origin list (see
    // 'CopyOrigin') assigns origins in a fixed, arbitrary order (survivor
    // first, then each merged-in type), unrelated to which copies a
    // downstream 'Knapsack' solve actually chooses to place. A solution
    // that places 3 copies drawn entirely from B's share (fully
    // satisfying the reduced instance's own 'copies_min' of 3) would
    // 'unreduce_solution' into leaving all 3 of A's genuinely mandatory
    // copies unplaced - violating A's true requirement, even though the
    // reduced-instance solve was itself entirely valid. Both-mandatory or
    // both-optional merges do not have this problem: a combined
    // 'copies_min' equal to the full merged copies count (both
    // mandatory) forces every copy to be placed regardless of origin,
    // and a combined 'copies_min' of zero (both optional) forces nothing
    // regardless of origin either way.
    ItemPos effective_copies_min_1 = effective_copies_min(reduction_item_types, item_type_id_1);
    ItemPos effective_copies_min_2 = effective_copies_min(reduction_item_types, item_type_id_2);
    bool item_1_fully_mandatory = (effective_copies_min_1 == item_1.copies);
    bool item_2_fully_mandatory = (effective_copies_min_2 == item_2.copies);
    bool item_1_fully_optional = (effective_copies_min_1 == 0);
    bool item_2_fully_optional = (effective_copies_min_2 == 0);
    if (!(item_1_fully_mandatory || item_1_fully_optional)
            || !(item_2_fully_mandatory || item_2_fully_optional))
        return false;
    if (item_1_fully_mandatory != item_2_fully_mandatory)
        return false;

    return true;
}

ItemPos Reduction::effective_copies_min(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id) const
{
    const ItemType& original_item_type = original_instance_->item_type(item_type_id);
    // For 'Knapsack', 'reduction_item_types[item_type_id].copies' can only
    // ever have shrunk below 'original_item_type.copies' via
    // 'remove_negative_profit_items' (companion absorption never runs
    // there - see 'companion_absorption_applies') - copies trimmed away
    // that way are simply gone, never placed anywhere, unlike companion-
    // absorbed copies (folded into another item's own dedicated
    // placement) - so the "subtract what's already accounted for
    // elsewhere" formula below does not apply; the requirement instead
    // stays exactly 'original_item_type.copies_min', just capped by
    // whatever copies genuinely remain (never below current 'copies',
    // which would be unsatisfiable). 'remove_negative_profit_items' only
    // ever trims down to exactly 'copies_min' itself, so this capping is
    // never actually tightened in practice - it only guards the
    // invariant.
    if (original_instance_->objective() == Objective::Knapsack) {
        return std::min(
                original_item_type.copies_min,
                reduction_item_types[item_type_id].copies);
    }
    ItemPos consumed = original_item_type.copies - reduction_item_types[item_type_id].copies;
    return std::max((ItemPos)0, original_item_type.copies_min - consumed);
}

bool Reduction::remove_negative_profit_items(
        std::vector<ReductionItemType>& reduction_item_types) const
{
    bool found = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        if (original_instance_->item_type(item_type_id).profit >= 0)
            continue;
        ItemPos min_copies = effective_copies_min(reduction_item_types, item_type_id);
        if (item.copies <= min_copies) {
            // Already at (or, degenerately, past) its own minimum -
            // nothing optional left to trim.
            continue;
        }
        item.copies = min_copies;
        if (item.copies == 0)
            item.removed = true;
        found = true;
    }
    return found;
}

namespace
{

/**
 * 'true' iff 'item_type_2''s own footprint fits within every orientation
 * 'item_type_1''s own 'oriented' flag allows - the same helper (same
 * name, same semantics) 'benders_decomposition.cpp' uses for its own
 * item-type precedence computation, reimplemented locally here since it
 * is not shared/exported (see 'Reduction::item_type_dominates''s own doc
 * comment for why this class needs the identical check).
 */
bool item_type_fits_footprint_of(
        const ItemType& item_type_2,
        const ItemType& item_type_1)
{
    std::vector<std::pair<Length, Length>> footprints_1;
    footprints_1.push_back({item_type_1.rect.x, item_type_1.rect.y});
    if (!item_type_1.oriented)
        footprints_1.push_back({item_type_1.rect.y, item_type_1.rect.x});

    for (const std::pair<Length, Length>& footprint: footprints_1) {
        bool fits = (item_type_2.rect.x <= footprint.first
                    && item_type_2.rect.y <= footprint.second)
                || (!item_type_2.oriented
                    && item_type_2.rect.y <= footprint.first
                    && item_type_2.rect.x <= footprint.second);
        if (!fits)
            return false;
    }
    return true;
}

/** Maximum value in 'schedule', or 0 if empty (no consumption at all). */
double max_or_zero(const std::vector<double>& schedule)
{
    if (schedule.empty())
        return 0.0;
    return *std::max_element(schedule.begin(), schedule.end());
}

/** Minimum value in 'schedule', or 0 if empty (no consumption at all). */
double min_or_zero(const std::vector<double>& schedule)
{
    if (schedule.empty())
        return 0.0;
    return *std::min_element(schedule.begin(), schedule.end());
}

/**
 * 'true' iff 'item_type_1' and 'item_type_2' (possibly the same one
 * twice) provably cannot both be packed into a single bin of type
 * 'bin_type' - the geometric half of this check (a direct generalization
 * of the single-item "self-incompatible" check: substitute
 * 'item_type_2 := item_type_1' and it reduces exactly to it, since
 * '2 * x > bin_x' follows from 'x > bin_x / 2' plus 'x > bin_x / 2') is
 * the same helper 'benders_decomposition.cpp' uses for its own no-good
 * cuts, reimplemented locally here since it is not shared/exported.
 * Extended with a weight and a per-resource check (only ever considering
 * non-'penalize' resources - a 'penalize' resource never makes a bin
 * outright infeasible, so it can never prove two items incompatible the
 * same hard way): either one alone is enough to prove the pigeonhole
 * argument, so this returns 'true' as soon as any one of the three
 * (geometric, weight, any one resource) holds.
 */
bool items_incompatible(
        ItemTypeId item_type_id_1,
        const ItemType& item_type_1,
        ItemTypeId item_type_id_2,
        const ItemType& item_type_2,
        const BinType& bin_type)
{
    bool geometrically_incompatible =
        (!item_type_1.oriented
                && !item_type_2.oriented
                && item_type_1.rect.min() + item_type_2.rect.min() > bin_type.rect.max())
        || (item_type_1.oriented
                && item_type_2.oriented
                && item_type_1.rect.x + item_type_2.rect.x > bin_type.rect.x
                && item_type_1.rect.y + item_type_2.rect.y > bin_type.rect.y)
        || (!item_type_1.oriented
                && item_type_2.oriented
                && item_type_1.rect.max() + item_type_2.rect.x > bin_type.rect.x
                && item_type_1.rect.max() + item_type_2.rect.y > bin_type.rect.y)
        || (item_type_1.oriented
                && !item_type_2.oriented
                && item_type_1.rect.x + item_type_2.rect.max() > bin_type.rect.x
                && item_type_1.rect.y + item_type_2.rect.max() > bin_type.rect.y);
    if (geometrically_incompatible)
        return true;

    if (item_type_1.weight + item_type_2.weight > bin_type.maximum_weight)
        return true;

    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);
        if (resource.penalize)
            continue;
        // Uses the *smallest* value anywhere in each item type's own
        // per-copy schedule (not just its first entry, which need not be
        // the smallest for a non-uniform schedule) - proving
        // incompatibility this way means even the most favorable possible
        // combined contribution from one occurrence of each already
        // exceeds capacity, a guaranteed overflow regardless of which
        // copy index either one actually ends up placed at.
        const std::vector<std::vector<double>>& item_consumptions = resource.item_consumptions;
        static const std::vector<double> empty_schedule;
        const std::vector<double>& schedule_1
            = (item_type_id_1 < (ItemTypeId)item_consumptions.size())?
                item_consumptions[item_type_id_1]: empty_schedule;
        const std::vector<double>& schedule_2
            = (item_type_id_2 < (ItemTypeId)item_consumptions.size())?
                item_consumptions[item_type_id_2]: empty_schedule;
        if (min_or_zero(schedule_1) + min_or_zero(schedule_2) > resource.capacity)
            return true;
    }

    return false;
}

}

bool Reduction::item_type_dominates(
        ItemTypeId item_type_id_a,
        ItemTypeId item_type_id_b) const
{
    if (item_type_id_a == item_type_id_b)
        return false;

    const ItemType& item_a = original_instance_->item_type(item_type_id_a);
    const ItemType& item_b = original_instance_->item_type(item_type_id_b);

    if (item_a.profit < item_b.profit)
        return false;
    if (item_a.group_id != item_b.group_id)
        return false;
    if (item_a.weight > item_b.weight)
        return false;
    if (!item_type_fits_footprint_of(item_a, item_b))
        return false;

    // Eligibility superset (and, redundantly but harmlessly, geometric
    // fit, already established above): A must be usable everywhere B is
    // - see 'benders_decomposition.cpp''s own 'compute_item_type_precedences'
    // for the same reuse of 'item_type_fits_bin_type' for this purpose.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        if (!original_instance_->item_type_fits_bin_type(item_type_id_b, bin_type_id))
            continue;
        if (!original_instance_->item_type_fits_bin_type(item_type_id_a, bin_type_id))
            return false;
    }

    // Resources: A's own consumption must never exceed B's, for every bin
    // type/resource - see this method's own doc comment in
    // 'reduction.hpp' for why the conservative max(A)-vs-min(B) comparison
    // is sound (if less effective) regardless of copy-index alignment.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = original_instance_->bin_type(bin_type_id);
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const std::vector<std::vector<double>>& item_consumptions
                = bin_type.resource(resource_id).item_consumptions;
            static const std::vector<double> empty_schedule;
            const std::vector<double>& schedule_a
                = (item_type_id_a < (ItemTypeId)item_consumptions.size())?
                    item_consumptions[item_type_id_a]: empty_schedule;
            const std::vector<double>& schedule_b
                = (item_type_id_b < (ItemTypeId)item_consumptions.size())?
                    item_consumptions[item_type_id_b]: empty_schedule;
            if (max_or_zero(schedule_a) > min_or_zero(schedule_b))
                return false;
        }
    }

    return true;
}

bool Reduction::items_provably_incompatible(
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2) const
{
    const ItemType& item_type_1 = original_instance_->item_type(item_type_id_1);
    const ItemType& item_type_2 = original_instance_->item_type(item_type_id_2);

    // Incompatible overall iff provably incompatible in *every* bin type
    // either one could ever be placed in: a bin type neither is eligible
    // for is irrelevant (never a place they could coexist anyway); a bin
    // type only one is eligible for already keeps them apart there on its
    // own; and a bin type checked 'false' below is a real, uncovered
    // possibility for them to coexist, which rules out "incompatible"
    // overall.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        bool eligible_1 = original_instance_->item_type_fits_bin_type(item_type_id_1, bin_type_id);
        bool eligible_2 = original_instance_->item_type_fits_bin_type(item_type_id_2, bin_type_id);
        if (!eligible_1 || !eligible_2)
            continue;
        if (!items_incompatible(
                item_type_id_1, item_type_1,
                item_type_id_2, item_type_2,
                original_instance_->bin_type(bin_type_id)))
            return false;
    }
    return true;
}

bool Reduction::remove_dominated_items(
        std::vector<ReductionItemType>& reduction_item_types) const
{
    bool found = false;
    for (ItemTypeId item_type_id_b = 0;
            item_type_id_b < (ItemTypeId)reduction_item_types.size();
            ++item_type_id_b) {
        if (reduction_item_types[item_type_id_b].removed)
            continue;
        if (original_instance_->item_type(item_type_id_b).copies_min != 0)
            continue;
        for (ItemTypeId item_type_id_a = 0;
                item_type_id_a < (ItemTypeId)reduction_item_types.size();
                ++item_type_id_a) {
            if (item_type_id_a == item_type_id_b)
                continue;
            if (reduction_item_types[item_type_id_a].removed)
                continue;
            if (!item_type_dominates(item_type_id_a, item_type_id_b))
                continue;
            if (!items_provably_incompatible(item_type_id_a, item_type_id_b))
                continue;
            reduction_item_types[item_type_id_b].removed = true;
            found = true;
            break;
        }
    }
    return found;
}

bool Reduction::bin_type_dominates(
        BinTypeId bin_type_id_a,
        BinTypeId bin_type_id_b) const
{
    if (bin_type_id_a == bin_type_id_b)
        return false;

    const BinType& bin_type_a = original_instance_->bin_type(bin_type_id_a);
    const BinType& bin_type_b = original_instance_->bin_type(bin_type_id_b);

    // Bins are never placed rotated (unlike items), so a plain per-axis
    // comparison - no orientation handling - is all that is needed here.
    // Unlike item dominance (where the *smaller* item wins, since it uses
    // less of a shared, limited resource for the same-or-better profit),
    // bin dominance runs the other way: A must be at least as big as B in
    // both dimensions to be able to hold anything B could - a *smaller*
    // bin is strictly less capable, never a safe substitute.
    if (bin_type_a.rect.x < bin_type_b.rect.x || bin_type_a.rect.y < bin_type_b.rect.y)
        return false;
    if (bin_type_a.cost > bin_type_b.cost)
        return false;
    if (bin_type_a.maximum_weight < bin_type_b.maximum_weight)
        return false;

    // Every one of these is a genuinely complex per-bin-type feature this
    // class does not attempt to compare or reconcile across two different
    // bin types - matching this class's existing precedent elsewhere
    // (see e.g. 'companion_absorption_applies').
    if (!bin_type_a.defects.empty() || !bin_type_b.defects.empty())
        return false;
    if (!bin_type_a.fixed_items.empty() || !bin_type_b.fixed_items.empty())
        return false;
    if (bin_type_a.semi_trailer_truck_data.is || bin_type_b.semi_trailer_truck_data.is)
        return false;

    // Resources: 'resource_id' carries no meaning across two different bin
    // types (it is just the next index into that one bin type's own
    // 'resources' vector - see 'InstanceBuilder::add_bin_type_resource()'),
    // so there is no sound way to reconcile A's resources against B's - any
    // resource of A's own blocks dominance. A bin type with no resources at
    // all is unrestricted along every resource dimension though, so B alone
    // having resources is fine - except a 'penalize' resource of B's with a
    // negative 'penalty' (a one-time profit bonus for crossing its
    // capacity - see 'Resource''s own doc comment and the same exclusion in
    // 'remove_negative_profit_items()'), which A, having no matching
    // resource, could never replicate: losing access to that bonus would
    // make A strictly worse than B for some solutions.
    if (bin_type_a.number_of_resources() > 0)
        return false;
    for (ResourceId resource_id = 0;
            resource_id < bin_type_b.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type_b.resource(resource_id);
        if (resource.penalize && resource.penalty < 0)
            return false;
    }

    // Eligibility superset: A must support every eligibility id B does.
    for (EligibilityId eligibility_id: bin_type_b.eligibility_ids) {
        if (std::find(
                    bin_type_a.eligibility_ids.begin(),
                    bin_type_a.eligibility_ids.end(),
                    eligibility_id)
                == bin_type_a.eligibility_ids.end())
            return false;
    }

    return true;
}

std::vector<bool> Reduction::compute_dominated_bin_types() const
{
    std::vector<bool> dominated(original_instance_->number_of_bin_types(), false);
    // See the class-level doc comment's "Removing dominated bin types"
    // paragraph for why this bound - unlike the equivalent one for items
    // - is both sound and actually achievable.
    BinPos enough_copies = (BinPos)original_instance_->number_of_items();
    for (BinTypeId bin_type_id_b = 0;
            bin_type_id_b < original_instance_->number_of_bin_types();
            ++bin_type_id_b) {
        if (original_instance_->bin_type(bin_type_id_b).copies_min != 0)
            continue;
        for (BinTypeId bin_type_id_a = 0;
                bin_type_id_a < original_instance_->number_of_bin_types();
                ++bin_type_id_a) {
            if (bin_type_id_a == bin_type_id_b)
                continue;
            if (dominated[bin_type_id_a])
                continue;
            if (original_instance_->bin_type(bin_type_id_a).copies < enough_copies)
                continue;
            if (!bin_type_dominates(bin_type_id_a, bin_type_id_b))
                continue;
            dominated[bin_type_id_b] = true;
            break;
        }
    }
    return dominated;
}

bool Reduction::merge_identical_items(
        std::vector<ReductionItemType>& reduction_item_types)
{
    bool found = false;
    for (ItemTypeId item_type_id_1 = 0;
            item_type_id_1 < (ItemTypeId)reduction_item_types.size();
            ++item_type_id_1) {
        if (reduction_item_types[item_type_id_1].removed)
            continue;
        for (ItemTypeId item_type_id_2 = item_type_id_1 + 1;
                item_type_id_2 < (ItemTypeId)reduction_item_types.size();
                ++item_type_id_2) {
            if (reduction_item_types[item_type_id_2].removed)
                continue;
            if (!items_mergeable(reduction_item_types, item_type_id_1, item_type_id_2))
                continue;
            reduction_item_types[item_type_id_2].removed = true;
            reduction_item_types[item_type_id_2].merged_into = item_type_id_1;
            found = true;
        }
    }
    return found;
}

Instance Reduction::reduction_to_instance(
        const std::vector<ReductionItemType>& reduction_item_types,
        const std::vector<bool>& bin_type_removed)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(original_instance_->objective());
    instance_builder.set_parameters(original_instance_->parameters());
    instance_builder.set_feasibility_callback(original_instance_->feasibility_callback());

    BinPos number_of_dedicated_bins = this->number_of_dedicated_bins();
    ItemPos remaining_items = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        // A merged-away item type (see 'merge_identical_items') is
        // 'removed' too, but its copies still need bin capacity in the
        // reduced instance - just folded into its survivor's own count
        // below - unlike every other reason an item type can be 'removed'
        // (absorbed as a companion, or consumed into a dedicated bin),
        // which genuinely need none.
        if (!reduction_item_types[item_type_id].removed
                || reduction_item_types[item_type_id].merged_into != -1)
            remaining_items += original_instance_->item_type(item_type_id).copies;
    }

    // Maps 'original_instance_''s own bin type ids to the reduced
    // instance's own ('-1' for a bin type 'compute_dominated_bin_types'
    // found dominated and skipped below) - needed both by the resource-
    // consumption-copying loop further down (which, unlike the item-type
    // loop, is keyed by bin type id directly) and by 'unreduce_solution'
    // (via 'reduced_to_original_bin_type_id_', this vector's own inverse,
    // populated alongside it below).
    std::vector<BinTypeId> original_to_reduced_bin_type_id(
            original_instance_->number_of_bin_types(), -1);
    reduced_to_original_bin_type_id_.clear();

    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        if (bin_type_id < (BinTypeId)bin_type_removed.size()
                && bin_type_removed[bin_type_id]) {
            // Dominated by some other bin type with enough copies to
            // cover any genuine use of this one (see
            // 'compute_dominated_bin_types') - skipped entirely, rather
            // than built with e.g. 0 copies, so it does not even appear
            // in the reduced instance.
            continue;
        }
        BinTypeId new_bin_type_id = instance_builder.add_bin_type(*original_instance_, bin_type_id);
        original_to_reduced_bin_type_id[bin_type_id] = new_bin_type_id;
        reduced_to_original_bin_type_id_.push_back(bin_type_id);
        // Override to this bin's own true, possibly-shrunk dimensions (see
        // 'true_bin_rect_'/'reduce_full_span_items') - a no-op when
        // 'reduce_full_span_items' never actually ran ('true_bin_rect_'
        // still holds this same bin type's own original dimensions then,
        // set at the top of the constructor). Skipped whenever
        // 'true_bin_rect_' is non-positive: 'proven_infeasible_' is
        // already 'true' in that case (see 'reduce_full_span_items''s own
        // guard), so this instance is never actually solved - using the
        // original, still-positive dimensions here is just a harmless
        // placeholder, the same reasoning as the bin-copies-exhausted case
        // just below.
        if (reduce_full_span_items_applies(*original_instance_)
                && true_bin_rect_.x > 0 && true_bin_rect_.y > 0) {
            instance_builder.set_bin_type_rect(
                    new_bin_type_id, true_bin_rect_.x, true_bin_rect_.y);
        }
        if (number_of_dedicated_bins > 0) {
            // Fold the dedicated bins' capacity out of the reduced
            // instance's own bin type copies: they are entirely absent
            // from this instance (see 'FullBinItem'/'PerfectPair'), so
            // nothing here should ever be allowed to use their reserved
            // capacity. 'reduce_full_bin_items'/'reduce_perfect_pairs'
            // never reserve more than 'bin_type.copies' itself, so this
            // subtraction never goes negative - but it can reach exactly
            // 0, which 'InstanceBuilder' rejects outright ('copies' must
            // be > 0 or -1), so that case needs its own handling below.
            const BinType& original_bin_type = original_instance_->bin_type(bin_type_id);
            if (original_bin_type.copies >= 0) {
                BinPos new_copies = original_bin_type.copies - number_of_dedicated_bins;
                if (new_copies > 0) {
                    instance_builder.set_bin_type_copies(new_bin_type_id, new_copies);
                } else if (remaining_items == 0) {
                    // Zero bin capacity left, but also nothing left that
                    // could ever need it (every item type was itself
                    // consumed by a dedicated-bin reservation): leave the
                    // bin type's copies at its original (nonzero) value -
                    // harmless, since a solve over zero items never
                    // touches bin capacity at all.
                } else {
                    // Zero bin capacity left, with real items still
                    // needing to be packed: the original instance needs
                    // strictly more bins than this bin type has copies
                    // for, so it is infeasible outright (only possible for
                    // 'Feasibility', whose bin copies are genuinely finite
                    // - the cap in 'reduce_full_bin_items'/
                    // 'reduce_perfect_pairs' only prevents *exceeding*
                    // available copies, not *exhausting* them). Record it
                    // and leave a harmless nonzero placeholder so the
                    // instance still builds; the recursive solve on it is
                    // never actually reached (see 'optimize()', which
                    // checks 'proven_infeasible()' first).
                    proven_infeasible_ = true;
                }
            }
            if (original_bin_type.copies_min > 0) {
                instance_builder.set_bin_type_copies_min(
                        new_bin_type_id,
                        std::max<BinPos>(0, original_bin_type.copies_min - number_of_dedicated_bins));
            }
        }
    }

    // A direct copy of the whole working representation, indexed by the
    // *original* instance's item type ids - see 'final_item_types_''s own
    // doc comment for why every item type needs to stay reachable here,
    // survivor or not.
    final_item_types_ = reduction_item_types;

    reduced_copy_origins_.clear();
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;

        // Per-copy origin list for this survivor: its own copies, then, in
        // discovery order, every item type merged into it (see
        // 'merge_identical_items') - any consistent order works, since
        // merged item types are interchangeable by construction (see
        // 'items_mergeable').
        std::vector<CopyOrigin> copy_origins;
        // Combined minimum copies requirement: the sum of the survivor's
        // own 'effective_copies_min' and every merged-in constituent's -
        // sound (regardless of origin order) only because 'items_mergeable'
        // already restricts every merge to same-category (fully mandatory
        // with fully mandatory, fully optional with fully optional) pairs -
        // see 'items_mergeable''s own doc comment.
        ItemPos total_copies_min = effective_copies_min(reduction_item_types, item_type_id);
        for (ItemPos copy_index = 0; copy_index < item.copies; ++copy_index)
            copy_origins.push_back({item_type_id, copy_index});
        for (ItemTypeId other_item_type_id = 0;
                other_item_type_id < (ItemTypeId)reduction_item_types.size();
                ++other_item_type_id) {
            if (reduction_item_types[other_item_type_id].merged_into != item_type_id)
                continue;
            ItemPos merged_copies = reduction_item_types[other_item_type_id].copies;
            total_copies_min += effective_copies_min(reduction_item_types, other_item_type_id);
            for (ItemPos copy_index = 0; copy_index < merged_copies; ++copy_index)
                copy_origins.push_back({other_item_type_id, copy_index});
        }

        const ItemType& original_item_type = original_instance_->item_type(item_type_id);
        ItemTypeId new_item_type_id = instance_builder.add_item_type(
                item.rect.x, item.rect.y, original_item_type.oriented);
        instance_builder.set_item_type_profit(new_item_type_id, original_item_type.profit);
        instance_builder.set_item_type_copies(new_item_type_id, (ItemPos)copy_origins.size());
        instance_builder.set_item_type_copies_min(new_item_type_id, total_copies_min);
        instance_builder.set_item_type_group(new_item_type_id, original_item_type.group_id);
        instance_builder.set_item_type_weight(new_item_type_id, original_item_type.weight);
        instance_builder.set_item_type_eligibility(new_item_type_id, original_item_type.eligibility_id);
        // Copy resource consumptions (the bin types themselves, including
        // their resources, were already copied above via 'add_bin_type(
        // *original_instance_, bin_type_id)' - using
        // 'original_to_reduced_bin_type_id' to translate ids, since
        // dominated bin types are skipped there and so no longer line up
        // 1:1 with 'original_instance_'s own).
        for (BinTypeId bin_type_id = 0;
                bin_type_id < original_instance_->number_of_bin_types();
                ++bin_type_id) {
            if (original_to_reduced_bin_type_id[bin_type_id] == -1)
                continue;
            const BinType& original_bin_type = original_instance_->bin_type(bin_type_id);
            for (ResourceId resource_id = 0;
                    resource_id < original_bin_type.number_of_resources();
                    ++resource_id) {
                const std::vector<std::vector<double>>& item_consumptions
                    = original_bin_type.resource(resource_id).item_consumptions;
                if (item_type_id >= (ItemTypeId)item_consumptions.size())
                    continue;
                const std::vector<double>& schedule = item_consumptions[item_type_id];
                for (ItemPos item_copy = 0;
                        item_copy < (ItemPos)schedule.size();
                        ++item_copy) {
                    instance_builder.add_resource_consumption(
                            original_to_reduced_bin_type_id[bin_type_id],
                            resource_id,
                            new_item_type_id,
                            item_copy,
                            schedule[item_copy]);
                }
            }
        }
        reduced_copy_origins_.push_back(std::move(copy_origins));
    }

    return instance_builder.build();
}

Length Reduction::max_achievable_dimension_sum(
        const std::vector<ReductionItemType>& reduction_item_types,
        const Instance& original_instance,
        Length capacity,
        bool width_axis,
        ItemTypeId excluded_item_type_id)
{
    multiplechoicesubsetsumsolver::InstanceBuilder mcss_instance_builder;
    mcss_instance_builder.set_capacity(capacity);
    multiplechoicesubsetsumsolver::GroupId mcss_group_id = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        bool oriented = original_instance.item_type(item_type_id).oriented;
        Length value = (width_axis)? item.rect.x: item.rect.y;
        Length rotated_value = (width_axis)? item.rect.y: item.rect.x;
        ItemPos copies = item.copies;
        if (item_type_id == excluded_item_type_id)
            --copies;
        for (ItemPos copy = 0; copy < copies; ++copy) {
            mcss_instance_builder.add_item(mcss_group_id, value);
            if (!oriented && rotated_value != value)
                mcss_instance_builder.add_item(mcss_group_id, rotated_value);
            ++mcss_group_id;
        }
    }
    multiplechoicesubsetsumsolver::Instance mcss_instance = mcss_instance_builder.build();
    multiplechoicesubsetsumsolver::Parameters mcss_parameters;
    mcss_parameters.verbosity_level = 0;
    auto mcss_output = multiplechoicesubsetsumsolver::dynamic_programming_bellman_array(
            mcss_instance,
            mcss_parameters);
    return mcss_output.bound;
}

bool Reduction::compute_shrunk_bin_sizes(
        const std::vector<ReductionItemType>& reduction_item_types,
        ShrunkBinSizes& shrunk_bin_sizes) const
{
    if (original_instance_->number_of_bin_types() != 1) {
        shrunk_bin_sizes.bin_width = 0;
        shrunk_bin_sizes.bin_height = 0;
        return false;
    }
    const BinType& bin_type = original_instance_->bin_type(0);
    shrunk_bin_sizes.bin_width = bin_type.rect.x;
    shrunk_bin_sizes.bin_height = bin_type.rect.y;

    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        if (!reduction_item_types[item_type_id].removed
                && reduction_item_types[item_type_id].copies < 0)
            return false;
    }

    // Equation (7): shrink the bin width/height to the largest achievable
    // combination of item widths/heights.
    shrunk_bin_sizes.bin_width = max_achievable_dimension_sum(
            reduction_item_types, *original_instance_, bin_type.rect.x, /* width_axis */ true);
    shrunk_bin_sizes.bin_height = max_achievable_dimension_sum(
            reduction_item_types, *original_instance_, bin_type.rect.y, /* width_axis */ false);

    return true;
}

bool Reduction::companion_absorption_applies(const Instance& instance)
{
    // Only sound for these three objectives - see the class-level doc
    // comment's "Companion absorption" paragraph.
    if (instance.objective() != Objective::BinPacking
            && instance.objective() != Objective::VariableSizedBinPacking
            && instance.objective() != Objective::Feasibility)
        return false;

    // Every check companion absorption performs (the wide/tall/both
    // companion-bin feasibility solves in 'try_reduce_group', and the
    // direct dimension-only matches in 'reduce_full_bin_items'/
    // 'reduce_perfect_pairs') reasons purely about geometry: whether a set
    // of item footprints fits together inside an empty rectangle. Four
    // instance features break that:
    //
    // - Defects: the companion-bin sub-instances built in
    //   'try_reduce_group' are always plain, defect-free rectangles (see
    //   'bin_dims' there) - a defect sitting where a companion or the big
    //   item itself would need to go is entirely invisible to the check,
    //   and 'reduce_full_bin_items'/'reduce_perfect_pairs' do not even run
    //   a solve to catch it.
    // - A finite bin weight capacity together with non-trivial item
    //   weights: unlike the wide/tall/both geometric argument, total bin
    //   weight is a *whole-bin* aggregate over every item that ends up
    //   sharing it. A validated-enlarged item's real companions are
    //   removed from the reduced instance and only reinserted by
    //   'unreduce_solution' after the downstream solve has already
    //   finished - that solve never sees them, so it can never verify the
    //   combined weight of whatever *it* additionally places in that same
    //   bin. (The reduced instance's own bookkeeping compounds this:
    //   'reduction_to_instance' carries over an enlarged item's own
    //   original weight only, silently dropping its companions'.)
    // - A non-'None' unloading constraint: the same whole-bin argument as
    //   weight - an unloading order that holds for the big item and its
    //   companions checked in isolation says nothing about whether it
    //   still holds once other items, chosen later and never part of that
    //   check, join the same bin.
    // - Resources: the exact same whole-bin argument as weight - a
    //   resource's capacity is a per-bin aggregate over every item sharing
    //   it, invisible to the per-group geometric checks here, and
    //   'reduction_to_instance' does carry resources over (so a downstream
    //   solve on the reduced instance would not silently ignore them), but
    //   that is exactly the problem: with real companion items removed
    //   from the reduced instance (only reinserted afterwards, by
    //   'unreduce_solution'), a resource-aware downstream solve would
    //   itself only ever see the validated-enlarged item's own
    //   consumption, never its companions', so it could not correctly
    //   enforce the resource's capacity across the bin either.
    //
    // Also only for instances with a single bin type - see the class-level
    // doc comment's "Companion absorption" paragraph.
    return instance.number_of_bin_types() == 1
            && instance.number_of_defects() == 0
            && instance.unloading_constraint() == UnloadingConstraint::None
            && !instance.weight_matters()
            && !instance.resources_matter();
}

bool Reduction::lift_item_dimensions_applies(const Instance& instance)
{
    // Unlike 'companion_absorption_applies', not restricted by objective,
    // weight, resources, or unloading constraint - see the class-level
    // doc comment's "Lifting item dimensions via subset sum" paragraph
    // for why: this operation never removes or hides any item type from
    // the reduced instance (a lifted item's own weight/resource
    // consumption/unloading group is untouched, and every other item type
    // stays fully present and independently placed), so none of the
    // whole-bin arguments that block companion absorption apply here.
    //
    // Still needs a single bin type - the bin's own true dimension this
    // operation reasons against would otherwise be ambiguous - and no
    // defects - a defect sitting inside the margin this operation claims
    // is "provably empty" would be invisible to its purely 1D subset-sum
    // argument, the same blind spot as companion absorption's own
    // geometric checks (see 'companion_absorption_applies').
    return instance.number_of_bin_types() == 1
            && instance.number_of_defects() == 0;
}

bool Reduction::remove_negative_profit_items_applies(const Instance& instance)
{
    if (instance.objective() != Objective::Knapsack)
        return false;

    // A 'penalize' resource with a negative 'penalty' is a one-time
    // profit bonus the first time a bin's consumption crosses its
    // capacity - a negative-profit item could still be worth including if
    // its own consumption helps trigger that crossing, an indirect
    // benefit this purely per-item check cannot see. See the class-level
    // doc comment's "Trimming negative-profit item types" paragraph.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const Resource& resource = bin_type.resource(resource_id);
            if (resource.penalize && resource.penalty < 0)
                return false;
        }
    }
    return true;
}

bool Reduction::remove_dominated_items_applies(const Instance& instance)
{
    return instance.objective() == Objective::Knapsack;
}

bool Reduction::remove_dominated_bin_types_applies(const Instance& instance)
{
    // Not for 'BinPacking': unlike every other objective this class
    // handles, its bin types must be used in the exact order they are
    // declared - each one's own copies fully exhausted before the next
    // is ever touched (see 'tree_search.cpp''s own 'BranchingScheme'
    // constructor, which builds its bin sequence in declared order for
    // every objective except 'Knapsack', and 'onedimensional''s own
    // 'Solution::bin_type_order_feasible()', explicitly scoped to
    // 'BinPacking' alone - the order is a genuine, enforced constraint on
    // solution validity only there, not merely a search-strategy detail).
    // Removing a bin type shifts every later one's position in that fixed
    // sequence, making it available *earlier* than the original problem
    // ever allowed - not an equivalent substitution, a different problem
    // entirely, regardless of how thoroughly one bin type dominates
    // another.
    if (instance.objective() == Objective::BinPacking)
        return false;
    return instance.number_of_bin_types() > 1;
}

bool Reduction::reduce_full_span_items_applies(const Instance& instance)
{
    // Only 'Feasibility' - see this method's own doc comment in
    // 'reduction.hpp' for why 'OpenDimensionX'/'OpenDimensionY' are not
    // (yet) included.
    if (instance.objective() != Objective::Feasibility)
        return false;

    // Exactly one bin *copy*, on top of every one of
    // 'companion_absorption_applies''s own exclusions (checked directly
    // here rather than by calling it, since that also allows 'BinPacking'/
    // 'VariableSizedBinPacking', which this operation never does) - see
    // this method's own doc comment in 'reduction.hpp'.
    return instance.number_of_bin_types() == 1
            && instance.bin_type(0).copies == 1
            && instance.number_of_defects() == 0
            && instance.unloading_constraint() == UnloadingConstraint::None
            && !instance.weight_matters()
            && !instance.resources_matter();
}

bool Reduction::lift_item_dimensions(
        std::vector<ReductionItemType>& reduction_item_types,
        Length bin_w,
        Length bin_h) const
{
    bool found = false;
    found |= lift_item_dimensions_axis(reduction_item_types, bin_w, /* width_axis */ true);
    found |= lift_item_dimensions_axis(reduction_item_types, bin_h, /* width_axis */ false);
    return found;
}

bool Reduction::lift_item_dimensions_axis(
        std::vector<ReductionItemType>& reduction_item_types,
        Length bin_dimension,
        bool width_axis) const
{
    // See this method's own doc comment in 'reduction.hpp' for why any
    // not-yet-'removed' item type with infinite copies rules this out
    // entirely for every item type on this axis, not just for itself.
    for (const ReductionItemType& item: reduction_item_types) {
        if (!item.removed && item.copies < 0)
            return false;
    }

    bool found = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        // Requires the item to be 'oriented': growing 'rect.x'/'rect.y'
        // directly only means anything if the item is fixed at that
        // declared orientation - a non-oriented item's rotated
        // presentation swaps which of its two dimensions plays "width"
        // and which plays "height", so growing one declared field alone
        // would silently corrupt its footprint on a downstream solve
        // choosing the *other* orientation (concretely: a non-oriented
        // 5x10 item whose declared width got lifted to 10 would become a
        // bogus 10x10 - not "5x10, rotatable" and not "10x5" either, an
        // item type larger than either of its two true presentations).
        // Same precondition 'is_big' already requires for 'Wide'/'Tall'
        // - see its own doc comment.
        if (!original_instance_->item_type(item_type_id).oriented)
            continue;
        // Requires exactly 1 remaining copy: growing 'rect.x'/'rect.y'
        // applies uniformly to *every* copy of this item type at once,
        // but 'max_achievable_dimension_sum''s "leave one out" adjustment
        // (see its own doc comment) only proves the bound for a single
        // occurrence - each of several copies would need its own row/
        // column, each with its own claim on the same limited pool of
        // other items, which a single shared "achievable_other" value
        // cannot back simultaneously (concretely: 3 copies of a 4x4 item
        // alongside 2 more 4x4 copies of another type, in a 100x100 bin -
        // "leave one out" finds only 16 of achievable width from the 4
        // other copies, so naively lifting the *type* would grow all 3
        // copies to 84 wide each, needing 252 combined - far more than
        // any real arrangement, and more than the bin itself, could ever
        // supply).
        if (item.copies != 1)
            continue;
        Length current_dimension = (width_axis)? item.rect.x: item.rect.y;
        Length capacity = bin_dimension - current_dimension;
        if (capacity <= 0) {
            // Already at (or, degenerately, past) the bin's own shrunk
            // dimension on this axis - nothing left to lift into.
            continue;
        }

        Length achievable_other = max_achievable_dimension_sum(
                reduction_item_types, *original_instance_, capacity, width_axis, item_type_id);
        if (achievable_other >= capacity) {
            // No provable gap: the other item types could, in the worst
            // case, combine to reach all the way up to the bin's shrunk
            // dimension alongside this one - nothing to lift safely.
            continue;
        }

        // The gap ('capacity - achievable_other') is provably,
        // permanently unreachable by any combination of the other item
        // types, so this item type's own dimension can close it exactly.
        Length new_dimension = bin_dimension - achievable_other;
        if (width_axis) {
            item.rect.x = new_dimension;
        } else {
            item.rect.y = new_dimension;
        }
        found = true;
    }
    return found;
}

Reduction::Reduction(
        const Instance& instance,
        const ReductionParameters& parameters):
    original_instance_(&instance),
    instance_(instance)
{
    // Working representation: a stable 1:1 copy of the original instance's
    // item types (see 'ReductionItemType'). Always built and compacted
    // back via 'reduction_to_instance' at the end (even when
    // 'parameters.reduce' is 'false' below, in which case it is an
    // identity rebuild): this keeps 'final_item_types_'/
    // 'reduced_copy_origins_' always populated, so 'unreduce_solution'
    // never needs a separate no-op code path. Callers that already know
    // 'parameters.reduce' is 'false' are better off not constructing a
    // 'Reduction' at all - but the constructor still handles that case
    // correctly, both for simplicity (one code path regardless of caller
    // diligence) and to avoid a second, redundant place that would need
    // to stay in sync with it.
    std::vector<ReductionItemType> reduction_item_types(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        reduction_item_types[item_type_id].rect = instance.item_type(item_type_id).rect;
        reduction_item_types[item_type_id].copies = instance.item_type(item_type_id).copies;
    }

    // Initialized here, ahead of the 'parameters.reduce' check below, so
    // 'reduction_to_instance' can always safely read it (a no-op override
    // to the original instance's own bin dimensions when
    // 'reduce_full_span_items' never actually runs) - see
    // 'true_bin_rect_''s own doc comment.
    if (reduce_full_span_items_applies(instance))
        true_bin_rect_ = instance.bin_type(0).rect;

    if (!parameters.reduce) {
        instance_ = reduction_to_instance(reduction_item_types);
        return;
    }

    // Every sub-operation below is independently gated by its own
    // 'parameters' flag - see 'ReductionParameters''s own doc comments -
    // on top of 'companion_absorption_applies' for the four companion-
    // absorption ones (wide/tall, both, full-bin items, perfect pairs),
    // 'lift_item_dimensions_applies' for item dimension lifting, and
    // 'remove_negative_profit_items_applies'/'remove_dominated_items_applies'
    // for the two 'Knapsack'-only trims below; see each one's own doc
    // comment for why they share a different precondition from
    // 'merge_identical_items' - and from each other.
    bool companion_absorption_ok = companion_absorption_applies(instance);
    bool run_wide_tall = companion_absorption_ok && parameters.enlarge_wide_tall_items;
    bool run_both = companion_absorption_ok && parameters.enlarge_both_items;
    bool run_full_bin_items = companion_absorption_ok && parameters.reduce_full_bin_items;
    bool run_perfect_pairs = companion_absorption_ok && parameters.reduce_perfect_pairs;
    bool run_full_span_items = reduce_full_span_items_applies(instance) && parameters.enlarge_wide_tall_items;
    bool run_lift_item_dimensions = lift_item_dimensions_applies(instance) && parameters.lift_item_dimensions;
    bool run_remove_negative_profit_items = remove_negative_profit_items_applies(instance)
            && parameters.remove_negative_profit_items;
    bool run_remove_dominated_items = remove_dominated_items_applies(instance)
            && parameters.remove_dominated_items;
    bool run_remove_dominated_bin_types = remove_dominated_bin_types_applies(instance)
            && parameters.remove_dominated_bin_types;

    // Run once, upfront, rather than inside the main per-round loop below:
    // neither an item type's own profit nor any of the other original-
    // instance properties 'remove_dominated_items'/'item_type_dominates'
    // read ever changes across rounds (nothing else in this class ever
    // touches them), so there is nothing to re-check repeatedly - unlike
    // every operation in the loop, none of which is either one's own
    // precondition or result. Negative-profit trimming runs first: it can
    // only ever remove or shrink a candidate's own copies, which can only
    // make 'remove_dominated_items''s own "enough copies" check *harder*
    // to satisfy for a would-be dominator, never easier - so running it
    // first is always at least as effective as the other order, never
    // less (and avoids ever using an already-doomed negative-profit item
    // type as a dominator in the first place).
    if (run_remove_negative_profit_items)
        remove_negative_profit_items(reduction_item_types);
    if (run_remove_dominated_items)
        remove_dominated_items(reduction_item_types);

    // Bin types are never touched by anything else in this class (unlike
    // item types' own working 'rect'/'copies'), so this needs no working
    // representation and no per-round re-checking either - computed once
    // here, and only consumed at the very end, by 'reduction_to_instance'.
    std::vector<bool> bin_type_removed;
    if (run_remove_dominated_bin_types)
        bin_type_removed = compute_dominated_bin_types();

    // Main reduction loop: repeat {lift item dimensions, wide, tall,
    // both, full-bin items, perfect pairs} until a full pass finds
    // nothing new (a later case's success can free up items that make an
    // earlier case worth retrying). Runs unconditionally, even when every
    // 'run_*' flag above is 'false': the first round then finds nothing
    // (every sub-operation below is skipped) and the loop exits
    // immediately after it, same end result as skipping the loop
    // outright, without needing a separate guard here to special-case it.
    for (Counter round_number = 0;
            round_number < parameters.maximum_number_of_rounds;
            ++round_number) {
        if (parameters.timer.needs_to_end())
            break;

        bool found = false;

        // Shrunk bin dimensions (equation (7), "shrinking the bins": see
        // 'compute_shrunk_bin_sizes'), recomputed every round from
        // 'reduction_item_types''s then-current, still-remaining item set
        // - see 'compute_shrunk_bin_sizes''s own doc comment for why
        // excluding already-'removed' item types only ever tightens this
        // bound, never understates it: once an item type is the only one
        // left, the bound correctly collapses to exactly its own current
        // dimensions, letting 'reduce_full_bin_items' below claim it as a
        // dedicated bin even when its dimensions never matched the
        // *true* bin - a sound, more aggressive equivalent encoding of
        // the same physical solution, not a different one, since nothing
        // else remains that could ever share a bin with it anyway.
        //
        // Computed first, ahead of every sub-operation below (including
        // 'lift_item_dimensions'): 'bin_w'/'bin_h' is itself already the
        // tightest sound substitute for the bin's own true dimension
        // throughout this whole class, so every operation that reasons
        // against "the bin's own dimension" - lifting included - should
        // use it rather than the true, looser one; shrinking first also
        // means a companionless lift that reaches exactly 'bin_w'/'bin_h'
        // on both axes chains directly into 'reduce_full_bin_items'/
        // 'reduce_perfect_pairs' below in the very same round, instead of
        // needing an extra round to be picked up.
        ShrunkBinSizes shrunk_bin_sizes;
        compute_shrunk_bin_sizes(reduction_item_types, shrunk_bin_sizes);
        Length bin_w = shrunk_bin_sizes.bin_width;
        Length bin_h = shrunk_bin_sizes.bin_height;

        if (run_lift_item_dimensions)
            found |= lift_item_dimensions(reduction_item_types, bin_w, bin_h);
        if (parameters.timer.needs_to_end())
            break;

        if (run_wide_tall)
            found |= reduce_group(reduction_item_types, parameters, EnlargementCase::Wide, bin_w, bin_h);
        if (parameters.timer.needs_to_end())
            break;
        if (run_wide_tall)
            found |= reduce_group(reduction_item_types, parameters, EnlargementCase::Tall, bin_w, bin_h);
        if (parameters.timer.needs_to_end())
            break;
        if (run_both)
            found |= reduce_both_groups(reduction_item_types, parameters, bin_w, bin_h);
        if (parameters.timer.needs_to_end())
            break;
        if (run_full_bin_items)
            found |= reduce_full_bin_items(
                    reduction_item_types, bin_w, bin_h);
        if (parameters.timer.needs_to_end())
            break;
        if (run_perfect_pairs)
            found |= reduce_perfect_pairs(
                    reduction_item_types, bin_w, bin_h);
        if (parameters.timer.needs_to_end())
            break;
        // Runs last: a plain pin is never more valuable than the two
        // dedicated-bin reductions just above for the same item type, so
        // giving them first crack at it each round avoids ever burning a
        // pin on an item that would have formed a strictly better
        // 'FullBinItem'/'PerfectPair' instead - see this function's own
        // doc comment in 'reduction.hpp'.
        if (run_full_span_items)
            found |= reduce_full_span_items(reduction_item_types);
        if (proven_infeasible_)
            break;
        if (!found)
            break;
    }

    if (!parameters.timer.needs_to_end() && parameters.merge_identical_items)
        merge_identical_items(reduction_item_types);

    instance_ = reduction_to_instance(reduction_item_types, bin_type_removed);
}

void Reduction::place_item_and_companions(
        SolutionBuilder& solution_builder,
        BinPos bin_pos,
        ItemTypeId item_type_id,
        Point bl_corner,
        bool rotate,
        const std::vector<CompanionItem>& companions) const
{
    solution_builder.add_item(bin_pos, item_type_id, bl_corner, rotate);
    for (const CompanionItem& companion: companions) {
        Point companion_bl_corner{
            bl_corner.x + companion.offset.x,
            bl_corner.y + companion.offset.y};
        place_item_and_companions(
                solution_builder,
                bin_pos,
                companion.item_type_id,
                companion_bl_corner,
                companion.rotate,
                companion.nested_companions);
    }
}

Solution Reduction::unreduce_solution(
        const Solution& solution) const
{
    SolutionBuilder solution_builder(*original_instance_);

    // For each reduced item type, how many of its placements have been
    // encountered so far while scanning 'solution' - indexes both
    // 'reduced_copy_origins_' (to resolve which original item type/copy a
    // given placement actually is - see 'merge_identical_items') and, once
    // resolved, that original item type's own 'companions_by_copy' (any
    // consistent order works for either, since a reduced item type's
    // copies are interchangeable by construction, whether from companion
    // enlargement or from a merge).
    std::vector<ItemPos> next_copy_index(instance_.number_of_item_types(), 0);

    // Set alongside every bin this loop builds (see 'full_span_items_''s
    // own placement below) - only ever meaningfully read when
    // 'full_span_items_' is non-empty, which itself only ever happens when
    // 'original_instance_' has exactly one bin type with exactly one
    // copy (see 'reduce_full_span_items_applies'), so 'solution' here
    // builds at most a single bin overall and this is unambiguous.
    BinPos single_bin_pos = -1;
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        // Translate the reduced instance's own bin type id back to
        // 'original_instance_''s id space - identity unless
        // 'compute_dominated_bin_types' skipped some bin types when
        // building 'instance_' (see 'reduced_to_original_bin_type_id_'
        // own doc comment; always populated by 'reduction_to_instance',
        // regardless of whether any bin type was actually skipped).
        BinTypeId original_bin_type_id = reduced_to_original_bin_type_id_[solution_bin.bin_type_id];
        for (BinPos copy = 0; copy < solution_bin.copies; ++copy) {
            BinPos new_bin_pos = solution_builder.add_bin(original_bin_type_id, 1);
            single_bin_pos = new_bin_pos;
            for (const SolutionItem& solution_item: solution_bin.items) {
                ItemPos reduced_copy_index = next_copy_index[solution_item.item_type_id]++;
                const CopyOrigin& origin
                    = reduced_copy_origins_[solution_item.item_type_id][reduced_copy_index];
                ItemTypeId original_item_type_id = origin.item_type_id;
                const ReductionItemType& item_type = final_item_types_[original_item_type_id];

                std::vector<CompanionItem> no_companions;
                const std::vector<CompanionItem>* companions = &no_companions;
                if (!item_type.companions_by_copy.empty())
                    companions = &item_type.companions_by_copy[origin.copy_index];
                place_item_and_companions(
                        solution_builder,
                        new_bin_pos,
                        original_item_type_id,
                        solution_item.bl_corner,
                        solution_item.rotate,
                        *companions);
            }
        }
    }

    // Place every pinned "full span item" (see 'FullSpanItem'/
    // 'reduce_full_span_items') into the reduced solution's own single
    // bin - unlike 'full_bin_items_'/'perfect_pairs_'/'both_groups_'
    // below, these were never set aside in a *separate* dedicated bin
    // (only ever meaningful with exactly one bin instance to shrink - see
    // 'reduce_full_span_items_applies'), so they share whichever single
    // bin the loop above already built, creating one from scratch only if
    // 'solution' itself placed nothing there at all (e.g. every item type
    // ended up pinned, leaving nothing for the downstream solve to
    // place).
    if (!full_span_items_.empty()) {
        BinPos bin_pos = (single_bin_pos != -1)?
            single_bin_pos: solution_builder.add_bin(0, 1);
        for (const FullSpanItem& full_span_item: full_span_items_) {
            place_item_and_companions(
                    solution_builder,
                    bin_pos,
                    full_span_item.item_type_id,
                    full_span_item.bl_corner,
                    full_span_item.rotate,
                    full_span_item.companions);
        }
    }

    // Reinstate each "full bin item"/"perfect pair"/"both group"
    // reservation as its own dedicated bin(s) (see
    // 'FullBinItem'/'PerfectPair'/'BothGroup'): entirely absent from
    // 'instance_', so nothing above ever encounters them while scanning
    // 'solution'. Only ever populated when 'original_instance_' has a
    // single bin type (see 'reduce_full_bin_items'/'reduce_perfect_pairs'/
    // 'reduce_both_groups'/'try_reduce_both_group'), so bin type id 0
    // always refers to it.
    for (const FullBinItem& full_bin_item: full_bin_items_) {
        for (BinPos copy = 0; copy < full_bin_item.copies; ++copy) {
            BinPos new_bin_pos = solution_builder.add_bin(0, 1);
            place_item_and_companions(
                    solution_builder,
                    new_bin_pos,
                    full_bin_item.item_type_id,
                    Point{0, 0},
                    full_bin_item.rotate,
                    full_bin_item.companions_by_copy[copy]);
        }
    }
    for (const PerfectPair& pair: perfect_pairs_) {
        for (BinPos copy = 0; copy < pair.copies; ++copy) {
            BinPos new_bin_pos = solution_builder.add_bin(0, 1);
            place_item_and_companions(
                    solution_builder,
                    new_bin_pos,
                    pair.item_type_id_1,
                    Point{0, 0},
                    false,
                    pair.item_1_companions_by_copy[copy]);
            place_item_and_companions(
                    solution_builder,
                    new_bin_pos,
                    pair.item_type_id_2,
                    pair.offset_2,
                    false,
                    pair.item_2_companions_by_copy[copy]);
        }
    }
    // Unlike 'full_bin_items_'/'perfect_pairs_', each 'BothGroup' entry is
    // already exactly one dedicated bin (see its own doc comment for why
    // there is no shared 'copies' count to expand), and every item in it
    // already carries its own absolute position and rotation directly
    // from the companion-bin check's own solution - no relative-offset
    // translation needed, since that check's own bin was already
    // 'bin_w'x'bin_h', the same as this dedicated bin.
    for (const BothGroup& group: both_groups_) {
        BinPos new_bin_pos = solution_builder.add_bin(0, 1);
        for (const BothGroup::PlacedItem& placed_item: group.items) {
            place_item_and_companions(
                    solution_builder,
                    new_bin_pos,
                    placed_item.item_type_id,
                    placed_item.bl_corner,
                    placed_item.rotate,
                    placed_item.companions);
        }
    }

    return solution_builder.build();
}

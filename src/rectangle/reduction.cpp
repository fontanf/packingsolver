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
                    && number_of_dedicated_bins() + big_item.copies > bin_type.copies) {
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
            && number_of_dedicated_bins() + total_copies_used > bin_type.copies) {
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
                && number_of_dedicated_bins() + copies > bin_type.copies) {
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
                    && number_of_dedicated_bins() + copies > bin_type.copies) {
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

Instance Reduction::reduction_to_instance(
        const std::vector<ReductionItemType>& reduction_item_types)
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
        if (!reduction_item_types[item_type_id].removed)
            remaining_items += original_instance_->item_type(item_type_id).copies;
    }

    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        BinTypeId new_bin_type_id = instance_builder.add_bin_type(*original_instance_, bin_type_id);
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

    reduced_to_original_item_type_ids_.clear();
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        const ItemType& original_item_type = original_instance_->item_type(item_type_id);
        ItemTypeId new_item_type_id = instance_builder.add_item_type(
                item.rect.x, item.rect.y, original_item_type.oriented);
        instance_builder.set_item_type_profit(new_item_type_id, original_item_type.profit);
        instance_builder.set_item_type_copies(new_item_type_id, item.copies);
        instance_builder.set_item_type_group(new_item_type_id, original_item_type.group_id);
        instance_builder.set_item_type_weight(new_item_type_id, original_item_type.weight);
        instance_builder.set_item_type_eligibility(new_item_type_id, original_item_type.eligibility_id);
        reduced_to_original_item_type_ids_.push_back(item_type_id);
    }

    return instance_builder.build();
}

namespace
{

/**
 * Maximum achievable combination of item widths (or heights) not
 * exceeding 'capacity' - the inner multiple-choice subset-sum
 * maximization of equation (7) - via 'multiplechoicesubsetsumsolver': one
 * group per item copy, containing one candidate value per orientation
 * that copy is allowed to present on this axis (see
 * 'Reduction::compute_shrunk_bin_sizes''s own doc comment for why this is
 * sound for non-oriented items too).
 */
Length max_achievable_dimension_sum(
        const Instance& instance,
        Length capacity,
        bool width_axis)
{
    multiplechoicesubsetsumsolver::InstanceBuilder mcss_instance_builder;
    mcss_instance_builder.set_capacity(capacity);
    multiplechoicesubsetsumsolver::GroupId mcss_group_id = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Length value = (width_axis)? item_type.rect.x: item_type.rect.y;
        Length rotated_value = (width_axis)? item_type.rect.y: item_type.rect.x;
        for (ItemPos copy = 0; copy < item_type.copies; ++copy) {
            mcss_instance_builder.add_item(mcss_group_id, value);
            if (!item_type.oriented && rotated_value != value)
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

}

Reduction::ShrunkBinSizes Reduction::compute_shrunk_bin_sizes(
        const Instance& instance)
{
    ShrunkBinSizes result;

    if (instance.number_of_bin_types() != 1) {
        result.bin_width = 0;
        result.bin_height = 0;
        return result;
    }
    const BinType& bin_type = instance.bin_type(0);
    result.bin_width = bin_type.rect.x;
    result.bin_height = bin_type.rect.y;

    // Infinite item copies can't be flattened into individual groups; the
    // technique isn't meaningful there anyway (an infinite-copies item
    // type can already saturate any capacity on its own), so skip it and
    // return the bin's own, unshrunk dimensions.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (instance.item_type(item_type_id).copies < 0)
            return result;
    }

    // Equation (7): shrink the bin width/height to the largest achievable
    // combination of item widths/heights.
    result.bin_width = max_achievable_dimension_sum(instance, bin_type.rect.x, /* width_axis */ true);
    result.bin_height = max_achievable_dimension_sum(instance, bin_type.rect.y, /* width_axis */ false);

    return result;
}

bool Reduction::applies(
        const Instance& instance,
        const ReductionParameters& parameters)
{
    // Every check this class performs (the wide/tall/both companion-bin
    // feasibility solves in 'try_reduce_group', and the direct
    // dimension-only matches in 'reduce_full_bin_items'/
    // 'reduce_perfect_pairs') reasons purely about geometry: whether a set
    // of item footprints fits together inside an empty rectangle. Three
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
    return (instance.objective() == Objective::BinPacking
                || instance.objective() == Objective::VariableSizedBinPacking
                || instance.objective() == Objective::Feasibility)
            && instance.number_of_bin_types() == 1
            && instance.number_of_defects() == 0
            && instance.unloading_constraint() == UnloadingConstraint::None
            && !instance.weight_matters()
            && !instance.resources_matter()
            && parameters.reduce;
}

Reduction::Reduction(
        const Instance& instance,
        const ReductionParameters& parameters):
    original_instance_(&instance),
    instance_(instance)
{
    // Working representation: a stable 1:1 copy of the original instance's
    // item types (see 'ReductionItemType'). Always built and compacted
    // back via 'reduction_to_instance' at the end (even when the
    // reduction doesn't apply below, in which case it is an identity
    // rebuild): this keeps 'final_item_types_'/
    // 'reduced_to_original_item_type_ids_' always populated, so
    // 'unreduce_solution' never needs a separate no-op code path. Callers
    // that already know 'applies()' is 'false' are better off not
    // constructing a 'Reduction' at all (see 'applies''s own doc comment)
    // - but the constructor still handles that case correctly, both for
    // simplicity (one code path regardless of caller diligence) and
    // because 'applies()' and the constructor could otherwise silently
    // drift out of sync.
    std::vector<ReductionItemType> reduction_item_types(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        reduction_item_types[item_type_id].rect = instance.item_type(item_type_id).rect;
        reduction_item_types[item_type_id].copies = instance.item_type(item_type_id).copies;
    }

    if (!applies(instance, parameters)) {
        instance_ = reduction_to_instance(reduction_item_types);
        return;
    }

    // Shrunk bin dimensions (equation (7), "shrinking the bins": see
    // 'compute_shrunk_bin_sizes'), computed once from the *original*,
    // never-reduced instance's full item set - not per round. This stays
    // sound throughout every round despite items later being removed: the
    // bound proves "no item in the original full set is small enough to
    // extend this achieved sum", and every item ever considered in a later
    // round is a subset of that same original full set, so the bound only
    // ever remains valid (if anything, recomputing it from a shrinking
    // remaining set could tighten it further, but that is a possible
    // future improvement, not a soundness requirement).
    //
    // Used as the effective bin dimensions throughout this whole class
    // ('is_big', 'could_fit', 'companion_bin_dimensions', enlargement
    // targets, 'reduce_full_bin_items', 'reduce_perfect_pairs'): any
    // multi-item combination that fits within the *true* bin capacity is,
    // by definition of 'shrunk_bin_w'/'shrunk_bin_h' as the maximum
    // achievable subset-sum from the full original item set, already
    // bounded by it - so substituting it for the true dimensions never
    // discards a genuinely achievable candidate, and the true bin's
    // remaining margin beyond it is provably unusable by any combination
    // of the actual items.
    ShrunkBinSizes shrunk_bin_sizes = compute_shrunk_bin_sizes(instance);
    Length bin_w = shrunk_bin_sizes.bin_width;
    Length bin_h = shrunk_bin_sizes.bin_height;

    // Outer fixpoint loop: repeat {wide, tall, both, full-bin items,
    // perfect pairs} until a full pass finds nothing new (a later case's
    // success can free up items that make an earlier case worth
    // retrying).
    for (Counter round_number = 0;
            round_number < parameters.maximum_number_of_rounds;
            ++round_number) {
        if (parameters.timer.needs_to_end())
            break;
        bool found = false;
        found |= reduce_group(reduction_item_types, parameters, EnlargementCase::Wide, bin_w, bin_h);
        if (parameters.timer.needs_to_end())
            break;
        found |= reduce_group(reduction_item_types, parameters, EnlargementCase::Tall, bin_w, bin_h);
        if (parameters.timer.needs_to_end())
            break;
        found |= reduce_both_groups(reduction_item_types, parameters, bin_w, bin_h);
        if (parameters.timer.needs_to_end())
            break;
        found |= reduce_full_bin_items(
                reduction_item_types, bin_w, bin_h);
        if (parameters.timer.needs_to_end())
            break;
        found |= reduce_perfect_pairs(
                reduction_item_types, bin_w, bin_h);
        if (!found)
            break;
    }

    instance_ = reduction_to_instance(reduction_item_types);
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
    // encountered so far while scanning 'solution' (used to pick the right
    // entry of 'companions_by_copy' - any consistent order works, since
    // companion-bin instances of the same type are interchangeable).
    std::vector<ItemPos> next_copy_index(instance_.number_of_item_types(), 0);

    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        for (BinPos copy = 0; copy < solution_bin.copies; ++copy) {
            BinPos new_bin_pos = solution_builder.add_bin(solution_bin.bin_type_id, 1);
            for (const SolutionItem& solution_item: solution_bin.items) {
                ItemTypeId original_item_type_id =
                    reduced_to_original_item_type_ids_[solution_item.item_type_id];
                const ReductionItemType& item_type = final_item_types_[original_item_type_id];

                std::vector<CompanionItem> no_companions;
                const std::vector<CompanionItem>* companions = &no_companions;
                if (!item_type.companions_by_copy.empty()) {
                    ItemPos copy_index = next_copy_index[solution_item.item_type_id]++;
                    companions = &item_type.companions_by_copy[copy_index];
                }
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

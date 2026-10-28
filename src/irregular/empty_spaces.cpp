#include "irregular/empty_spaces.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

bool packingsolver::irregular::overlaps(
        const EmptySpace& space,
        Point bl_corner,
        LengthDbl bx,
        LengthDbl by)
{
    return shape::strictly_lesser(bl_corner.x, space.xe())
        && shape::strictly_greater(bl_corner.x + bx, space.xs())
        && shape::strictly_lesser(bl_corner.y, space.ye())
        && shape::strictly_greater(bl_corner.y + by, space.ys());
}

void packingsolver::irregular::add_empty_space(
        std::vector<EmptySpace>& spaces,
        const EmptySpace& new_space)
{
    if (!shape::strictly_greater(new_space.bx, 0.0) || !shape::strictly_greater(new_space.by, 0.0))
        return;
    for (const EmptySpace& existing: spaces)
        if (existing.contains(new_space))
            return;
    spaces.push_back(new_space);
}

void packingsolver::irregular::cut_empty_spaces(
        std::vector<EmptySpace>& spaces,
        Point bl_corner,
        LengthDbl bx,
        LengthDbl by)
{
    for (ItemPos i = 0; i < (ItemPos)spaces.size(); ) {
        if (!overlaps(spaces[i], bl_corner, bx, by)) {
            ++i;
            continue;
        }
        EmptySpace space = spaces[i];
        spaces[i] = spaces.back();
        spaces.pop_back();

        // Left (x-)
        if (shape::strictly_greater(bl_corner.x, space.xs())) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bx = bl_corner.x - space.bl_corner.x;
            sub.by = space.by;
            add_empty_space(spaces, sub);
        }
        // Right (x+)
        if (shape::strictly_lesser(bl_corner.x + bx, space.xe())) {
            EmptySpace sub;
            sub.bl_corner.x = bl_corner.x + bx;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bx = space.xe() - (bl_corner.x + bx);
            sub.by = space.by;
            add_empty_space(spaces, sub);
        }
        // Bottom (y-)
        if (shape::strictly_greater(bl_corner.y, space.ys())) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bx = space.bx;
            sub.by = bl_corner.y - space.bl_corner.y;
            add_empty_space(spaces, sub);
        }
        // Top (y+)
        if (shape::strictly_lesser(bl_corner.y + by, space.ye())) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = bl_corner.y + by;
            sub.bx = space.bx;
            sub.by = space.ye() - (bl_corner.y + by);
            add_empty_space(spaces, sub);
        }
        // Do not increment i: the element swapped into position i must be checked.
    }
}

void packingsolver::irregular::cut_empty_spaces(
        std::vector<EmptySpace>& spaces,
        const std::vector<AxisAlignedBoundingBox>& rectangles,
        LengthDbl minimum_width,
        LengthDbl minimum_height)
{
    if (rectangles.empty())
        return;
    AxisAlignedBoundingBox rectangles_aabb;
    for (const AxisAlignedBoundingBox& rectangle: rectangles)
        rectangles_aabb = merge(rectangles_aabb, rectangle);

    // Cut each empty space overlapping a rectangle by all the rectangles,
    // on its own: the containment checks are then only made between its own
    // parts, and its final parts are checked against all the empty spaces
    // once.
    std::vector<EmptySpace> new_spaces;
    std::vector<EmptySpace> parts;
    for (ItemPos space_pos = 0; space_pos < (ItemPos)spaces.size(); ) {
        const EmptySpace& space = spaces[space_pos];
        bool cut = false;
        if (overlaps(
                    space,
                    {rectangles_aabb.x_min, rectangles_aabb.y_min},
                    rectangles_aabb.x_max - rectangles_aabb.x_min,
                    rectangles_aabb.y_max - rectangles_aabb.y_min)) {
            for (const AxisAlignedBoundingBox& rectangle: rectangles) {
                if (overlaps(
                            space,
                            {rectangle.x_min, rectangle.y_min},
                            rectangle.x_max - rectangle.x_min,
                            rectangle.y_max - rectangle.y_min)) {
                    cut = true;
                    break;
                }
            }
        }
        if (!cut) {
            ++space_pos;
            continue;
        }
        parts = {space};
        spaces[space_pos] = spaces.back();
        spaces.pop_back();
        for (const AxisAlignedBoundingBox& rectangle: rectangles) {
            cut_empty_spaces(
                    parts,
                    {rectangle.x_min, rectangle.y_min},
                    rectangle.x_max - rectangle.x_min,
                    rectangle.y_max - rectangle.y_min);
        }
        for (const EmptySpace& part: parts) {
            if (shape::strictly_lesser(part.bx, minimum_width)
                    || shape::strictly_lesser(part.by, minimum_height)) {
                continue;
            }
            new_spaces.push_back(part);
        }
    }

    // Add the largest parts first, so that a part is never added before a
    // part containing it.
    std::sort(
            new_spaces.begin(),
            new_spaces.end(),
            [](const EmptySpace& space_1, const EmptySpace& space_2)
            {
                return space_1.area() > space_2.area();
            });
    for (const EmptySpace& new_space: new_spaces)
        add_empty_space(spaces, new_space);
}

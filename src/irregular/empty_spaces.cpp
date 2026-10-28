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

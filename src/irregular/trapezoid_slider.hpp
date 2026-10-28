#pragma once

#include "irregular/trapezoid_set.hpp"

namespace packingsolver
{
namespace irregular
{

/**
 * Position of a trapezoid set whose supported part rests on a supporting
 * part.
 */
struct SlidePosition
{
    /** Current element of the supporting part. */
    ElementPos supporting_part_element_pos = 0;

    /** Current element of the supported part. */
    ElementPos supported_part_element_pos = 0;

    /** x-coordinate of the trapezoid set. */
    LengthDbl x = 0.0;

    /** y-coordinate of the trapezoid set. */
    LengthDbl y = 0.0;
};

/**
 * Slide a trapezoid, whose supported part rests on a supporting part, along
 * the supporting part, to the right, following the slopes of both parts,
 * until it doesn't intersect 'trapezoid_to_avoid' anymore.
 *
 * 'current_trapezoid' is the trapezoid at 'position'; it is moved with it.
 *
 * Returns 'true' iff the position has been updated. If the end of both parts
 * has been reached, both element positions of 'position' are equal to the
 * number of elements of their part.
 */
bool slide_trapezoid(
        SlidePosition& position,
        const Shape& supporting_shape,
        const Shape& supported_shape,
        const GeneralizedTrapezoid& trapezoid_to_avoid,
        GeneralizedTrapezoid& current_trapezoid);

/** Trapezoid of an obstacle. */
inline const GeneralizedTrapezoid& obstacle_trapezoid(
        const GeneralizedTrapezoid& trapezoid)
{
    return trapezoid;
}

/** Trapezoid of an obstacle storing it in a 'trapezoid' attribute. */
template <typename Obstacle>
inline const GeneralizedTrapezoid& obstacle_trapezoid(
        const Obstacle& obstacle)
{
    return obstacle.trapezoid;
}

/**
 * Class to slide a trapezoid set, whose supported part rests on a supporting
 * part, along the supporting part, to the right, until it doesn't overlap a
 * set of obstacles.
 *
 * Each move is the smallest one getting a trapezoid of the set out of an
 * obstacle, so the final position is the first position along the supporting
 * part, at or after the initial one, without overlap, whatever the order in
 * which the obstacles are considered.
 *
 * The slide fails if the end of the supporting or of the supported part is
 * reached, or if the trapezoid set goes beyond 'x_max'.
 */
class TrapezoidSetSlider
{

public:

    /**
     * Constructor.
     *
     * 'supporting_shape' must be given at its actual position.
     */
    TrapezoidSetSlider(
            const TrapezoidSet& trapezoid_set,
            const Shape& supporting_shape,
            const Shape& supported_shape,
            LengthDbl x_max):
        trapezoid_set_(trapezoid_set),
        supporting_shape_(supporting_shape),
        supported_shape_(supported_shape),
        x_max_(x_max) { }

    /**
     * Initial position: the first point of the supported part on the first
     * point of the supporting part.
     */
    SlidePosition initial_position() const
    {
        SlidePosition position;
        position.x = supporting_shape_.elements.front().start.x - supported_shape_.elements.front().start.x;
        position.y = supporting_shape_.elements.front().start.y - supported_shape_.elements.front().start.y;
        return position;
    }

    /** Return 'true' iff the end of both parts has been reached. */
    inline bool end_of_support(const SlidePosition& position) const
    {
        return position.supporting_part_element_pos == supporting_shape_.elements.size()
            && position.supported_part_element_pos == supported_shape_.elements.size();
    }

    /** Return 'true' iff the trapezoid set goes beyond 'x_max'. */
    inline bool out_of_bounds(const SlidePosition& position) const
    {
        return strictly_greater(position.x + trapezoid_set_.x_max, x_max_);
    }

    /**
     * Slide the trapezoid set until it doesn't overlap the obstacles.
     *
     * Return 'false' if the slide failed.
     */
    template <typename Obstacle>
    bool slide(
            SlidePosition& position,
            const std::vector<Obstacle>& obstacles) const
    {
        for (;;) {
            bool moved = false;
            if (!pass(position, obstacles, moved))
                return false;
            if (!moved)
                return true;
        }
    }

    /**
     * Slide the trapezoid set until it overlaps neither the obstacles of
     * the first group nor the ones of the second group.
     *
     * Return 'false' if the slide failed.
     */
    template <typename Obstacle1, typename Obstacle2>
    bool slide(
            SlidePosition& position,
            const std::vector<Obstacle1>& obstacles_1,
            const std::vector<Obstacle2>& obstacles_2) const
    {
        for (;;) {
            bool moved = false;
            if (!pass(position, obstacles_1, moved))
                return false;
            if (!pass(position, obstacles_2, moved))
                return false;
            if (!moved)
                return true;
        }
    }

    /**
     * Slide the trapezoid set until it is to the right of the vertical line
     * of abscissa 'x_wall'.
     *
     * 'y_min' and 'y_max' are the bounds of the bin; 'x_far' is an abscissa
     * far enough to the left of the bin.
     *
     * Return 'false' if the slide failed.
     */
    bool slide_right_of(
            SlidePosition& position,
            LengthDbl x_wall,
            LengthDbl x_far,
            LengthDbl y_min,
            LengthDbl y_max) const;

private:

    /** Trapezoid set. */
    const TrapezoidSet& trapezoid_set_;

    /** Supporting shape. */
    const Shape& supporting_shape_;

    /** Supported shape. */
    const Shape& supported_shape_;

    /** Maximum x-coordinate of the trapezoid set. */
    LengthDbl x_max_;

    /**
     * Move each trapezoid of the trapezoid set out of each obstacle, once.
     *
     * 'moved' is set to 'true' if the trapezoid set moved.
     *
     * Return 'false' if the slide failed.
     */
    template <typename Obstacle>
    bool pass(
            SlidePosition& position,
            const std::vector<Obstacle>& obstacles,
            bool& moved) const
    {
        for (const auto& item_shape_trapezoids: trapezoid_set_.shapes) {
            for (const GeneralizedTrapezoid& item_shape_trapezoid: item_shape_trapezoids) {
                GeneralizedTrapezoid current_trapezoid = item_shape_trapezoid;
                current_trapezoid.shift_right(position.x);
                current_trapezoid.shift_top(position.y);
                for (const Obstacle& obstacle: obstacles) {
                    bool updated = slide_trapezoid(
                            position,
                            supporting_shape_,
                            supported_shape_,
                            obstacle_trapezoid(obstacle),
                            current_trapezoid);
                    if (end_of_support(position))
                        return false;
                    if (updated) {
                        moved = true;
                        if (out_of_bounds(position))
                            return false;
                    }
                }
            }
        }
        return true;
    }

};

}
}

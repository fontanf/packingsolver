#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

using TrapezoidPos = int64_t;

/**
 * Class for a generalized trapezoid which parallel edges are parallel to the
 * x-axis.
 */
class GeneralizedTrapezoid
{

public:

    /** Standard constructor. */
    GeneralizedTrapezoid(
            LengthDbl yb,
            LengthDbl yt,
            LengthDbl xbl,
            LengthDbl xbr,
            LengthDbl xtl,
            LengthDbl xtr):
        yb_(yb),
        yt_(yt),
        xbl_(xbl),
        xbr_(xbr),
        xtl_(xtl),
        xtr_(xtr)
    {
        // Checks.
        if (yb_ >= yt_) {
            throw std::logic_error(
                    "GeneralizedTrapezoid::GeneralizedTrapezoid."
                    "yb: " + std::to_string(yb)
                    + "; yt: " + std::to_string(yt) + ".");
        }
        if (striclty_greater(xbl_, xbr_)) {
            throw std::logic_error(
                    "GeneralizedTrapezoid::GeneralizedTrapezoid."
                    "xbl: " + std::to_string(xbl)
                    + "; xbr: " + std::to_string(xbr) + ".");
        }
        if (striclty_greater(xtl_, xtr_)) {
            throw std::logic_error(
                    "GeneralizedTrapezoid::GeneralizedTrapezoid."
                    "xtl: " + std::to_string(xtl)
                    + "; xtr: " + std::to_string(xtr) + ".");
        }

        width_top_ = x_top_right() - x_top_left();
        width_bottom_ = x_bottom_right() - x_bottom_left();
        height_ = y_top() - y_bottom();
        area_ = (width_top() + width_bottom()) * height() / 2.0;
        a_left_ = (x_top_left() - x_bottom_left()) / (y_top() - y_bottom());
        a_right_ = (x_top_right() - x_bottom_right()) / (y_top() - y_bottom());
        x_min_ = std::min(x_bottom_left(), x_top_left());
        x_max_ = std::max(x_bottom_right(), x_top_right());
    }

    void shift_top(LengthDbl l)
    {
        yb_ += l;
        yt_ += l;
    }

    void shift_right(LengthDbl l)
    {
        xbl_ += l;
        xbr_ += l;
        xtl_ += l;
        xtr_ += l;
        x_min_ += l;
        x_max_ += l;
    }

    bool operator==(const GeneralizedTrapezoid& trapezoid) const
    {
        return ((y_bottom() == trapezoid.y_bottom())
                && (y_top() == trapezoid.y_top())
                && (x_bottom_left() == trapezoid.x_bottom_left())
                && (x_bottom_right() == trapezoid.x_bottom_right())
                && (x_top_left() == trapezoid.x_top_left())
                && (x_top_right() == trapezoid.x_top_right())
               );
    }

    /*
     * Getters
     */

    /** Get the y-coordinate of the bottom side of the generalized trapezoid. */
    inline LengthDbl y_bottom() const { return yb_; }

    /** Get the y-coordinate of the top side of the generalized trapezoid. */
    inline LengthDbl y_top() const { return yt_; }

    /** Get the x-coordinate of the bottom-left corner of the trapezoid. */
    inline LengthDbl x_bottom_left() const { return xbl_; }

    /** Get the x-coordinate of the bottom-right corner of the trapezoid. */
    inline LengthDbl x_bottom_right() const { return xbr_; }

    /** Get the x-coordinate of the top-left corner of the trapezoid. */
    inline LengthDbl x_top_left() const { return xtl_; }

    /** Get the x-coordinate of the top-right corner of the trapezoid. */
    inline LengthDbl x_top_right() const { return xtr_; }

    /** Get the height of the generalized trapezoid. */
    inline LengthDbl height() const { return height_; }

    /** Get the bottom width of the generalized trapezoid. */
    inline LengthDbl width_bottom() const { return width_bottom_; }

    /** Get the top width of the generalized trapezoid. */
    inline LengthDbl width_top() const { return width_top_; }

    /** Get the greatest x of the generalized trapezoid. */
    inline LengthDbl x_max() const { return x_max_; }

    /** Get the smallest x of the generalized trapezoid. */
    inline LengthDbl x_min() const { return x_min_; }

    /** Get the area of the generalized trapezoid. */
    inline AreaDbl area() const { return area_; }


    /** Get the x-coordinate of a point on the left side of the trapezoid. */
    inline LengthDbl x_left(LengthDbl y) const
    {
        if (y == y_bottom())
            return x_bottom_left();
        if (y == y_top())
            return x_top_left();
        if (x_bottom_left() == x_top_left())
            return x_bottom_left();
        return x_bottom_left() + (y - y_bottom()) * a_left_;
    }

    /** Get the x-coordinate of a point on the right side of the trapezoid. */
    inline LengthDbl x_right(LengthDbl y) const
    {
        if (y == y_bottom())
            return x_bottom_right();
        if (y == y_top())
            return x_top_right();
        if (x_bottom_right() == x_top_right())
            return x_bottom_right();
        return x_bottom_right() + (y - y_bottom()) * a_right_;
    }

    AreaDbl area(LengthDbl x_left) const
    {
        if (x_left > x_bottom_right()) {
            if (x_left > x_top_right()) {
                throw std::invalid_argument(
                        "GeneralizedTrapezoid::area");
            }
            double k = (x_top_right() - x_bottom_right()) / (x_top_right() - x_left);
            return (x_top_right() - x_bottom_right()) * height() / 2.0 / k / k;
        } else if (x_left > x_top_right()) {
            double k = (x_bottom_right() - x_top_right()) / (x_bottom_right() - x_left);
            return (x_bottom_right() - x_top_right()) * height() / 2.0 / k / k;
        }
        LengthDbl width_top = x_top_right() - x_left;
        LengthDbl width_bottom = x_bottom_right() - x_left;
        return (width_top + width_bottom) * height() / 2.0;
    }

    /** Check of the generalized trapezoid intersects another generalized trapezoid. */
    inline bool intersect(
            const GeneralizedTrapezoid& trapezoid) const
    {
        if (y_bottom() >= trapezoid.y_top())
            return false;
        if (y_top() <= trapezoid.y_bottom())
            return false;

        Length yb = std::max(y_bottom(), trapezoid.y_bottom());
        Length yt = std::min(y_top(), trapezoid.y_top());

        Length x1br = x_right(yb);
        Length x1tr = x_right(yt);
        Length x2bl = trapezoid.x_left(yb);
        Length x2tl = trapezoid.x_left(yt);
        if (x1br <= x2bl && x1tr <= x2tl)
            return false;

        Length x1bl = x_left(yb);
        Length x1tl = x_left(yt);
        Length x2br = trapezoid.x_right(yb);
        Length x2tr = trapezoid.x_right(yt);
        if (x1bl >= x2br && x1tl >= x2tr)
            return false;

        return true;
    }

    /**
     * Compute by how much the generalized trapezoid must be shifted to be right
     * next to another generalized trapezoid.
     */
    inline LengthDbl compute_right_shift(
            const GeneralizedTrapezoid& trapezoid) const
    {
        if (y_bottom() >= trapezoid.y_top())
            return 0.0;
        if (y_top() <= trapezoid.y_bottom())
            return 0.0;

        Length yb = std::max(y_bottom(), trapezoid.y_bottom());
        Length yt = std::min(y_top(), trapezoid.y_top());
        //std::cout << "yb " << yb << " yt " << yt << std::endl;

        Length x1bl = x_left(yb);
        Length x1tl = x_left(yt);
        Length x2br = trapezoid.x_right(yb);
        Length x2tr = trapezoid.x_right(yt);
        //std::cout << "x1bl " << x1bl << " x1tl " << x1tl << std::endl;
        //std::cout << "x2br " << x2br << " x2tr " << x2tr << std::endl;
        if (x1bl >= x2br && x1tl >= x2tr)
            return 0.0;

        return std::max(x2br - x1bl, x2tr - x1tl);
    }

    /**
     * Compute by how much the generalized trapezoid must be shifted to be right
     * next to another generalized trapezoid, only if they already intersect.
     */
    inline LengthDbl compute_right_shift_if_intersects(
            const GeneralizedTrapezoid& trapezoid) const
    {
        if (y_bottom() >= trapezoid.y_top())
            return 0.0;
        if (y_top() <= trapezoid.y_bottom())
            return 0.0;

        Length yb = std::max(y_bottom(), trapezoid.y_bottom());
        Length yt = std::min(y_top(), trapezoid.y_top());

        Length x1br = x_right(yb);
        Length x1tr = x_right(yt);
        Length x2bl = trapezoid.x_left(yb);
        Length x2tl = trapezoid.x_left(yt);
        if (x1br <= x2bl && x1tr <= x2tl)
            return 0.0;

        Length x1bl = x_left(yb);
        Length x1tl = x_left(yt);
        Length x2br = trapezoid.x_right(yb);
        Length x2tr = trapezoid.x_right(yt);
        if (x1bl >= x2br && x1tl >= x2tr)
            return 0.0;

        return std::max(x2br - x1bl, x2tr - x1tl);
    }

private:

    /** x-coordinate of the bottom. */
    LengthDbl yb_;

    /** y-coordinate of the top. */
    LengthDbl yt_;

    /** x-coordinate of the bottom-left point. */
    LengthDbl xbl_;

    /** x-coordinate of the bottom-right point. */
    LengthDbl xbr_;

    /** x-coordinate of the top-left point. */
    LengthDbl xtl_;

    /** x-coordinate of the top-right point. */
    LengthDbl xtr_;

    /*
     * Computed attributes
     */

    /** height of the generalized trapezoid. */
    LengthDbl height_;

    /** Bottom width of the generalized trapezoid. */
    LengthDbl width_bottom_;

    /** Bottom height of the generalized trapezoid. */
    LengthDbl width_top_;

    /** Smallest x of the generalized trapezoid. */
    LengthDbl x_min_;

    /** Greatest x of the generalized trapezoid. */
    LengthDbl x_max_;

    /** Area of the generalized trapezoid. */
    AreaDbl area_;

    /** Left slope. */
    double a_left_;

    /** Right slope. */
    double a_right_;

};

inline std::ostream& operator<<(
        std::ostream& os,
        const GeneralizedTrapezoid& trapezoid)
{
    os << "yb " << trapezoid.y_bottom()
        << " yt " << trapezoid.y_top()
        << " xbl " << trapezoid.x_bottom_left()
        << " xbr " << trapezoid.x_bottom_right()
        << " xtl " << trapezoid.x_top_left()
        << " xtr " << trapezoid.x_top_right()
        ;
    return os;
}

}
}

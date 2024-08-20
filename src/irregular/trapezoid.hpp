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
    inline GeneralizedTrapezoid(
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

        left_side_increasing_not_vertical_ = (a_left() > 0);
        left_side_decreasing_not_vertical_ = (a_left() < -0);
        right_side_increasing_not_vertical_ = (a_right() > 0);
        right_side_decreasing_not_vertical_ = (a_right() < -0);
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

    void extend_left(LengthDbl l)
    {
        xbl_ = l;
        xtl_ = l;
        x_min_ = l;
        left_side_decreasing_not_vertical_ = false;
        left_side_increasing_not_vertical_ = false;
        a_left_ = 0;
    }

    void set_top_covered(bool bottom_covered) { bottom_covered_ = bottom_covered; }

    void set_bottom_covered(bool bottom_covered) { bottom_covered_ = bottom_covered; }

    bool operator==(const GeneralizedTrapezoid& trapezoid) const
    {
        return (y_bottom() == trapezoid.y_bottom()
                && y_top() == trapezoid.y_top()
                && equal(x_bottom_left(), trapezoid.x_bottom_left())
                && equal(x_bottom_right(), trapezoid.x_bottom_right())
                && equal(x_top_left(), trapezoid.x_top_left())
                && equal(x_top_right(), trapezoid.x_top_right()));
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

    inline double a_left() const { return a_left_; }

    inline double a_right() const { return a_right_; }

    inline bool left_side_increasing_not_vertical() const { return left_side_increasing_not_vertical_; }

    inline bool left_side_decreasing_not_vertical() const { return left_side_decreasing_not_vertical_; }

    inline bool right_side_increasing_not_vertical() const { return right_side_increasing_not_vertical_; }

    inline bool right_side_decreasing_not_vertical() const { return right_side_decreasing_not_vertical_; }

    inline bool top_covered() const { return top_covered_; }

    inline bool bottom_covered() const { return bottom_covered_; }


    /** Get the x-coordinate of a point on the left side of the trapezoid. */
    inline LengthDbl x_left(LengthDbl y) const
    {
        if (equal(y, y_bottom()))
            return x_bottom_left();
        if (equal(y, y_top()))
            return x_top_left();
        if (equal(x_bottom_left(), x_top_left()))
            return x_bottom_left();
        return x_bottom_left() + (y - y_bottom()) * a_left_;
    }

    /** Get the x-coordinate of a point on the right side of the trapezoid. */
    inline LengthDbl x_right(LengthDbl y) const
    {
        if (equal(y, y_bottom()))
            return x_bottom_right();
        if (equal(y, y_top()))
            return x_top_right();
        if (equal(x_bottom_right(), x_top_right()))
            return x_bottom_right();
        return x_bottom_right() + (y - y_bottom()) * a_right_;
    }

    AreaDbl area(LengthDbl x_left) const
    {
        if (striclty_greater(x_left, x_bottom_right())) {
            if (striclty_greater(x_left, x_top_right())) {
                throw std::invalid_argument(
                        "GeneralizedTrapezoid::area(LengthDbl)."
                        " x_left: " + std::to_string(x_left)
                        + "; x_top_right: " + std::to_string(x_top_right())
                        + "; x_bottom_right: " + std::to_string(x_bottom_right())
                        + ".");
            }
            double k = (x_top_right() - x_bottom_right()) / (x_top_right() - x_left);
            return (x_top_right() - x_bottom_right()) * height() / 2.0 / k / k;
        } else if (striclty_greater(x_left, x_top_right())) {
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
        if (!striclty_lesser(y_bottom(), trapezoid.y_top()))
            return false;
        if (!striclty_greater(y_top(), trapezoid.y_bottom()))
            return false;

        LengthDbl yb = std::max(y_bottom(), trapezoid.y_bottom());
        LengthDbl yt = std::min(y_top(), trapezoid.y_top());

        LengthDbl x1br = x_right(yb);
        LengthDbl x1tr = x_right(yt);
        LengthDbl x2bl = trapezoid.x_left(yb);
        LengthDbl x2tl = trapezoid.x_left(yt);
        if (!striclty_greater(x1br, x2bl)
                && !striclty_greater(x1tr, x2tl))
            return false;

        LengthDbl x1bl = x_left(yb);
        LengthDbl x1tl = x_left(yt);
        LengthDbl x2br = trapezoid.x_right(yb);
        LengthDbl x2tr = trapezoid.x_right(yt);
        if (!striclty_lesser(x1bl, x2br)
                && !striclty_lesser(x1tl, x2tr))
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
        if (!striclty_lesser(y_bottom(), trapezoid.y_top()))
            return 0.0;
        if (!striclty_greater(y_top(), trapezoid.y_bottom()))
            return 0.0;

        LengthDbl yb = std::max(y_bottom(), trapezoid.y_bottom());
        LengthDbl yt = std::min(y_top(), trapezoid.y_top());
        //std::cout << "yb " << yb << " yt " << yt << std::endl;

        LengthDbl x1bl = x_left(yb);
        LengthDbl x1tl = x_left(yt);
        LengthDbl x2br = trapezoid.x_right(yb);
        LengthDbl x2tr = trapezoid.x_right(yt);
        //std::cout << "x1bl " << x1bl << " x1tl " << x1tl << std::endl;
        //std::cout << "x2br " << x2br << " x2tr " << x2tr << std::endl;
        if (!striclty_lesser(x1bl, x2br)
                && !striclty_lesser(x1tl, x2tr))
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
        if (!striclty_lesser(y_bottom(), trapezoid.y_top()))
            return 0.0;
        if (!striclty_greater(y_top(), trapezoid.y_bottom()))
            return 0.0;

        LengthDbl yb = std::max(y_bottom(), trapezoid.y_bottom());
        LengthDbl yt = std::min(y_top(), trapezoid.y_top());

        LengthDbl x1br = x_right(yb);
        LengthDbl x1tr = x_right(yt);
        LengthDbl x2bl = trapezoid.x_left(yb);
        LengthDbl x2tl = trapezoid.x_left(yt);
        if (!striclty_greater(x1br, x2bl)
                && !striclty_greater(x1tr, x2tl))
            return 0.0;

        LengthDbl x1bl = x_left(yb);
        LengthDbl x1tl = x_left(yt);
        LengthDbl x2br = trapezoid.x_right(yb);
        LengthDbl x2tr = trapezoid.x_right(yt);
        if (!striclty_lesser(x1bl, x2br)
                && !striclty_lesser(x1tl, x2tr))
            return 0.0;

        return std::max(x2br - x1bl, x2tr - x1tl);
    }

    inline LengthDbl compute_top_right_shift(
            const GeneralizedTrapezoid& trapezoid,
            double a) const
    {
        LengthDbl x_shift = 0.0;

        for (const Point& p: std::array<Point, 4>{
                Point{x_bottom_left(), y_bottom()},
                Point{x_bottom_right(), y_bottom()},
                Point{x_top_left(), y_top()},
                Point{x_top_right(), y_top()}}) {
            //std::cout << "p1 " << p.to_string() << std::endl;
            LengthDbl b = p.y - p.x * a;

            // Bottom side of second trapezoid.
            {
                LengthDbl x = (trapezoid.y_bottom() - b) / a;
                //std::cout << "bottom:"
                //    << " x " << x
                //    << " x_shift " << x - p.x
                //    << " y_shift " << trapezoid.y_bottom() - p.y
                //    << std::endl;
                if (striclty_greater(x, p.x)
                        && !striclty_lesser(x, trapezoid.x_bottom_left())
                        && !striclty_greater(x, trapezoid.x_bottom_right())) {
                    x_shift = std::max(x_shift, x - p.x);
                }
            }

            // Top side of second trapezoid.
            {
                LengthDbl x = (trapezoid.y_top() - b) / a;
                //std::cout << "top:"
                //    << " x " << x
                //    << " x_shift " << x - p.x
                //    << " y_shift " << trapezoid.y_top() - p.y
                //    << std::endl;
                if (striclty_greater(x, p.x)
                        && !striclty_lesser(x, trapezoid.x_top_left())
                        && !striclty_greater(x, trapezoid.x_top_right())) {
                    x_shift = std::max(x_shift, x - p.x);
                }
            }

            // Left side of second trapezoid.
            {
                double a_left = 1.0 / trapezoid.a_left_;
                LengthDbl b_left = trapezoid.y_bottom() - a_left * trapezoid.x_bottom_left();
                LengthDbl x = 0;
                LengthDbl y = 0;
                if (trapezoid.a_left() == 0) {
                    x = trapezoid.x_top_left();
                    y = a * x + b;
                } if (!equal(a, a_left)) {
                    x = (b_left - b) / (a - a_left);
                    y = a * x + b;
                } else if (equal(b, b_left)) {
                    x = trapezoid.x_top_left();
                    y = trapezoid.y_top();
                }
                if (striclty_greater(x, p.x)
                        && !striclty_lesser(y, trapezoid.y_bottom())
                        && !striclty_greater(y, trapezoid.y_top())) {
                    //std::cout << "left:"
                    //    << " x_shift " << x - p.x
                    //    << " y_shift " << y - p.y
                    //    << std::endl;
                    x_shift = std::max(x_shift, x - p.x);
                }
            }

            // Right side of second trapezoid.
            {
                double a_right = 1.0 / trapezoid.a_right_;
                LengthDbl b_right = trapezoid.y_bottom() - a_right * trapezoid.x_bottom_right();
                LengthDbl x = 0;
                LengthDbl y = 0;
                if (trapezoid.a_right() == 0) {
                    x = trapezoid.x_top_right();
                    y = a * x + b;
                } else if (!equal(a, a_right)) {
                    x = (b_right - b) / (a - a_right);
                    y = a * x + b;
                } else if (equal(b, b_right)) {
                    x = trapezoid.x_top_right();
                    y = trapezoid.y_top();
                }
                //std::cout << "right:"
                //    << " b " << b
                //    << " b_right " << b_right
                //    << " a " << a
                //    << " a_right " << a_right
                //    << " x " << x
                //    << " y " << y
                //    << " x_shift " << x - p.x
                //    << " y_shift " << y - p.y
                //    << std::endl;
                if (striclty_greater(x, p.x)
                        && !striclty_lesser(y, trapezoid.y_bottom())
                        && !striclty_greater(y, trapezoid.y_top())) {
                    x_shift = std::max(x_shift, x - p.x);
                }
            }
        }

        for (const Point& p: std::array<Point, 4>{
                Point{trapezoid.x_bottom_left(), trapezoid.y_bottom()},
                Point{trapezoid.x_bottom_right(), trapezoid.y_bottom()},
                Point{trapezoid.x_top_left(), trapezoid.y_top()},
                Point{trapezoid.x_top_right(), trapezoid.y_top()}}) {
            //std::cout << "p2 " << p.to_string() << std::endl;
            LengthDbl b = p.y - p.x * a;

            // Bottom side of second trapezoid.
            {
                LengthDbl x = (y_bottom() - b) / a;
                //std::cout << "bottom:"
                //    << " x " << x
                //    << " x_shift " << p.x - x
                //    << " y_shift " << p.y - y_bottom()
                //    << std::endl;
                if (striclty_lesser(x, p.x)
                        && !striclty_lesser(x, x_bottom_left())
                        && !striclty_greater(x, x_bottom_right())) {
                    x_shift = std::max(x_shift, p.x - x);
                }
            }

            // Top side of second trapezoid.
            {
                LengthDbl x = (y_top() - b) / a;
                //std::cout << "top:"
                //    << " x " << x
                //    << " x_shift " << p.x - x
                //    << " y_shift " << p.y - y_top()
                //    << std::endl;
                if (striclty_lesser(x, p.x)
                        && !striclty_lesser(x, x_top_left())
                        && !striclty_greater(x, x_top_right())) {
                    x_shift = std::max(x_shift, p.x - x);
                }
            }

            // Left side of second trapezoid.
            {
                double a_left = 1.0 / a_left_;
                LengthDbl b_left = y_bottom() - a_left * x_bottom_left();
                LengthDbl x = 0;
                LengthDbl y = 0;
                if (a_left_ == 0) {
                    x = x_top_left();
                    y = a * x + b;
                } else if (!equal(a, a_left)) {
                    x = (b_left - b) / (a - a_left);
                    y = a * x + b;
                } else if (equal(b, b_left)) {
                    x = x_top_left();
                    y = y_top();
                }
                //std::cout << "left:"
                //    << " x " << x
                //    << " x_shift " << p.x - x
                //    << " y_shift " << p.y - y
                //    << std::endl;
                if (striclty_lesser(x, p.x)
                        && !striclty_lesser(y, y_bottom())
                        && !striclty_greater(y, y_top())) {
                    x_shift = std::max(x_shift, p.x - x);
                }
            }

            // Right side of second trapezoid.
            {
                double a_right = 1.0 / a_right_;
                LengthDbl b_right = y_bottom() - a_right * x_bottom_right();
                LengthDbl x = 0;
                LengthDbl y = 0;
                if (a_right_ == 0) {
                    x = x_top_right();
                    y = a * x + b;
                } else if (!equal(a, a_right)) {
                    x = (b_right - b) / (a - a_right);
                    y = a * x + b;
                } else if (equal(b, b_right)) {
                    x = x_top_right();
                    y = y_top();
                }
                //std::cout << "right:"
                //    << " x " << x
                //    << " x_shift " << p.x - x
                //    << " y_shift " << p.y - y
                //    << std::endl;
                if (striclty_lesser(x, p.x)
                        && !striclty_lesser(y, y_bottom())
                        && !striclty_greater(y, y_top())) {
                    x_shift = std::max(x_shift, p.x - x);
                }
            }
        }

        return x_shift;
    }

    GeneralizedTrapezoid clean() const
    {
        LengthDbl yb = yb_;
        LengthDbl yt = yt_;
        LengthDbl xbl = xbl_;
        LengthDbl xbr = xbr_;
        LengthDbl xtl = xtl_;
        LengthDbl xtr = xtr_;
        if (a_left() > 1e2) {
            xtl = xbl_;
        } else if (a_left() < -1e2) {
            xbl = xtl_;
        } else if (a_left() > 0 && a_left() < 1e-2) {
            xtl = xbl_;
        } else if (a_left() < 0 && a_left() > -1e-2) {
            xbl = xtl_;
        }
        if (a_right() > 1e2) {
            xbr = xtr_;
        } else if (a_right() < -1e2) {
            xtr = xbr_;
        } else if (a_right() > 0 && a_right() < 1e-2) {
            xbr = xtr_;
        } else if (a_right() < 0 && a_right() > -1e-2) {
            xtr = xbr_;
        }
        return GeneralizedTrapezoid(
                yb,
                yt,
                xbl,
                xbr,
                xtl,
                xtr);
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

    bool left_side_increasing_not_vertical_ = false;

    bool left_side_decreasing_not_vertical_ = false;

    bool right_side_increasing_not_vertical_ = false;

    bool right_side_decreasing_not_vertical_ = false;

    bool bottom_covered_ = false;

    bool top_covered_ = false;

};

inline std::ostream& operator<<(
        std::ostream& os,
        const GeneralizedTrapezoid& trapezoid)
{
    std::streamsize precision = std::cout.precision();
    os << std::setprecision(std::numeric_limits<double>::digits10 + 1)
        << "yb " << trapezoid.y_bottom()
        << " yt " << trapezoid.y_top()
        << " xbl " << trapezoid.x_bottom_left()
        << " xbr " << trapezoid.x_bottom_right()
        << " xtl " << trapezoid.x_top_left()
        << " xtr " << trapezoid.x_top_right()
        << std::setprecision(precision)
        ;
    return os;
}

}
}

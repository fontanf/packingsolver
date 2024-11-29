#pragma once

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{
namespace irregular
{

using LengthDbl = double;
using AreaDbl = double;
using ElementPos = int64_t;
using ItemShapePos = int64_t;
using ShapePos = int64_t;

/**
 * Structure for a point.
 */
struct Point
{
    /** x-coordinate. */
    LengthDbl x;

    /** y-coordiante. */
    LengthDbl y;

    /*
     * Transformations
     */

    Point& shift(
            LengthDbl x,
            LengthDbl y);

    Point rotate(Angle angle) const;

    Point axial_symmetry_identity_line() const;

    Point axial_symmetry_y_axis() const;

    Point axial_symmetry_x_axis() const;

    /*
     * Export
     */

    std::string to_string() const;

    /*
     * Others
     */

    bool operator==(const Point& point) const { return x == point.x && y == point.y; }
};

Point operator+(
        const Point& point_1,
        const Point& point_2);

Point operator-(
        const Point& point_1,
        const Point& point_2);

LengthDbl norm(
        const Point& vector);

LengthDbl distance(
        const Point& point_1,
        const Point& point_2);

LengthDbl dot_product(
        const Point& vector_1,
        const Point& vector_2);

LengthDbl cross_product(
        const Point& vector_1,
        const Point& vector_2);

Angle angle_radian(
        const Point& vector);

/**
 * Return the angle between two vectors.
 *
 * The angle is measured anticlockwise and always belongs to [0, 2 * pi[.
 */
Angle angle_radian(
        const Point& vector_1,
        const Point& vector_2);

enum class ShapeElementType
{
    LineSegment,
    CircularArc,
};

std::string element2str(ShapeElementType type);
ShapeElementType str2element(const std::string& str);

char element2char(ShapeElementType type);

/**
 * Structure for the elementary elements composing a shape.
 */
struct ShapeElement
{
    /** Type of element. */
    ShapeElementType type;

    /** Start point of the element. */
    Point start;

    /** End point of the element. */
    Point end;

    /** If the element is a CircularArc, center of the circle. */
    Point center = {0, 0};

    /** If the element is a CircularArc, direction of the rotation. */
    bool anticlockwise = true;

    /** Length of the element. */
    LengthDbl length() const;

    ShapeElement rotate(Angle angle) const;

    ShapeElement axial_symmetry_identity_line() const;

    ShapeElement axial_symmetry_x_axis() const;

    ShapeElement axial_symmetry_y_axis() const;

    std::string to_string() const;
};

enum class ShapeType
{
    Circle,
    Square,
    Rectangle,
    Polygon,
    MultiPolygon,
    PolygonWithHoles,
    MultiPolygonWithHoles,
    GeneralShape,
};

std::string shape2str(ShapeType type);

/**
 * Structure for a shape.
 *
 * A shape is connected and provided in anticlockwise direction.
 */
struct Shape
{
    /**
     * List of elements.
     *
     * The end point of an element must be the start point of the next element.
     */
    std::vector<ShapeElement> elements;

    /** Return true iff the shape is a circle. */
    bool is_circle() const;

    /** Return true iff the shape is a square. */
    bool is_square() const;

    /** Return true iff the shape is a rectangle. */
    bool is_rectangle() const;

    /** Return true iff the shape is a polygon. */
    bool is_polygon() const;

    /** Compute the area of the shape. */
    AreaDbl compute_area() const;

    /** Compute the smallest and greatest x and y of the shape. */
    std::pair<Point, Point> compute_min_max(
            Angle angle = 0.0,
            bool mirror = false) const;

    /** Compute the width and length of the shape. */
    std::pair<LengthDbl, LengthDbl> compute_width_and_length(
            Angle angle = 0.0,
            bool mirror = false) const;

    /* Check if the shape is connected and in anticlockwise direction. */
    bool check() const;

    Shape& shift(
            LengthDbl x,
            LengthDbl y);

    Shape rotate(Angle angle) const;

    Shape axial_symmetry_identity_line() const;

    Shape axial_symmetry_y_axis() const;

    Shape axial_symmetry_x_axis() const;

    Shape reverse() const;

    std::string to_string(Counter indentation) const;

    std::string to_svg(double factor) const;

    void write_svg(
            const std::string& file_path) const;
};

Shape build_polygon_shape(const std::vector<Point>& points);

double compute_svg_factor(double width);

std::string to_svg(
        const Shape& shape,
        const std::vector<Shape>& holes,
        double factor,
        const std::string& fill_color = "blue");

void write_svg(
        const Shape& shape,
        const std::vector<Shape>& holes,
        const std::string& file_path);

struct ItemShape
{
    /** Main shape. */
    Shape shape;

    /**
     * Holes.
     *
     * Holes are shapes contained inside the main shape.
     */
    std::vector<Shape> holes;

    /** Quality rule. */
    QualityRule quality_rule;

    std::string to_string(Counter indentation) const;
};

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct Parameters
{
    /**
     * Quality rules.
     *
     * if 'quality_rules_[quality_rule][k] = 0' (resp. '1'), then defect 'k' is
     * not allowed (resp. allowed) for qulity rule rule 'quality_rule'.
     */
    std::vector<std::vector<uint8_t>> quality_rules;

    /** Minimum distance between two items. */
    LengthDbl item_item_minimum_spacing = 0.0;

    /** Minimum distance between and item and a bin. */
    LengthDbl item_bin_minimum_spacing = 0.0;
};

/**
 * Defect structure for a problem of type 'irregular'.
 */
struct Defect
{
    /** Shape. */
    Shape shape;

    /** Holes. */
    std::vector<Shape> holes;

    /** Type of the defect. */
    DefectTypeId type;

    std::string to_string(Counter indentation) const;
};

/**
 * Bin type structure for a problem of type 'irregular'.
 */
struct BinType
{
    /** Cost of the bin type. */
    Profit cost;

    /** Maximum number of copies of the bin type. */
    BinPos copies;

    /** Minimum number of copies to use of the bin type. */
    BinPos copies_min;

    /** Shape of the bin type. */
    Shape shape;

    /** Defects of the bin type. */
    std::vector<Defect> defects;

    /*
     * Computed attributes.
     */

    /** Area of the bin type. */
    AreaDbl area = 0.0;

    /** Minimum x of the item type. */
    LengthDbl x_min;

    /** Maximum x of the item type. */
    LengthDbl x_max;

    /** Minimum y of the item type. */
    LengthDbl y_min;

    /** Maximum y of the item type. */
    LengthDbl y_max;

    AreaDbl space() const { return area; }

    AreaDbl packable_area(QualityRule quality_rule) const { (void)quality_rule; return 0; } // TODO

    std::string to_string(Counter indentation) const;

    void write_svg(
            const std::string& file_path) const;
};

/**
 * Item type structure for a problem of type 'irregular'.
 */
struct ItemType
{
    /** Profit of the item type. */
    Profit profit;

    /** Number of copies of the item type. */
    ItemPos copies;

    /**
     * Shape of the item type.
     *
     * The shape is composed of multiple non-overlaping sub-shapes which may
     * follow different quality rules.
     */
    std::vector<ItemShape> shapes;

    /** Allowed rotations of the item type. */
    std::vector<std::pair<Angle, Angle>> allowed_rotations = {{0, 0}};

    /** Allow mirroring the item type. */
    bool allow_mirroring = false;

    /*
     * Computed attributes.
     */

    /** Area of the item type. */
    AreaDbl area = 0;

    AreaDbl space() const { return area; }

    /** Return type of shape of the item type. */
    ShapeType shape_type() const;

    std::pair<Point, Point> compute_min_max(
            Angle angle = 0.0,
            bool mirror = false) const;

    bool has_full_continuous_rotations() const;

    bool has_only_discrete_rotations() const;

    std::string to_string(Counter indentation) const;

    void write_svg(
            const std::string& file_path) const;
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Instance ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Instance class for a problem of type "irregular".
 */
class Instance
{

public:

    /*
     * Getters
     */

    /** Get the problem type. */
    static inline ProblemType type() { return ProblemType::Irregular; };

    /** Get the objective of the problem. */
    inline Objective objective() const { return objective_; }

    /*
     * Getters: parameters
     */

    /** Get parameters. */
    const Parameters& parameters() const { return parameters_; }

    /**
     * Return 'true' iff quality_rule 'quality_rule' can contain a defect of
     * type 'type'.
     */
    bool can_contain(QualityRule quality_rule, DefectTypeId type) const;

    /*
     * Getters: bin types
     */

    /** Get the number of bin types. */
    inline BinTypeId number_of_bin_types() const { return bin_types_.size(); }

    /** Get bin type i. */
    inline const BinType& bin_type(BinTypeId bin_type_id) const { return bin_types_[bin_type_id]; }

    /** Get the number of bins. */
    inline BinPos number_of_bins() const { return bin_type_ids_.size(); }

    /** Get the i_pos's bin. */
    inline BinTypeId bin_type_id(BinPos bin_pos) const { return bin_type_ids_[bin_pos]; }

    /** Get the total area of the bins before bin i_pos. */
    inline AreaDbl previous_bin_area(BinPos bin_pos) const { return previous_bins_area_[bin_pos]; }

    /** Get the total area of the bins. */
    inline AreaDbl bin_area() const { return bin_area_; }

    /** Get the maximum cost of the bins. */
    inline Profit maximum_bin_cost() const { return maximum_bin_cost_; }

    /** Get the number of defects. */
    inline DefectId number_of_defects() const { return number_of_defects_; }

    /*
     * Getters: item types
     */

    /* Get the number of item types. */
    inline ItemTypeId number_of_item_types() const { return item_types_.size(); }

    /** Get an item type. */
    inline const ItemType& item_type(ItemTypeId item_type_id) const { return item_types_[item_type_id]; }

    /** Get the number of items. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Get the number rectangular of items. */
    inline ItemPos number_of_rectangular_items() const { return number_of_rectangular_items_; }

    /** Get the number circular of items. */
    inline ItemPos number_of_circular_items() const { return number_of_circular_items_; }

    /** Get the total area of the items. */
    inline AreaDbl item_area() const { return item_area_; }

    /** Get the mean area of the items. */
    inline AreaDbl mean_area() const { return item_area_ / number_of_items(); }

    /** Get the smallest area of the items. */
    inline AreaDbl smallest_item_area() const { return smallest_item_area_; }

    /** Get the largest area of the items. */
    inline AreaDbl largest_item_area() const { return largest_item_area_; }

    /** Get the total profit of the items. */
    inline Profit item_profit() const { return item_profit_; }

    /** Get the id of the item type with maximum efficiency. */
    inline ItemTypeId max_efficiency_item_type_id() const { return max_efficiency_item_type_id_; }

    /** Get the maximum number of copies of the items. */
    inline ItemPos maximum_item_copies() const { return maximum_item_copies_; }

    /** Return true iff all items have infinite copies. */
    inline bool unbounded_knapsack() const { return all_item_types_infinite_copies_; }

    /*
     * Export
     */

    /** Print the instance into a stream. */
    std::ostream& format(
            std::ostream& os,
            int verbosity_level = 1) const;

    /** Write the instance to a file. */
    void write(const std::string& instance_path) const;

private:

    /*
     * Private methods
     */

    /** Create an instance manually. */
    Instance() { }

    /*
     * Private attributes
     */

    /** Objective. */
    Objective objective_;

    /** Parameters. */
    Parameters parameters_;

    /** Bin types. */
    std::vector<BinType> bin_types_;

    /** Item types. */
    std::vector<ItemType> item_types_;

    /*
     * Private attributes computed by the 'build' method
     */

    /** Convert bin position to bin type. */
    std::vector<BinTypeId> bin_type_ids_;

    /** For each bin position, the area of the previous bins. */
    std::vector<AreaDbl> previous_bins_area_;

    /** Total bin area. */
    AreaDbl bin_area_ = 0;

    /** Maximum bin cost. */
    Profit maximum_bin_cost_ = 0.0;

    /** Number of defects. */
    DefectId number_of_defects_ = 0;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Number of rectangular items. */
    ItemPos number_of_rectangular_items_ = 0;

    /** Number of circular items. */
    ItemPos number_of_circular_items_ = 0;

    /** Total item area. */
    AreaDbl item_area_ = 0;

    /** Total item profit. */
    Profit item_profit_ = 0;

    /** Smallest item area. */
    AreaDbl smallest_item_area_ = std::numeric_limits<AreaDbl>::infinity();

    /** Largest item area. */
    AreaDbl largest_item_area_ = 0.0;

    /** Id of the item with maximum efficiency. */
    ItemTypeId max_efficiency_item_type_id_ = -1;

    /** Maximum item copies. */
    ItemPos maximum_item_copies_ = 0;

    /** True iff all item types have an infinite number of copies. */
    bool all_item_types_infinite_copies_ = false;

    friend class InstanceBuilder;

};

}
}

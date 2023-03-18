#pragma once

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{
namespace irregular
{

using LengthDbl = double;
using AreaDbl = double;

/**
 * Structure for a point.
 */
struct Point
{
    /** x-coordinate. */
    LengthDbl x;

    /** y-coordiante. */
    LengthDbl y;

    bool operator==(const Point& point) const { return x == point.x && y == point.y; }

    std::string to_string() const;
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

/**
 * Return the angle between two vectors.
 *
 * The angle is measured anticlockwise and always belongs to [0, 2 * pi[.
 */
Angle angle(
        const Point& vector_1,
        const Point& vector_2);

enum class ShapeElementType
{
    LineSegment,
    CircularArc,
};

std::string element2str(ShapeElementType type);

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

    /** Compute the smallest x of the shape. */
    LengthDbl compute_x_min() const;

    /** Compute the greatest x of the shape. */
    LengthDbl compute_x_max() const;

    /** Compute the smallest y of the shape. */
    LengthDbl compute_y_min() const;

    /** Compute the greatest y of the shape. */
    LengthDbl compute_y_max() const;

    /** Compute the length of the shape. */
    LengthDbl compute_length() const;

    /** Compute the width of the shape. */
    LengthDbl compute_width() const;

    /* Check if the shape is connected and in anticlockwise direction. */
    bool check() const;

    std::string to_string(Counter indentation) const;
};

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

/**
 * Defect structure for a problem of type 'irregular'.
 */
struct Defect
{
    /** Id of the defect. */
    DefectId id;

    /** Shape. */
    Shape shape;

    /** Holes. */
    std::vector<Shape> holes;

    /** Type of the defect. */
    DefectTypeId type;

    std::string to_string(Counter indentation) const;
};

/**
 * Item type structure for a problem of type 'irregular'.
 */
struct ItemType
{
    /** Id of the item type. */
    ItemTypeId id;

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

    /*
     * Computed attributes.
     */

    /** Area of the item type. */
    AreaDbl area = 0;

    /** Minimum x of the item type. */
    LengthDbl x_min;

    /** Maximum x of the item type. */
    LengthDbl x_max;

    /** Minimum y of the item type. */
    LengthDbl y_min;

    /** Maximum y of the item type. */
    LengthDbl y_max;

    AreaDbl space() const { return area; }

    /** Return type of shape of the item type. */
    ShapeType shape_type() const;

    std::string to_string(Counter indentation) const;
};

/**
 * Bin type structure for a problem of type 'irregular'.
 */
struct BinType
{
    /** Id of the bin type. */
    BinTypeId id;

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

    /** Total area of the previous bins. */
    AreaDbl previous_bin_area = 0;

    /** Number of previous bins. */
    BinPos previous_bin_copies = 0;

    AreaDbl space() const { return area; }

    AreaDbl packable_area(QualityRule quality_rule) const { (void)quality_rule; return 0; } // TODO

    std::string to_string(Counter indentation) const;
};

struct Parameters
{
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
     * Constructors and destructor.
     */

    /** Create an instance manually. */
    Instance() { }

    /** Set the objective. */
    void set_objective(Objective objective) { objective_ = objective; }

    /** Read item types from a file. */
    void read(std::string instance_path);

    /** Set parameters. */
    void set_parameters(const Parameters& parameters) { parameters_ = parameters; }

    /** Add a quality rule. */
    inline void add_quality_rule(
            const std::vector<uint8_t>& quality_rule);

    /** Add an item type. */
    ItemTypeId add_item_type(
            const std::vector<ItemShape>& shapes,
            Profit profit = -1,
            ItemPos copies = 1,
            const std::vector<std::pair<Angle, Angle>>& allowed_rotations = {{0, 0}});

    /** Add a bin type. */
    BinTypeId add_bin_type(
            const Shape& shape,
            Profit cost = -1,
            BinPos copies = 1,
            BinPos copies_min = 0);

    /**
     * Add a bin type from another bin type.
     *
     * This method is used in the column generation procedure.
     */
    inline void add_bin_type(
            const BinType& bin_type,
            BinPos copies,
            BinPos copies_min = 0)
    {
        add_bin_type(
                bin_type.shape,
                bin_type.cost,
                copies,
                copies_min);
    }

    /**
     * Add an item type from another item type.
     *
     * This method is used in the column generation procedure.
     */
    inline void add_item_type(
            const ItemType& item_type,
            Profit profit,
            ItemPos copies)
    {
        add_item_type(
                item_type.shapes,
                profit,
                copies,
                item_type.allowed_rotations);
    }

    /** Add a defect. */
    void add_defect(
            BinTypeId i,
            DefectTypeId type,
            const Shape& shape,
            const std::vector<Shape>& holes = {});

    /*
     * Getters
     */

    /** Get the problem type. */
    inline ProblemType type() const { return ProblemType::Irregular; };

    /** Get the objective of the problem. */
    inline Objective objective() const { return objective_; }

    /* Get the number of item types. */
    inline ItemTypeId number_of_item_types() const { return item_types_.size(); }

    /** Get the number of items. */
    inline ItemTypeId number_of_items() const { return number_of_items_; }

    /** Get the number of bin types. */
    inline BinTypeId number_of_bin_types() const { return bin_types_.size(); }

    /** Get the number of bins. */
    inline BinPos number_of_bins() const { return bins_pos2type_.size(); }

    /** Get the number of defects. */
    inline DefectId number_of_defects() const { return number_of_defects_; }

    /** Get the total area of the items. */
    inline AreaDbl item_area() const { return item_area_; }

    /** Get the total area of the bins. */
    inline Area bin_area() const { return bin_area_; }

    /** Get the mean area of the items. */
    inline AreaDbl mean_area() const { return item_area_ / number_of_items(); }

    /** Get the total packable area. */
    inline AreaDbl packable_area() const { return packable_area_; }

    /** Get the total profit of the items. */
    inline Profit item_profit() const { return item_profit_; }

    /** Get the id of the item type with maximum efficiency. */
    inline ItemTypeId max_efficiency_item() const { return max_efficiency_item_; }

    /** Return true iff all items have infinite copies. */
    inline bool unbounded_knapsck() const { return all_item_type_infinite_copies_; }

    /** Get item type j. */
    inline const ItemType& item_type(ItemTypeId j) const { return item_types_[j]; }

    /** Get bin type i. */
    inline const BinType& bin_type(BinTypeId i) const { return bin_types_[i]; }

    /** Get defect k. */
    inline const Defect& defect(BinTypeId i, DefectId k) const { return bin_types_[i].defects[k]; }

    /**
     * Return 'true' iff quality_rule 'quality_rule' can contain a defect of
     * type 'type'.
     */
    inline bool can_contain(QualityRule quality_rule, DefectTypeId type) const;

    /** Get the i_pos's bin. */
    inline const BinType& bin(BinPos i_pos) const { return bin_types_[bins_pos2type_[i_pos]]; }

    /** Get the total area of the bins before bin i_pos. */
    AreaDbl previous_bin_area(BinPos i_pos) const;

    /** Get parameters. */
    const Parameters& parameters() const { return parameters_; }

    /*
     * Export.
     */

    /** Print the instance into a stream. */
    std::ostream& print(
            std::ostream& os,
            int verbose = 1) const;

private:

    /*
     * Private attributes.
     */

    /** Objective. */
    Objective objective_;

    /** Parameters. */
    Parameters parameters_;

    /** Item types. */
    std::vector<ItemType> item_types_;

    /** Bin types. */
    std::vector<BinType> bin_types_;

    /**
     * Quality rules.
     *
     * if 'quality_rules_[quality_rule][k] = 0' (resp. '1'), then defect 'k' is
     * not allowed (resp. allowed) for qulity rule rule 'quality_rule'.
     */
    std::vector<std::vector<uint8_t>> quality_rules_;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Convert bin position to bin type. */
    std::vector<BinTypeId> bins_pos2type_;

    /** Number of defects. */
    DefectId number_of_defects_ = 0;

    /** Total item area. */
    AreaDbl item_area_ = 0;

    /** Total bin area. */
    AreaDbl bin_area_ = 0;

    /** Total packable area. */
    AreaDbl packable_area_ = 0;

    /** Total item profit. */
    Profit item_profit_ = 0;

    /** Id of the item with maximum efficiency. */
    ItemTypeId max_efficiency_item_ = -1;

    /** True iff all item types have an infinite number of copies. */
    bool all_item_type_infinite_copies_ = false;

};

}
}

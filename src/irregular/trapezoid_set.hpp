#pragma once

#include "packingsolver/irregular/instance.hpp"

#include "irregular/rotations.hpp"
#include "irregular/shape_simplification.hpp"

#include "shape/trapezoid.hpp"

namespace packingsolver
{
namespace irregular
{

using ItemShapeTrapezoidPos = int64_t;
using TrapezoidSetId = int64_t;

/**
 * Direction in which the items are packed.
 *
 * The branching schemes using trapezoid sets always pack the items from left
 * to right, then from bottom to top, in their own frame. The other directions
 * are obtained by mirroring and transposing the instance.
 */
enum class Direction
{
    LeftToRightThenBottomToTop,
    LeftToRightThenTopToBottom,
    RightToLeftThenBottomToTop,
    RightToLeftThenTopToBottom,
    BottomToTopThenLeftToRight,
    BottomToTopThenRightToLeft,
    TopToBottomThenLeftToRight,
    TopToBottomThenRightToLeft,
    Any,
};

/** Convert a shape from original coordinates to direction coordinates. */
Shape convert_shape(
        Shape shape,
        Direction direction);

/** Mirror then rotate a shape. */
Shape convert_shape(
        const Shape& shape,
        Angle angle,
        bool mirror);

/**
 * Mirror then rotate a shape, then convert it from original coordinates to
 * direction coordinates.
 */
Shape convert_shape(
        const Shape& shape,
        Angle angle,
        bool mirror,
        Direction direction);

/** Convert a point from original coordinates to direction coordinates. */
Point convert_point(
        const Point& point,
        Direction direction);

/** Convert a point from direction coordinates to original coordinates. */
Point convert_point_back(
        const Point& point,
        Direction direction);

/**
 * Decomposition of an item type, in a given orientation and in the frame of a
 * given direction, into generalized trapezoids.
 */
struct TrapezoidSet
{
    /** Item type. */
    ItemTypeId item_type_id;

    /** Angle. */
    Angle angle;

    /** Mirror. */
    bool mirror;

    /** Shapes. */
    std::vector<std::vector<GeneralizedTrapezoid>> shapes;

    /** Inflated shapes. */
    std::vector<std::vector<GeneralizedTrapezoid>> shapes_inflated;

    /** Supporting parts of the trapezoid set. */
    std::vector<ShapePos> supporting_parts;

    /** Supported parts of the trapezoid set. */
    std::vector<ShapePos> supported_parts;

    LengthDbl x_min;

    LengthDbl x_max;

    LengthDbl y_min;

    LengthDbl y_max;

    void write_svg(
            const std::string& file_path) const;
};

/**
 * Supporting part (chain of edges with the material below them, on which
 * another item can rest) or supported part (chain of edges with the material
 * above them, with which an item rests on another one).
 */
struct Support
{
    Shape shape;

    TrapezoidSetId trapezoid_set_id = -1;

    ItemShapePos item_shape_pos = -1;

    ShapePos hole_pos = -1;

    BinTypeId bin_type_id = -1;

    DefectId defect_id = -1;

    AxisAlignedBoundingBox aabb;
};

/** Trapezoid of an obstacle of a bin: its borders, defects and fixed items. */
struct DefectTrapezoid
{
    DefectTrapezoid(
            DefectId defect_id,
            const GeneralizedTrapezoid& trapezoid):
        defect_id(defect_id),
        trapezoid(trapezoid) { }

    /** Defect id, '-1' for the borders and the fixed items. */
    DefectId defect_id = -1;

    /** Trapezoid. */
    GeneralizedTrapezoid trapezoid;
};

/**
 * Bin type, with the trapezoid sets of the items that can be packed into it,
 * in the frame of a given direction.
 */
struct TrapezoidBinType
{
    /** Trapezoids of the obstacles of the bin. */
    std::vector<DefectTrapezoid> defects;

    /**
     * Indices into supporting_parts for the supports belonging to the bin
     * itself (floor, defect top surfaces and fixed items).
     */
    std::vector<ShapePos> bin_supporting_part_ids;

    /** Trapezoid sets for items placed in this bin type. */
    std::vector<TrapezoidSet> trapezoid_sets;

    /** All Support objects for this bin type (bin + item shapes). */
    std::vector<Support> supporting_parts;

    /** All supported-part Support objects for items in this bin type. */
    std::vector<Support> supported_parts;

    LengthDbl x_min;

    LengthDbl x_max;

    LengthDbl y_min;

    LengthDbl y_max;
};

/**
 * Build the trapezoid set of an item type in a given orientation, in the
 * frame of a given direction.
 *
 * Its supporting and supported parts are appended to 'supporting_parts' and
 * 'supported_parts'; the positions stored in the trapezoid set refer to these
 * vectors.
 */
TrapezoidSet build_trapezoid_set(
        const Instance& instance,
        const SimplifiedInstance& simplified_instance,
        ItemTypeId item_type_id,
        const ItemTypeRotation& rotation,
        Direction direction,
        TrapezoidSetId trapezoid_set_id,
        std::vector<Support>& supporting_parts,
        std::vector<Support>& supported_parts);

/**
 * Build the trapezoid bin type of a bin type in the frame of a given
 * direction.
 *
 * The shapes used are the simplified ones; the item types are considered in
 * the given orientations.
 */
TrapezoidBinType build_trapezoid_bin_type(
        const Instance& instance,
        const SimplifiedInstance& simplified_instance,
        const std::vector<std::vector<ItemTypeRotation>>& item_types_rotations,
        BinTypeId bin_type_id,
        Direction direction);

}
}

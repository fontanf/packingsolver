#include "irregular/shape_simplification.hpp"

#include "packingsolver/irregular/instance_builder.hpp"

#include "irregular/shape_self_intersections_removal.hpp"

#include "optimizationtools/containers/indexed_binary_heap.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

enum class ApproximatedShapeType { Border, Defect, DefectHole, ItemShape, ItemShapeHole };

struct ApproximatedElementKey
{
    ApproximatedShapeType type = ApproximatedShapeType::Border;

    BinTypeId bin_type_id = -1;

    DefectId defect_id = -1;

    ShapePos hole_pos = -1;

    ItemTypeId item_type_id = -1;

    ItemShapePos item_shape_pos = -1;

    ElementPos element_pos = -1;
};

struct ApproximatedShapeElement
{
    ShapeElement element;

    ElementPos element_key_id;

    bool removed = false;

    ElementPos element_prev_pos = -1;

    ElementPos element_next_pos = -1;
};

struct ApproximatedShape
{
    ElementPos number_of_elements = 0;

    BinPos copies = 1;

    std::vector<ApproximatedShapeElement> elements;

    Shape shape() const
    {
        Shape new_shape;

        //for (ElementPos element_pos = 0;
        //        element_pos < (ElementPos)elements.size();
        //        ++element_pos) {
        //    const ApproximatedShapeElement& element = elements[element_pos];
        //    std::cout << "element_pos " << element_pos
        //        << " removed " << element.removed
        //        << " element_prev_pos " << element.element_prev_pos
        //        << " element_next_pos " << element.element_next_pos
        //        << std::endl;
        //}

        ElementPos initial_element_pos = 0;
        while (this->elements[initial_element_pos].removed)
            initial_element_pos++;
        //std::cout << "initial_element_pos " << initial_element_pos
        //    << " / " << this->elements.size()
        //    << std::endl;

        ElementPos element_pos = initial_element_pos;
        for (;;) {
            //std::cout << "element_pos " << element_pos
            //    << " element_next_pos " << this->elements[element_pos].element_next_pos
            //    << " removed " << this->elements[element_pos].removed
            //    << std::endl;
            new_shape.elements.push_back(this->elements[element_pos].element);
            element_pos = this->elements[element_pos].element_next_pos;
            if (element_pos == initial_element_pos)
                break;
        }

        return new_shape;
    }
};

struct ApproximatedDefect
{
    /** Shape. */
    ApproximatedShape shape;

    /** Holes. */
    std::vector<ApproximatedShape> holes;
};

struct ApproximatedBinType
{
    /** Shape of the bin type. */
    std::vector<ApproximatedShape> borders;

    /** Defects of the bin type. */
    std::vector<ApproximatedDefect> defects;
};

struct ApproximatedItemShape
{
    /** Main shape. */
    ApproximatedShape shape;

    /** Holes. */
    std::vector<ApproximatedShape> holes;
};

struct ApproximatedItemType
{
    /** Shapes. */
    std::vector<ApproximatedItemShape> shapes;
};

ApproximatedShape& approximated_shape(
        std::vector<ApproximatedBinType>& approximated_bin_types,
        std::vector<ApproximatedItemType>& approximated_item_types,
        const std::vector<ApproximatedElementKey>& element_keys,
        ElementPos element_key_id)
{
    const ApproximatedElementKey& element_key = element_keys[element_key_id];
    //std::cout << "element_key_id " << element_key_id
    //    << " type " << (int)element_key.type
    //    << " bin_type_id " << element_key.bin_type_id
    //    << " defect_id " << element_key.defect_id
    //    << " hole_pos " << element_key.hole_pos
    //    << " item_type_id " << element_key.item_type_id
    //    << std::endl;
    switch (element_key.type)
    {
    case ApproximatedShapeType::Border:
        return approximated_bin_types[element_key.bin_type_id].borders[element_key.defect_id];
    case ApproximatedShapeType::Defect:
        return approximated_bin_types[element_key.bin_type_id].defects[element_key.defect_id].shape;
    case ApproximatedShapeType::DefectHole:
        return approximated_bin_types[element_key.bin_type_id].defects[element_key.defect_id].holes[element_key.hole_pos];
    case ApproximatedShapeType::ItemShape:
        return approximated_item_types[element_key.item_type_id].shapes[element_key.item_shape_pos].shape;
    case ApproximatedShapeType::ItemShapeHole:
        return approximated_item_types[element_key.item_type_id].shapes[element_key.item_shape_pos].holes[element_key.hole_pos];
    default:
        return approximated_item_types[0].shapes[0].shape;
    }
}

AreaDbl compute_approximation_cost(
        const ApproximatedShape& shape,
        ApproximatedShapeType type,
        ElementPos element_pos)
{
    if (element_pos >= (ElementPos)shape.elements.size()) {
        throw std::runtime_error(
                "irregular::compute_approximation_cost; "
                "element_pos: " + std::to_string(element_pos) + "; "
                "shape.elements.size(): " + std::to_string(shape.elements.size()) + ".\n");
    }
    const ApproximatedShapeElement& element = shape.elements[element_pos];
    //std::cout << "element_pos " << element_pos << " / " << shape.elements.size() << std::endl;
    //std::cout << "element_prev_pos " << element.element_prev_pos << std::endl;
    //std::cout << "element_next_pos " << element.element_next_pos << std::endl;
    const ApproximatedShapeElement& element_prev = shape.elements[element.element_prev_pos];
    const ApproximatedShapeElement& element_next = shape.elements[element.element_next_pos];

    // Compute previous and next angles.
    Angle angle_prev = angle_radian(
            element_prev.element.start - element_prev.element.end,
            element.element.end - element.element.start);
    Angle angle_next = angle_radian(
            element.element.start - element.element.end,
            element_next.element.end - element_next.element.start);

    if (type == ApproximatedShapeType::ItemShape
            || type == ApproximatedShapeType::Defect) {
        // Outer approximation.
        if (equal(angle_next, M_PI)) {
            return 0.0;
        } else if (angle_next > M_PI) {
            if (angle_prev > M_PI) {
                // Check if the triangle is bounded.
                // Compute intersection.
                LengthDbl x1 = element_prev.element.start.x;
                LengthDbl y1 = element_prev.element.start.y;
                LengthDbl x2 = element_prev.element.end.x;
                LengthDbl y2 = element_prev.element.end.y;
                LengthDbl x3 = element_next.element.start.x;
                LengthDbl y3 = element_next.element.start.y;
                LengthDbl x4 = element_next.element.end.x;
                LengthDbl y4 = element_next.element.end.y;
                LengthDbl denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
                // If no intersection, no approximation possible.
                if (equal(denom, 0.0))
                    return std::numeric_limits<Angle>::infinity();
                LengthDbl xp = ((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / denom;
                LengthDbl yp = ((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / denom;
                AreaDbl cost = shape.copies * build_shape({
                        {element.element.start.x}, {element.element.start.y},
                        {xp, yp},
                        {element.element.end.x, element.element.end.y}
                        }).compute_area();
                if (cost < 0)
                    return std::numeric_limits<Angle>::infinity();
                return cost;
            } else {
                // No approximation possible.
                return std::numeric_limits<Angle>::infinity();
            }
        } else {
            // angle_next < M_PI
            Angle cost = shape.copies * build_shape({
                    {element.element.start.x, element.element.start.y},
                    {element_next.element.end.x, element_next.element.end.y},
                    {element.element.end.x, element.element.end.y}
                    }).compute_area();
            if (cost < 0) {
                throw std::runtime_error(
                        "irregular::compute_approximation_cost: outer; "
                        "angle_prev: " + std::to_string(angle_prev) + "; "
                        "angle_next: " + std::to_string(angle_next) + "; "
                        "element_prev.element: " + element_prev.element.to_string() + "; "
                        "element.element: " + element.element.to_string() + "; "
                        "element_next.element: " + element_next.element.to_string() + "; "
                        "element.element.start: " + element.element.start.to_string() + "; "
                        "element_next.element.end: " + element_next.element.end.to_string() + "; "
                        "element.element.end: " + element.element.end.to_string() + "; "
                        "cost: " + std::to_string(cost) + ".\n");
            }
            return cost;
        }
    } else {
        // Inner approximation.
        if (equal(angle_next, M_PI)) {
            return 0.0;
        } else if (angle_next < M_PI) {
            if (angle_prev < M_PI) {
                // Check if the triangle is bounded.
                // Compute intersection.
                LengthDbl x1 = element_prev.element.start.x;
                LengthDbl y1 = element_prev.element.start.y;
                LengthDbl x2 = element_prev.element.end.x;
                LengthDbl y2 = element_prev.element.end.y;
                LengthDbl x3 = element_next.element.start.x;
                LengthDbl y3 = element_next.element.start.y;
                LengthDbl x4 = element_next.element.end.x;
                LengthDbl y4 = element_next.element.end.y;
                LengthDbl denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
                // If no intersection, no approximation possible.
                if (equal(denom, 0.0))
                    return std::numeric_limits<Angle>::infinity();
                LengthDbl xp = ((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / denom;
                LengthDbl yp = ((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / denom;
                AreaDbl cost = shape.copies * build_shape({
                        {element.element.start.x, element.element.start.y},
                        {element.element.end.x, element.element.end.y},
                        {xp, yp}}).compute_area();
                if (cost < 0)
                    return std::numeric_limits<Angle>::infinity();
                return cost;
            } else {
                // No approximation possible.
                return std::numeric_limits<Angle>::infinity();
            }
        } else {
            // angle_next < M_PI
            Angle cost = shape.copies * build_shape({
                    {element.element.start.x, element.element.start.y},
                    {element.element.end.x, element.element.end.y},
                    {element_next.element.end.x, element_next.element.end.y}
                    }).compute_area();
            if (cost < 0) {
                throw std::runtime_error(
                        "irregular::compute_approximation_cost: inner; "
                        "angle_prev: " + std::to_string(angle_prev) + "; "
                        "angle_next: " + std::to_string(angle_next) + "; "
                        "element.element.start: " + element.element.start.to_string() + "; "
                        "element_next.element.end: " + element_next.element.end.to_string() + "; "
                        "element.element.end: " + element.element.end.to_string() + "; "
                        "cost: " + std::to_string(cost) + ".\n");
            }
            return cost;
        }
    }
    return 0.0;
}

void apply_approximation(
        ApproximatedShape& shape,
        ApproximatedShapeType type,
        ElementPos element_pos)
{
    ApproximatedShapeElement& element = shape.elements[element_pos];
    //std::cout << "element_pos " << element_pos << " / " << shape.elements.size() << std::endl;
    //std::cout << "element_prev_pos " << element.element_prev_pos << std::endl;
    //std::cout << "element_next_pos " << element.element_next_pos << std::endl;
    ApproximatedShapeElement& element_prev = shape.elements[element.element_prev_pos];
    ApproximatedShapeElement& element_next = shape.elements[element.element_next_pos];

    // Compute previous and next angles.
    Angle angle_prev = angle_radian(
            element_prev.element.start - element_prev.element.end,
            element.element.end - element.element.start);
    Angle angle_next = angle_radian(
            element.element.start - element.element.end,
            element_next.element.end - element_next.element.start);

    if (type == ApproximatedShapeType::ItemShape
            || type == ApproximatedShapeType::Defect) {
        // Outer approximation.
        if (equal(angle_next, M_PI)) {
            element_next.element.start = element.element.start;
        } else if (angle_next > M_PI) {
            if (angle_prev > M_PI) {
                // Check if the triangle is bounded.
                // Compute intersection.
                LengthDbl x1 = element_prev.element.start.x;
                LengthDbl y1 = element_prev.element.start.y;
                LengthDbl x2 = element_prev.element.end.x;
                LengthDbl y2 = element_prev.element.end.y;
                LengthDbl x3 = element_next.element.start.x;
                LengthDbl y3 = element_next.element.start.y;
                LengthDbl x4 = element_next.element.end.x;
                LengthDbl y4 = element_next.element.end.y;
                LengthDbl denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
                if (equal(denom, 0.0)) {
                    throw std::runtime_error(
                            "irregular::apply_approximation: outer; "
                            "element_prev.element: " + element_prev.element.to_string() + "; "
                            "element.element: " + element.element.to_string() + "; "
                            "element_next.element: " + element_next.element.to_string() + "; "
                            "angle_prev: " + std::to_string(angle_prev) + "; "
                            "angle_next: " + std::to_string(angle_next) + "; "
                            "denom: " + std::to_string(denom) + ".");
                }
                LengthDbl xp = ((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / denom;
                LengthDbl yp = ((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / denom;
                element_prev.element.end = {xp, yp};
                element_next.element.start = {xp, yp};
            } else {
                // No approximation possible.
                throw std::runtime_error(
                        "irregular::apply_approximation: outer; "
                        "element_prev.element: " + element_prev.element.to_string() + "; "
                        "element.element: " + element.element.to_string() + "; "
                        "element_next.element: " + element_next.element.to_string() + "; "
                        "angle_prev: " + std::to_string(angle_prev) + "; "
                        "angle_next: " + std::to_string(angle_next) + ".");
            }
        } else {
            // angle_next < M_PI
            element_next.element.start = element.element.start;
        }
    } else {
        // Inner approximation.
        if (equal(angle_next, M_PI)) {
            element_next.element.start = element.element.start;
        } else if (angle_next < M_PI) {
            if (angle_prev < M_PI) {
                // Check if the triangle is bounded.
                // Compute intersection.
                LengthDbl x1 = element_prev.element.start.x;
                LengthDbl y1 = element_prev.element.start.y;
                LengthDbl x2 = element_prev.element.end.x;
                LengthDbl y2 = element_prev.element.end.y;
                LengthDbl x3 = element_next.element.start.x;
                LengthDbl y3 = element_next.element.start.y;
                LengthDbl x4 = element_next.element.end.x;
                LengthDbl y4 = element_next.element.end.y;
                LengthDbl denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
                if (equal(denom, 0.0)) {
                    throw std::runtime_error(
                            "irregular::apply_approximation: inner; "
                            "denom: " + std::to_string(denom) + ".");
                }
                LengthDbl xp = ((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / denom;
                LengthDbl yp = ((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / denom;
                AreaDbl cost = build_shape({
                        {element.element.start.x, element.element.start.y},
                        {element.element.end.x, element.element.end.y},
                        {xp, yp}}).compute_area();
                element_prev.element.end = {xp, yp};
                element_next.element.start = {xp, yp};
            } else {
                // No approximation possible.
                throw std::runtime_error(
                        "irregular::apply_approximation: inner; "
                        "angle_prev: " + std::to_string(angle_prev) + "; "
                        "angle_next: " + std::to_string(angle_next) + ".");
            }
        } else {
            // angle_next < M_PI
            element_next.element.start = element.element.start;
        }
    }
    element_prev.element_next_pos = element.element_next_pos;
    element_next.element_prev_pos = element.element_prev_pos;
    element.removed = true;
    shape.number_of_elements--;
}

}

Instance irregular::shape_simplification(
        const Instance& instance,
        double maximum_approximation_ratio)
{
    //std::cout << "shape_simplification" << std::endl;
    // Build element_keys, approximated_bin_types, approximated_item_types.
    std::vector<ApproximatedElementKey> element_keys;
    std::vector<ApproximatedBinType> approximated_bin_types;
    std::vector<ApproximatedItemType> approximated_item_types;
    AreaDbl total_bin_area = 0.0;
    AreaDbl total_item_area = 0.0;
    ElementPos total_number_of_elements = 0;

    // Add elements from bin types.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        ApproximatedBinType approximated_bin_type;

        total_bin_area += bin_type.copies * (bin_type.x_max - bin_type.x_min) * (bin_type.y_max - bin_type.y_min);
        auto bin_borders = borders(bin_type.shape);

        // Borders.
        for (DefectId border_pos = 0;
                border_pos < (DefectId)bin_borders.size();
                ++border_pos) {
            ApproximatedShape approximated_border;

            const Shape& shape = bin_borders[border_pos];
            //std::cout << "border " << shape.to_string(0);
            total_bin_area -= bin_type.copies * shape.compute_area();
            total_number_of_elements += bin_type.copies * shape.elements.size();
            ApproximatedShape approximated_shape;
            for (ElementPos element_pos = 0;
                    element_pos < (ElementPos)shape.elements.size();
                    ++element_pos) {
                const ShapeElement& element = shape.elements[element_pos];
                ApproximatedShapeElement approximated_element;
                approximated_element.element = element;
                approximated_element.element_prev_pos = (element_pos != 0)?
                    element_pos - 1:
                    shape.elements.size() - 1;
                approximated_element.element_next_pos = (element_pos != (ElementPos)shape.elements.size() - 1)?
                    element_pos + 1:
                    0;
                approximated_element.element_key_id = element_keys.size();
                approximated_shape.elements.push_back(approximated_element);
                approximated_shape.number_of_elements++;
                approximated_shape.copies = bin_type.copies;
                ApproximatedElementKey element_key;
                element_key.type = ApproximatedShapeType::Border;
                element_key.bin_type_id = bin_type_id;
                element_key.defect_id = border_pos;
                element_key.element_pos = element_pos;
                element_keys.push_back(element_key);
            }
            approximated_border = approximated_shape;
            approximated_bin_type.borders.push_back(approximated_border);
        }

        // Defects.
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            const Defect& defect = bin_type.defects[defect_id];
            ApproximatedDefect approximated_defect;

            const Shape& shape = defect.shape;
            total_bin_area -= bin_type.copies * shape.compute_area();
            total_number_of_elements += bin_type.copies * shape.elements.size();
            ApproximatedShape approximated_shape;
            for (ElementPos element_pos = 0;
                    element_pos < (ElementPos)shape.elements.size();
                    ++element_pos) {
                const ShapeElement& element = shape.elements[element_pos];
                ApproximatedShapeElement approximated_element;
                approximated_element.element = element;
                approximated_element.element_prev_pos = (element_pos != 0)?
                    element_pos - 1:
                    shape.elements.size() - 1;
                approximated_element.element_next_pos = (element_pos != (ElementPos)shape.elements.size() - 1)?
                    element_pos + 1:
                    0;
                approximated_element.element_key_id = element_keys.size();
                approximated_shape.elements.push_back(approximated_element);
                approximated_shape.number_of_elements++;
                approximated_shape.copies = bin_type.copies;
                ApproximatedElementKey element_key;
                element_key.type = ApproximatedShapeType::Defect;
                element_key.bin_type_id = bin_type_id;
                element_key.defect_id = defect_id;
                element_key.element_pos = element_pos;
                element_keys.push_back(element_key);
            }
            approximated_defect.shape = approximated_shape;

            ShapePos hole_pos = 0;
            for (const Shape& shape: defect.holes) {

                if (strictly_lesser(shape.compute_area(), instance.smallest_item_area()))
                    continue;
                total_bin_area += bin_type.copies * shape.compute_area();
                total_number_of_elements += bin_type.copies * shape.elements.size();
                ApproximatedShape approximated_shape;
                for (ElementPos element_pos = 0;
                        element_pos < (ElementPos)shape.elements.size();
                        ++element_pos) {
                    const ShapeElement& element = shape.elements[element_pos];
                    ApproximatedShapeElement approximated_element;
                    approximated_element.element = element;
                    approximated_element.element_prev_pos = (element_pos != 0)?
                        element_pos - 1:
                        shape.elements.size() - 1;
                    approximated_element.element_next_pos = (element_pos != (ElementPos)shape.elements.size() - 1)?
                        element_pos + 1:
                        0;
                    approximated_element.element_key_id = element_keys.size();
                    approximated_shape.elements.push_back(approximated_element);
                    approximated_shape.number_of_elements++;
                    approximated_shape.copies = bin_type.copies;
                    ApproximatedElementKey element_key;
                    element_key.type = ApproximatedShapeType::DefectHole;
                    element_key.bin_type_id = bin_type_id;
                    element_key.defect_id = defect_id;
                    element_key.hole_pos = hole_pos;
                    element_key.element_pos = element_pos;
                    element_keys.push_back(element_key);
                }
                hole_pos++;
                approximated_defect.holes.push_back(approximated_shape);
            }
            approximated_bin_type.defects.push_back(approximated_defect);
        }
        approximated_bin_types.push_back(approximated_bin_type);
    }
    // Add elements from item types.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ApproximatedItemType approximated_item_type;
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            const ItemShape& item_shape = item_type.shapes[item_shape_pos];
            ApproximatedItemShape approximated_item_shape;

            const Shape& shape = item_shape.shape;
            total_item_area += item_type.copies * shape.compute_area();
            total_number_of_elements += item_type.copies * shape.elements.size();
            ApproximatedShape approximated_shape;
            for (ElementPos element_pos = 0;
                    element_pos < (ElementPos)shape.elements.size();
                    ++element_pos) {
                const ShapeElement& element = shape.elements[element_pos];
                ApproximatedShapeElement approximated_element;
                approximated_element.element = element;
                approximated_element.element_prev_pos = (element_pos != 0)?
                    element_pos - 1:
                    shape.elements.size() - 1;
                approximated_element.element_next_pos = (element_pos != (ElementPos)shape.elements.size() - 1)?
                    element_pos + 1:
                    0;
                approximated_element.element_key_id = element_keys.size();
                approximated_shape.elements.push_back(approximated_element);
                approximated_shape.number_of_elements++;
                approximated_shape.copies = item_type.copies;
                ApproximatedElementKey element_key;
                element_key.type = ApproximatedShapeType::ItemShape;
                element_key.item_type_id = item_type_id;
                element_key.item_shape_pos = item_shape_pos;
                element_key.element_pos = element_pos;
                element_keys.push_back(element_key);
            }
            approximated_item_shape.shape = approximated_shape;

            ShapePos hole_pos = 0;
            for (const Shape& shape: item_shape.holes) {
                if (strictly_lesser(shape.compute_area(), instance.smallest_item_area()))
                    continue;
                total_item_area -= item_type.copies * shape.compute_area();
                total_number_of_elements += item_type.copies * shape.elements.size();
                ApproximatedShape approximated_shape;
                for (ElementPos element_pos = 0;
                        element_pos < (ElementPos)shape.elements.size();
                        ++element_pos) {
                    const ShapeElement& element = shape.elements[element_pos];
                    ApproximatedShapeElement approximated_element;
                    approximated_element.element = element;
                    approximated_element.element_prev_pos = (element_pos != 0)?
                        element_pos - 1:
                        shape.elements.size() - 1;
                    approximated_element.element_next_pos = (element_pos != (ElementPos)shape.elements.size() - 1)?
                        element_pos + 1:
                        0;
                    approximated_element.element_key_id = element_keys.size();
                    approximated_shape.elements.push_back(approximated_element);
                    approximated_shape.number_of_elements++;
                    approximated_shape.copies = item_type.copies;
                    ApproximatedElementKey element_key;
                    element_key.type = ApproximatedShapeType::ItemShapeHole;
                    element_key.item_type_id = item_type_id;
                    element_key.item_shape_pos = item_shape_pos;
                    element_key.hole_pos = hole_pos;
                    element_key.element_pos = element_pos;
                    element_keys.push_back(element_key);
                }
                hole_pos++;
                approximated_item_shape.holes.push_back(approximated_shape);
            }
            approximated_item_type.shapes.push_back(approximated_item_shape);
        }
        approximated_item_types.push_back(approximated_item_type);
    }

    // Initialize priority_queue.
    optimizationtools::IndexedBinaryHeap<AreaDbl> priority_queue(
            element_keys.size());
    for (ElementPos element_key_id = 0;
            element_key_id < (ElementPos)element_keys.size();
            ++element_key_id) {
        const ApproximatedElementKey& element_key = element_keys[element_key_id];
        ApproximatedShape& shape = approximated_shape(
                approximated_bin_types,
                approximated_item_types,
                element_keys,
                element_key_id);
        //std::cout << "element_key_id " << element_key_id
        //    << " type " << (int)element_key.type
        //    << " bin_type_id " << element_key.bin_type_id
        //    << " defect_id " << element_key.defect_id
        //    << " hole_pos " << element_key.hole_pos
        //    << " item_type_id " << element_key.item_type_id
        //    << " element_pos " << element_key.element_pos
        //    << std::endl;
        AreaDbl cost = compute_approximation_cost(
                shape,
                element_key.type,
                element_key.element_pos);
        //std::cout << "element_key_id " << element_key_id
        //    << " type " << (int)element_key.type
        //    << " bin_type_id " << element_key.bin_type_id
        //    << " defect_id " << element_key.defect_id
        //    << " hole_pos " << element_key.hole_pos
        //    << " item_type_id " << element_key.item_type_id
        //    << " element_pos " << element_key.element_pos
        //    << " cost " << cost
        //    << std::endl;
        if (cost < std::numeric_limits<AreaDbl>::infinity())
            priority_queue.update_key(element_key_id, cost);
    }

    AreaDbl current_cost = 0.0;
    while (!priority_queue.empty()) {

        // Check stop criterion based on average number of element per item.
        if (total_number_of_elements <= 8 * instance.number_of_items())
            break;

        // Draw highest priority element.
        ElementPos element_key_id = priority_queue.top().first;
        //std::cout << "element_key_id " << element_key_id
        //    << " " << priority_queue.size() << std::endl;
        AreaDbl new_cost = priority_queue.top().second;
        priority_queue.pop();

        if (new_cost == std::numeric_limits<AreaDbl>::infinity())
            break;

        // Check stop criterion based on maximum approximation ratio.
        if (current_cost + new_cost
                > maximum_approximation_ratio
                * std::min(total_bin_area, total_item_area)) {
            break;
        }

        ApproximatedShape& shape = approximated_shape(
                approximated_bin_types,
                approximated_item_types,
                element_keys,
                element_key_id);

        if (shape.number_of_elements <= 4)
            continue;

        const ApproximatedElementKey& element_key = element_keys[element_key_id];
        //std::cout << "element_key_id " << element_key_id
        //    << " type " << (int)element_key.type
        //    << " bin_type_id " << element_key.bin_type_id
        //    << " defect_id " << element_key.defect_id
        //    << " hole_pos " << element_key.hole_pos
        //    << " item_type_id " << element_key.item_type_id
        //    << " element_pos " << element_key.element_pos
        //    << " cost " << new_cost
        //    << " current_cost " << current_cost
        //    << " max_cost " << (std::min)(bin_area, item_area)
        //    << std::endl;

        // Apply simplification.
        current_cost += new_cost;
        total_number_of_elements -= shape.copies;
        const ApproximatedShapeElement& element = shape.elements[element_key.element_pos];
        ElementPos element_prev_pos = element.element_prev_pos;
        ElementPos element_next_pos = element.element_next_pos;
        ElementPos element_next_next_pos = shape.elements[element_next_pos].element_next_pos;
        apply_approximation(
                shape,
                element_key.type,
                element_key.element_pos);

        // Update priority queue values.
        AreaDbl cost_prev = compute_approximation_cost(
                shape,
                element_key.type,
                element_prev_pos);
        AreaDbl cost_next = compute_approximation_cost(
                shape,
                element_key.type,
                element_next_pos);
        AreaDbl cost_next_next = compute_approximation_cost(
                shape,
                element_key.type,
                element_next_next_pos);
        //std::cout << "element_prev_pos " << element_prev_pos
        //    << " cost_prev " << cost_prev << std::endl;
        //std::cout << "element_next_pos " << element_next_pos
        //    << " cost_next " << cost_next << std::endl;
        //std::cout << "element_next_next_pos " << element_next_next_pos
        //    << " cost_next_next " << cost_next_next << std::endl;
        if (priority_queue.contains(shape.elements[element_prev_pos].element_key_id)
                || cost_prev < std::numeric_limits<AreaDbl>::infinity()) {
            priority_queue.update_key(
                    shape.elements[element_prev_pos].element_key_id,
                    cost_prev);
        }
        if (priority_queue.contains(shape.elements[element_next_pos].element_key_id)
                || cost_next < std::numeric_limits<AreaDbl>::infinity()) {
            priority_queue.update_key(
                    shape.elements[element_next_pos].element_key_id,
                    cost_next);
        }
        if (priority_queue.contains(shape.elements[element_next_next_pos].element_key_id)
                || cost_next_next < std::numeric_limits<AreaDbl>::infinity()) {
            priority_queue.update_key(
                    shape.elements[element_next_next_pos].element_key_id,
                    cost_next_next);
        }
    }
    //std::cout << "end" << std::endl;

    // Build new instance.
    InstanceBuilder new_instance_builder;
    // Parameters.
    new_instance_builder.set_objective(instance.objective());
    new_instance_builder.set_parameters(instance.parameters());
    // Add bins.
    //std::cout << "add bins..." << std::endl;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinType new_bin_type = bin_type;
        new_bin_type.shape = build_shape({
                {bin_type.x_min, bin_type.y_min},
                {bin_type.x_max, bin_type.y_min},
                {bin_type.x_max, bin_type.y_max},
                {bin_type.x_min, bin_type.y_max}});
        new_bin_type.defects.clear();
        // Borders.
        for (const ApproximatedShape& border: approximated_bin_types[bin_type_id].borders) {
            //std::cout << "border " << border.shape().to_string(0) << std::endl;
            auto border_processed = remove_self_intersections(border.shape());
            //std::cout << "border_processed " << border_processed.first.to_string(0) << std::endl;
            Defect new_defect;
            new_defect.shape = border_processed.first;
            new_defect.holes = border_processed.second;
            new_bin_type.defects.push_back(new_defect);
        }
        // Defects.
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            const ApproximatedDefect& defect = approximated_bin_types[bin_type_id].defects[defect_id];
            auto defect_processed = remove_self_intersections(approximated_bin_types[bin_type_id].defects[defect_id].shape.shape());
            Defect new_defect = bin_type.defects[defect_id];
            new_defect.shape = defect_processed.first;
            for (ShapePos hole_pos = 0;
                    hole_pos < (ShapePos)defect.holes.size();
                    ++hole_pos) {
                auto hole_processed = extract_all_holes_from_self_intersecting_hole(defect.holes[hole_pos].shape());
                for (const Shape& hole: hole_processed)
                    new_defect.holes.push_back(hole);
            }
            new_bin_type.defects.push_back(new_defect);
        }
        new_instance_builder.add_bin_type(
                new_bin_type,
                new_bin_type.copies,
                new_bin_type.copies_min);
    }
    // Add items.
    //std::cout << "add items..." << std::endl;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        //std::cout << "item_type_id " << item_type_id << std::endl;
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemType new_item_type = item_type;
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            //std::cout << "item_shape_pos " << item_shape_pos << std::endl;
            const ApproximatedItemShape& item_shape = approximated_item_types[item_type_id].shapes[item_shape_pos];
            auto item_shape_processed = remove_self_intersections(item_shape.shape.shape());
            new_item_type.shapes[item_shape_pos].shape = item_shape_processed.first;
            new_item_type.shapes[item_shape_pos].holes = item_shape_processed.second;
            for (ShapePos hole_pos = 0;
                    hole_pos < (ShapePos)item_shape.holes.size();
                    ++hole_pos) {
                //std::cout << "hole_pos " << hole_pos << std::endl;
                auto hole_processed = extract_all_holes_from_self_intersecting_hole(item_shape.holes[hole_pos].shape());
                for (const Shape& hole: hole_processed)
                    new_item_type.shapes[item_shape_pos].holes.push_back(hole);
            }
        }
        new_instance_builder.add_item_type(
                new_item_type,
                new_item_type.profit,
                new_item_type.copies);
    }

    Instance new_instance = new_instance_builder.build();

    //new_instance.format(std::cout, 4);
    //instance.write("original_instance.json");
    //new_instance.write("approximated_instance.json");
    //exit(1);
    //std::cout << "shape_simplification end" << std::endl;
    //return instance;

    return new_instance;
}

#include "irregular/shape_simplification.hpp"

#include "shape/simplification.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

enum class ApproximatedShapeType
{
    Border,
    BorderInflated,
    BorderHole,
    BorderHoleDeflated,
    Defect,
    DefectInflated,
    DefectHole,
    DefectHoleDeflated,
    ItemShape,
    ItemShapeInflated,
    ItemShapeHole,
    ItemShapeHoleDeflated,
};

struct ApproximatedShapeKey
{
    ApproximatedShapeType type = ApproximatedShapeType::Border;

    BinTypeId bin_type_id = -1;

    DefectId defect_id = -1;

    ShapePos hole_pos = -1;

    ItemTypeId item_type_id = -1;

    ItemShapePos item_shape_pos = -1;
};

}

SimplifiedInstance irregular::shape_simplification(
        const Instance& instance,
        double maximum_approximation_ratio)
{
    std::vector<shape::SimplifyInputShape> simplify_inflated_bin_types_input;
    std::vector<shape::SimplifyInputShape> simplify_item_types_input;
    std::vector<shape::SimplifyInputShape> simplify_inflated_item_types_input;
    std::vector<ApproximatedShapeKey> simplify_inflated_bin_types_keys;
    std::vector<ApproximatedShapeKey> simplify_item_types_keys;
    std::vector<ApproximatedShapeKey> simplify_inflated_item_types_keys;

    AreaDbl smallest_item_area = std::numeric_limits<AreaDbl>::infinity();
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        AreaDbl area = 0.0;
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            const ItemShape& item_shape = item_type.shapes[item_shape_pos];
            area += item_shape.shape_scaled.shape.compute_area();
        }
        if (smallest_item_area > area)
            smallest_item_area = area;
    }

    AreaDbl total_bin_area = 0.0;
    AreaDbl total_item_area = 0.0;

    // Add elements from bin types.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        total_bin_area += bin_type.copies * (bin_type.x_max - bin_type.x_min) * (bin_type.y_max - bin_type.y_min);

        // Borders.
        for (DefectId border_pos = 0;
                border_pos < (DefectId)bin_type.borders.size();
                ++border_pos) {
            const Defect& border = bin_type.borders[border_pos];
            total_bin_area -= bin_type.copies * border.shape_scaled.shape.compute_area();

            bool outer = true;
            SimplifyInputShape simplify_input_shape;
            simplify_input_shape.shape = border.shape_inflated.shape;
            simplify_input_shape.copies = bin_type.copies;
            simplify_input_shape.outer = outer;
            simplify_inflated_bin_types_input.push_back(simplify_input_shape);

            ApproximatedShapeKey key;
            key.type = ApproximatedShapeType::BorderInflated;
            key.bin_type_id = bin_type_id;
            key.defect_id = border_pos;
            simplify_inflated_bin_types_keys.push_back(key);
        }

        // Defects.
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            const Defect& defect = bin_type.defects[defect_id];
            total_bin_area -= bin_type.copies * defect.shape_scaled.shape.compute_area();

            bool outer = true;
            SimplifyInputShape simplify_input_shape;
            simplify_input_shape.shape = defect.shape_inflated.shape;
            simplify_input_shape.copies = bin_type.copies;
            simplify_input_shape.outer = outer;
            simplify_inflated_bin_types_input.push_back(simplify_input_shape);

            ApproximatedShapeKey key;
            key.type = ApproximatedShapeType::DefectInflated;
            key.bin_type_id = bin_type_id;
            key.defect_id = defect_id;
            simplify_inflated_bin_types_keys.push_back(key);

            for (ShapePos hole_pos = 0;
                    hole_pos < defect.shape_scaled.holes.size();
                    ++defect_id) {
                const Shape& hole = defect.shape_scaled.holes[hole_pos];

                if (strictly_lesser(hole.compute_area(), smallest_item_area))
                    continue;
                total_bin_area += bin_type.copies * hole.compute_area();
            }

            for (ShapePos hole_pos = 0;
                    hole_pos < defect.shape_inflated.holes.size();
                    ++hole_pos) {
                const Shape& hole_deflated = defect.shape_inflated.holes[hole_pos];

                if (strictly_lesser(hole_deflated.compute_area(), smallest_item_area))
                    continue;

                bool outer = false;
                SimplifyInputShape simplify_input_shape;
                simplify_input_shape.shape = hole_deflated;
                simplify_input_shape.copies = bin_type.copies;
                simplify_input_shape.outer = outer;
                simplify_inflated_bin_types_input.push_back(simplify_input_shape);

                ApproximatedShapeKey key;
                key.type = ApproximatedShapeType::DefectHoleDeflated;
                key.bin_type_id = bin_type_id;
                key.defect_id = defect_id;
                key.hole_pos = hole_pos;
                simplify_inflated_bin_types_keys.push_back(key);
            }
        }
    }
    // Add elements from item types.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);

        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            const ItemShape& item_shape = item_type.shapes[item_shape_pos];
            total_item_area += item_type.copies * item_shape.shape_scaled.shape.compute_area();
            //std::cout << "item_type_id " << item_type_id
            //    << " item_shape_pos " << item_shape_pos
            //    << std::endl;

            {
                bool outer = true;
                SimplifyInputShape simplify_input_shape;
                simplify_input_shape.shape = item_shape.shape_scaled.shape;
                simplify_input_shape.copies = item_type.copies;
                simplify_input_shape.outer = outer;
                simplify_item_types_input.push_back(simplify_input_shape);

                ApproximatedShapeKey key;
                key.type = ApproximatedShapeType::ItemShape;
                key.item_type_id = item_type_id;
                key.item_shape_pos = item_shape_pos;
                simplify_item_types_keys.push_back(key);
            }
            {
                bool outer = true;
                SimplifyInputShape simplify_input_shape;
                simplify_input_shape.shape = item_shape.shape_inflated.shape;
                simplify_input_shape.copies = item_type.copies;
                simplify_input_shape.outer = outer;
                simplify_inflated_item_types_input.push_back(simplify_input_shape);

                ApproximatedShapeKey key;
                key.type = ApproximatedShapeType::ItemShapeInflated;
                key.item_type_id = item_type_id;
                key.item_shape_pos = item_shape_pos;
                simplify_inflated_item_types_keys.push_back(key);
            }

            for (ShapePos hole_pos = 0;
                    hole_pos < item_shape.shape_scaled.holes.size();
                    ++hole_pos) {
                const Shape& hole = item_shape.shape_scaled.holes[hole_pos];

                if (strictly_lesser(hole.compute_area(), smallest_item_area))
                    continue;

                total_item_area -= item_type.copies * hole.compute_area();

                bool outer = false;
                SimplifyInputShape simplify_input_shape;
                simplify_input_shape.shape = hole;
                simplify_input_shape.copies = item_type.copies;
                simplify_input_shape.outer = outer;
                simplify_item_types_input.push_back(simplify_input_shape);

                ApproximatedShapeKey key;
                key.type = ApproximatedShapeType::ItemShapeHole;
                key.item_type_id = item_type_id;
                key.item_shape_pos = item_shape_pos;
                key.hole_pos = hole_pos;
                simplify_item_types_keys.push_back(key);
            }

            for (ShapePos hole_pos = 0;
                    hole_pos < item_shape.shape_inflated.holes.size();
                    ++hole_pos) {
                const Shape& hole_deflated = item_shape.shape_inflated.holes[hole_pos];

                if (strictly_lesser(hole_deflated.compute_area(), smallest_item_area))
                    continue;

                bool outer = false;
                SimplifyInputShape simplify_input_shape;
                simplify_input_shape.shape = hole_deflated;
                simplify_input_shape.copies = item_type.copies;
                simplify_input_shape.outer = outer;
                simplify_inflated_item_types_input.push_back(simplify_input_shape);

                ApproximatedShapeKey key;
                key.type = ApproximatedShapeType::ItemShapeHoleDeflated;
                key.item_type_id = item_type_id;
                key.item_shape_pos = item_shape_pos;
                key.hole_pos = hole_pos;
                simplify_inflated_item_types_keys.push_back(key);
            }
        }
    }

    AreaDbl maximum_approximation_area = maximum_approximation_ratio * (std::min)(total_bin_area, total_item_area);

    // Run shape simplification algorithm.
    std::vector<shape::ShapeWithHoles> simplify_inflated_bin_types_output
        = shape::simplify(simplify_inflated_bin_types_input, maximum_approximation_area);
    std::vector<shape::ShapeWithHoles> simplify_item_types_output
        = shape::simplify(simplify_item_types_input, maximum_approximation_area);
    std::vector<shape::ShapeWithHoles> simplify_inflated_item_types_output
        = shape::simplify(simplify_inflated_item_types_input, maximum_approximation_area);

    // Build simplified instance.
    SimplifiedInstance simplified_instance;
    simplified_instance.bin_types = std::vector<SimplifiedBinType>(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        simplified_instance.bin_types[bin_type_id].borders
            = std::vector<SimplifiedShape>(bin_type.borders.size());
        simplified_instance.bin_types[bin_type_id].defects
            = std::vector<SimplifiedShape>(bin_type.defects.size());
    }
    simplified_instance.item_types = std::vector<SimplifiedItemType>(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        simplified_instance.item_types[item_type_id].shapes
            = std::vector<SimplifiedShape>(item_type.shapes.size());
    }

    // Add bins.
    for (ShapePos shape_pos = 0;
            shape_pos < (ShapePos)simplify_inflated_bin_types_output.size();
            ++shape_pos) {
        const ApproximatedShapeKey& key = simplify_inflated_bin_types_keys[shape_pos];
        const ShapeWithHoles& simplified_shape = simplify_inflated_bin_types_output[shape_pos];

        if (key.type == ApproximatedShapeType::BorderInflated) {
            simplified_instance.bin_types[key.bin_type_id].borders[key.defect_id].shape_inflated.shape
                = simplified_shape.shape;
            for (const auto& s: simplified_shape.holes)
                simplified_instance.bin_types[key.bin_type_id].borders[key.defect_id].shape_inflated.holes.push_back(s);

        } else if (key.type == ApproximatedShapeType::DefectInflated) {
            simplified_instance.bin_types[key.bin_type_id].defects[key.defect_id].shape_inflated.shape
                = simplified_shape.shape;
            for (const auto& s: simplified_shape.holes)
                simplified_instance.bin_types[key.bin_type_id].defects[key.defect_id].shape_inflated.holes.push_back(s);

        } else if (key.type == ApproximatedShapeType::DefectHoleDeflated) {
            for (const auto& s: simplified_shape.holes)
                simplified_instance.bin_types[key.bin_type_id].defects[key.defect_id].shape_inflated.holes.push_back(s);
        }
    }

    // Add inflated items.
    for (ShapePos shape_pos = 0;
            shape_pos < (ShapePos)simplify_inflated_item_types_output.size();
            ++shape_pos) {
        const ApproximatedShapeKey& key = simplify_inflated_item_types_keys[shape_pos];
        const ShapeWithHoles& simplified_shape = simplify_inflated_item_types_output[shape_pos];
        //std::cout << "shape_pos " << shape_pos
        //    << " key.type " << (int)key.type
        //    << " key.item_type_id " << key.item_type_id
        //    << " key.item_shape_pos " << key.item_shape_pos
        //    << " key.hole_pos " << key.hole_pos
        //    << std::endl;
        //std::cout << "simplified_shape " << simplified_shape.shape.to_string(0) << std::endl;
        //for (const auto& s: simplified_shape.holes)
        //    std::cout << "simplified_shape " << s.to_string(0) << std::endl;

        if (key.type == ApproximatedShapeType::ItemShapeInflated) {
            simplified_instance.item_types[key.item_type_id].shapes[key.item_shape_pos].shape_inflated.shape
                = simplified_shape.shape;
            for (const auto& s: simplified_shape.holes)
                simplified_instance.item_types[key.item_type_id].shapes[key.item_shape_pos].shape_inflated.holes.push_back(s);

        } else if (key.type == ApproximatedShapeType::ItemShapeHoleDeflated) {
            for (const auto& s: simplified_shape.holes)
                simplified_instance.item_types[key.item_type_id].shapes[key.item_shape_pos].shape_inflated.holes.push_back(s);
        }
    }

    // Add items.
    for (ShapePos shape_pos = 0;
            shape_pos < (ShapePos)simplify_item_types_output.size();
            ++shape_pos) {
        const ApproximatedShapeKey& key = simplify_item_types_keys[shape_pos];
        const ShapeWithHoles& simplified_shape = simplify_item_types_output[shape_pos];

        if (key.type == ApproximatedShapeType::ItemShape) {
            simplified_instance.item_types[key.item_type_id].shapes[key.item_shape_pos].shape.shape
                = simplified_shape.shape;
            for (const auto& s: simplified_shape.holes)
                simplified_instance.item_types[key.item_type_id].shapes[key.item_shape_pos].shape.holes.push_back(s);

            //const ItemType& item_type = instance.item_type(key.item_type_id);
            //const ItemShape& item_shape = item_type.shapes[key.item_shape_pos];
            //std::cout << "shape_pos " << shape_pos << std::endl;
            //std::cout << "outer " << simplify_item_types_input[shape_pos].outer << std::endl;
            //auto mm1 = item_shape.shape_orig.compute_min_max();
            //auto mm2 = item_shape.shape_scaled.compute_min_max();
            //auto mm3 = simplify_item_types_input[shape_pos].shape.compute_min_max();
            //auto mm4 = simplified_shape.shape.compute_min_max();
            //std::cout << "orig " << mm1.second.to_string() << std::endl;
            //std::cout << "scal " << mm2.second.to_string() << std::endl;
            //std::cout << "clea " << mm3.second.to_string() << std::endl;
            //std::cout << "simp " << mm4.second.to_string() << std::endl;

        } else if (key.type == ApproximatedShapeType::ItemShapeHole) {
            for (const auto& s: simplified_shape.holes)
                simplified_instance.item_types[key.item_type_id].shapes[key.item_shape_pos].shape.holes.push_back(s);
        }
    }

    return simplified_instance;
}

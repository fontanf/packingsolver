#pragma once

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

enum class CutType1 { ThreeStagedGuillotine, TwoStagedGuillotine };
enum class CutType2 { Roadef2018, NonExact, Exact, Homogenous };
enum class CutOrientation { Horinzontal, Vertical, Any };

std::istream& operator>>(std::istream& in, CutType1& cut_type_1);
std::istream& operator>>(std::istream& in, CutType2& cut_type_2);
std::istream& operator>>(std::istream& in, CutOrientation& o);

std::ostream& operator<<(std::ostream &os, CutType1 cut_type_1);
std::ostream& operator<<(std::ostream &os, CutType2 cut_type_2);
std::ostream& operator<<(std::ostream &os, CutOrientation o);

struct Coord
{
    Length x;
    Length y;
};

std::ostream& operator<<(std::ostream &os, Coord xy);

struct Rectangle
{
    Length w;
    Length h;

    Area area() const { return w * h; }
};

bool rect_intersection(Coord c1, Rectangle r1, Coord c2, Rectangle r2);

std::ostream& operator<<(std::ostream &os, Rectangle r);

/******************************************************************************/

struct ItemType
{
    ItemTypeId id;
    Profit profit;
    ItemPos copies;
    Rectangle rect;
    StackId stack;
    bool oriented;
};

std::ostream& operator<<(std::ostream &os, const ItemType& item_type);

struct Defect
{
    DefectId id;
    BinTypeId bin_id;
    Coord pos;
    Rectangle rect;
};

std::ostream& operator<<(std::ostream &os, const Defect& defect);

struct BinType
{
    BinTypeId id;
    Profit cost;
    BinPos copies;
    BinPos copies_min;

    Rectangle rect;
    std::vector<Defect> defects;

    Area previous_bin_area = 0;
    BinPos previous_bin_copies = 0;

    Length  width(CutOrientation o) const { return (o == CutOrientation::Vertical)? rect.w: rect.h; }
    Length height(CutOrientation o) const { return (o == CutOrientation::Vertical)? rect.h: rect.w; }
};

std::ostream& operator<<(std::ostream &os, const BinType& bin_type);

/********************************** Instance **********************************/

class Instance
{

public:

    /**
     * Create instances from files
     */
    Instance(
            Objective objective,
            std::string items_filepath,
            std::string bins_filepath,
            std::string defects_filepath);

    /**
     * Create instance manually
     */
    Instance(Objective objective): objective_(objective) { }
    void add_item_type(Length w, Length h, Profit p = -1, ItemPos copies = 1, bool oriented = false, bool new_stack = true);
    void add_bin_type(Length w, Length h, Profit cost = -1, BinPos copies = 1, BinPos copies_min = 0);
    void add_defect(BinTypeId i, Length x, Length y, Length w, Length h);

    inline void add_bin_type(const BinType& bin_type, BinPos copies, BinPos copies_min = 0)
    {
        add_bin_type(bin_type.rect.w, bin_type.rect.h, bin_type.cost, copies, copies_min);
    }

    inline void add_item_type(const ItemType& item_type, Profit profit, ItemPos copies)
    {
        add_item_type(item_type.rect.w, item_type.rect.h, profit, copies, item_type.oriented);
    }

    void set_bin_infinite_width();
    void set_bin_infinite_height();
    void set_bin_infinite_copies();
    void set_bin_unweighted();
    void set_item_infinite_copies();
    void set_unweighted();

    /**
     * Getters
     */

    inline ProblemType type() const { return ProblemType::RectangleGuillotine; };
    inline Objective objective() const { return objective_; }

    inline ItemTypeId item_type_number()    const { return item_types_.size(); }
    inline ItemTypeId item_number()         const { return item_number_; }
    inline StackId    stack_number()        const { return stacks_.size(); }
    inline ItemPos    stack_size(StackId s) const { return stack_sizes_[s]; }
    inline DefectId   defect_number()       const { return defects_.size(); }
    inline BinTypeId  bin_type_number()     const { return bin_types_.size(); }
    inline BinPos     bin_number()          const { return bin_number_; }

    inline Area       item_area()           const { return item_area_; }
    inline Area       mean_area()           const { return item_area_ / item_number(); }
    inline Area       defect_area()         const { return defect_area_; }
    inline Area       packable_area()       const { return packable_area_; }
    inline Profit     item_profit()         const { return item_profit_; }
    inline ItemTypeId max_efficiency_item() const { return max_efficiency_item_; }
    inline bool       unbounded_knapsck()   const { return all_item_type_infinite_copies; }

    inline const ItemType&  item_type(ItemTypeId j) const { return item_types_[j]; }
    inline const Defect&         defect(DefectId k) const { return defects_[k]; }
    inline const BinType&     bin_type(BinTypeId i) const { return bin_types_[i]; }

    inline const ItemType& item(StackId s, ItemPos j_pos) const;
    inline const BinType& bin(BinPos i_pos) const;
    Area previous_bin_area(BinPos i_pos) const;

    inline const std::vector<ItemType>&              item_types()     const { return item_types_; }
    inline const std::vector<ItemType>&              stack(StackId s) const { return stacks_[s]; }
    inline const std::vector<std::vector<ItemType>>& stacks()         const { return stacks_; }
    inline const std::vector<Defect>&                defects()        const { return defects_; }

    inline Length  width(const ItemType& item, bool rotate, CutOrientation o) const;
    inline Length height(const ItemType& item, bool rotate, CutOrientation o) const;

    inline Length   left(const Defect& defect, CutOrientation o) const;
    inline Length  right(const Defect& defect, CutOrientation o) const;
    inline Length    top(const Defect& defect, CutOrientation o) const;
    inline Length bottom(const Defect& defect, CutOrientation o) const;

    DefectId rect_intersects_defect(
            Length l, Length r, Length b, Length t, BinTypeId i, CutOrientation o) const;
    DefectId item_intersects_defect(
            Length l, Length b, const ItemType& item, bool rotate, BinTypeId i, CutOrientation o) const;
    DefectId x_intersects_defect(
            Length x, BinTypeId i, CutOrientation o) const;
    DefectId y_intersects_defect(
            Length l, Length r, Length y, BinTypeId i, CutOrientation o) const;

    Counter state_number() const;

    void write(std::string filepath) const;

private:

    std::vector<ItemType> item_types_;
    std::vector<Defect> defects_;
    std::vector<BinType> bin_types_;
    Objective objective_;

    std::vector<std::vector<ItemType>> stacks_;

    ItemPos item_number_ = 0;
    BinPos bin_number_ = 0;
    std::vector<ItemPos> stack_sizes_;
    Length length_sum_ = 0;
    Area item_area_ = 0;
    Area defect_area_ = 0;
    Area packable_area_ = 0;
    Profit item_profit_ = 0;
    ItemTypeId max_efficiency_item_ = -1;

    bool all_bin_type_one_copy_ = true;
    bool all_item_type_one_copy_ = true;
    bool all_item_type_infinite_copies = false;

};

std::ostream& operator<<(std::ostream &os, const Instance& ins);

/****************************** inlined methods *******************************/

const ItemType& Instance::item(StackId s, ItemPos j_pos) const
{
    assert(j_pos < stack_sizes_[s]);

    if (all_item_type_one_copy_)
        return stacks_[s][j_pos];

    ItemPos j_tmp = 0;
    ItemTypeId j = 0;
    for (;;) {
        if (j_tmp <= j_pos && j_pos < j_tmp + stacks_[s][j].copies) {
            return stacks_[s][j];
        } else {
            j_tmp += stacks_[s][j].copies;
            j++;
        }
    }
}

const BinType& Instance::bin(BinPos i_pos) const
{
    assert(i_pos < bin_number_);

    if (all_bin_type_one_copy_)
        return bin_types_[i_pos];

    BinPos i_tmp = 0;
    BinTypeId i = 0;
    for (;;) {
        if (i_tmp <= i_pos && i_pos < i_tmp + bin_types_[i].copies) {
            return bin_types_[i];
        } else {
            i_tmp += bin_types_[i].copies;
            i++;
        }
    }
}

Length Instance::left(const Defect& defect, CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?  defect.pos.x: defect.pos.y;
}

Length Instance::right(const Defect& defect, CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        defect.pos.x + defect.rect.w:
        defect.pos.y + defect.rect.h;
}

Length Instance::top(const Defect& defect, CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        defect.pos.y + defect.rect.h:
        defect.pos.x + defect.rect.w;
}

Length Instance::bottom(const Defect& defect, CutOrientation o) const
{
    return (o == CutOrientation::Vertical)? defect.pos.y: defect.pos.x;
}

Length Instance::width(const ItemType& item, bool rotate, CutOrientation o) const
{
    if (o == CutOrientation::Vertical) {
        return (!rotate)? item.rect.w: item.rect.h;
    } else {
        return (!rotate)? item.rect.h: item.rect.w;
    }
}

Length Instance::height(const ItemType& item, bool rotate, CutOrientation o) const
{
    if (o == CutOrientation::Vertical) {
        return (!rotate)? item.rect.h: item.rect.w;
    } else {
        return (!rotate)? item.rect.w: item.rect.h;
    }
}

}
}


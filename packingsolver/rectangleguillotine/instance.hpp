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

struct Item
{
    ItemTypeId id;
    Rectangle rect;
    Profit profit;
    ItemPos copies;
    StackId stack;
    bool oriented;
};

std::ostream& operator<<(std::ostream &os, const Item& item);

struct Defect
{
    DefectId id;
    BinTypeId bin_id;
    Coord pos;
    Rectangle rect;
};

std::ostream& operator<<(std::ostream &os, const Defect& defect);

struct Bin
{
    BinTypeId id;
    Rectangle rect;
    BinPos copies;
    std::vector<Defect> defects;

    Area previous_bin_area = 0;
    BinPos previous_bin_copies = 0;

    Length  width(CutOrientation o) const { return (o == CutOrientation::Vertical)? rect.w: rect.h; }
    Length height(CutOrientation o) const { return (o == CutOrientation::Vertical)? rect.h: rect.w; }
};

std::ostream& operator<<(std::ostream &os, const Bin& bin);

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
    void add_item(Length w, Length h, Profit p = -1, ItemPos copies = 1, bool oriented = false, bool new_stack = true);
    void add_bin(Length w, Length h, BinPos copies = 1);
    void add_defect(BinTypeId i, Length x, Length y, Length w, Length h);

    void set_bin_infinite_width();
    void set_bin_infinite_height();
    void set_bin_infinite_copies();
    void set_item_infinite_copies();
    void set_unweighted();

    /**
     * Getters
     */

    inline Objective objective() const { return objective_; }

    inline ItemTypeId item_type_number()    const { return items_.size(); }
    inline ItemTypeId item_number()         const { return item_number_; }
    inline StackId    stack_number()        const { return stacks_.size(); }
    inline ItemPos    stack_size(StackId s) const { return stack_sizes_[s]; }
    inline DefectId   defect_number()       const { return defects_.size(); }
    inline BinTypeId  bin_type_number()     const { return bins_.size(); }
    inline BinPos     bin_number()          const { return bin_number_; }

    inline Area       item_area()           const { return item_area_; }
    inline Area       mean_area()           const { return item_area_ / item_number(); }
    inline Area       defect_area()         const { return defect_area_; }
    inline Area       packable_area()       const { return packable_area_; }
    inline Profit     item_profit()         const { return item_profit_; }
    inline ItemTypeId max_efficiency_item() const { return max_efficiency_item_; }

    inline const Item&   item(ItemTypeId j)    const { return items_[j]; }
    inline const Defect& defect(DefectId k)    const { return defects_[k]; }
    inline const Bin&    bin_type(BinTypeId i) const { return bins_[i]; }

    inline const Item& item(StackId s, ItemPos j_pos) const;
    inline const Bin& bin(BinPos i_pos) const;
    Area previous_bin_area(BinPos i_pos) const;

    inline const std::vector<Item>&              items()          const { return items_; }
    inline const std::vector<Item>&              stack(StackId s) const { return stacks_[s]; }
    inline const std::vector<std::vector<Item>>& stacks()         const { return stacks_; }
    inline const std::vector<Defect>&            defects()        const { return defects_; }

    inline Length  width(const Item& item, bool rotate, CutOrientation o) const;
    inline Length height(const Item& item, bool rotate, CutOrientation o) const;

    inline Length   left(const Defect& defect, CutOrientation o) const;
    inline Length  right(const Defect& defect, CutOrientation o) const;
    inline Length    top(const Defect& defect, CutOrientation o) const;
    inline Length bottom(const Defect& defect, CutOrientation o) const;

    DefectId rect_intersects_defect(
            Length l, Length r, Length b, Length t, BinTypeId i, CutOrientation o) const;
    DefectId item_intersects_defect(
            Length l, Length b, const Item& item, bool rotate, BinTypeId i, CutOrientation o) const;
    DefectId x_intersects_defect(
            Length x, BinTypeId i, CutOrientation o) const;
    DefectId y_intersects_defect(
            Length l, Length r, Length y, BinTypeId i, CutOrientation o) const;

    Counter state_number() const;

    void write(std::string filepath) const;

private:

    std::vector<Item> items_;
    std::vector<Defect> defects_;
    std::vector<Bin> bins_;
    Objective objective_;

    std::vector<std::vector<Item>> stacks_;

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

};

std::ostream& operator<<(std::ostream &os, const Instance& ins);

/****************************** inlined methods *******************************/

const Item& Instance::item(StackId s, ItemPos j_pos) const
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

const Bin& Instance::bin(BinPos i_pos) const
{
    assert(i_pos < bin_number_);

    if (all_bin_type_one_copy_)
        return bins_[i_pos];

    BinPos i_tmp = 0;
    BinTypeId i = 0;
    for (;;) {
        if (i_tmp <= i_pos && i_pos < i_tmp + bins_[i].copies) {
            return bins_[i];
        } else {
            i_tmp += bins_[i].copies;
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

Length Instance::width(const Item& item, bool rotate, CutOrientation o) const
{
    if (o == CutOrientation::Vertical) {
        return (!rotate)? item.rect.w: item.rect.h;
    } else {
        return (!rotate)? item.rect.h: item.rect.w;
    }
}

Length Instance::height(const Item& item, bool rotate, CutOrientation o) const
{
    if (o == CutOrientation::Vertical) {
        return (!rotate)? item.rect.h: item.rect.w;
    } else {
        return (!rotate)? item.rect.w: item.rect.h;
    }
}

}
}


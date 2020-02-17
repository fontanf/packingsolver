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
    BinPos bin_id;
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
            std::string items_filename,
            std::string bins_filename,
            std::string defects_filename);

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

    const Item& item(StackId s, ItemPos j_pos) const;
    const Bin& bin(BinPos i_pos) const;
    Area previous_bin_area(BinPos i_pos) const;

    inline const std::vector<Item>&              items()          const { return items_; }
    inline const std::vector<Item>&              stack(StackId s) const { return stacks_[s]; }
    inline const std::vector<std::vector<Item>>& stacks()         const { return stacks_; }
    inline const std::vector<Defect>&            defects()        const { return defects_; }

    Length  width(const Item& item, bool rotate, CutOrientation o) const;
    Length height(const Item& item, bool rotate, CutOrientation o) const;

    Length   left(const Defect& defect, CutOrientation o) const;
    Length  right(const Defect& defect, CutOrientation o) const;
    Length    top(const Defect& defect, CutOrientation o) const;
    Length bottom(const Defect& defect, CutOrientation o) const;

    DefectId rect_intersects_defect(
            Length l, Length r, Length b, Length t, BinTypeId i, CutOrientation o) const;
    DefectId x_intersects_defect(
            Length x, BinTypeId i, CutOrientation o) const;
    DefectId y_intersects_defect(
            Length l, Length r, Length y, BinTypeId i, CutOrientation o) const;

    Counter state_number() const;

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

}
}


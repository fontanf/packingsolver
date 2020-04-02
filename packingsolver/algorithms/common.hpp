#pragma once

#include <cstdint>

#include "optimizationtools/info.hpp"

namespace packingsolver
{

typedef int16_t ItemTypeId;
typedef int16_t ItemPos;
typedef int64_t Length;
typedef int64_t Area;
typedef int16_t StackId;
typedef int64_t Profit;
typedef int16_t DefectId;
typedef int16_t BinTypeId;
typedef int16_t BinPos;
typedef int16_t Depth;
typedef int64_t Seed;
typedef int16_t GuideId;
typedef int64_t Counter;

using optimizationtools::Info;

enum class ProblemType { RectangleGuillotine, Rectangle };
enum class Objective { Default, BinPacking, StripPacking, Knapsack, BinPackingLeftovers };

std::istream& operator>>(std::istream& in, ProblemType& problem_type);
std::istream& operator>>(std::istream& in, Objective& objective);
std::ostream& operator<<(std::ostream &os, ProblemType problem_type);
std::ostream& operator<<(std::ostream &os, Objective objective);

}


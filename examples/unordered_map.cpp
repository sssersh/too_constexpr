#include "cant.h"
#include "constexpr_std/unordered_map"

#if __clang__

static constexpr auto constexpr_unordered_map = cant::too_constexpr(
        []() -> std::unordered_map<int, int>
        {
            return { {1,11}, {2, 12}, {3, 13}, {4, 14}, {5, 15}, {6, 16}, {7, 17}, {8, 18}, {9, 19} };
        });
static_assert(constexpr_unordered_map.at(2) == 12, "Error");

#else
#warning "Example with constexpr std::unordered_map work only on clang"
#endif
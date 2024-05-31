
#include "cant.h"

#include <vector>

constexpr static auto constexpr_vector =
    cant::too_constexpr(
        []() -> std::vector<int>
        {
            return { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        }
    );

auto dummy()
{
    return constexpr_vector;
}

static_assert(constexpr_vector[0] == 1, "error");

using constexpr_vector_t = std::remove_cvref_t<decltype(constexpr_vector)>;
using magic_vector_allocator_t = decltype(constexpr_vector)::allocator_type;

static_assert(
    std::is_same_v<
        constexpr_vector_t,
        std::vector<int, magic_vector_allocator_t>
    >,
    "type of constexpr_vector is not std::vector specialization"
);

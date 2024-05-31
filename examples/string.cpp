
#include "cant.h"

#include <string>

constexpr auto constexpr_string = cant::too_constexpr(
        []() -> std::string
        {
            return "Toooooo long string just for example";
        }
);

static_assert(constexpr_string == "Toooooo long string just for example", "Strings are not same");

using constexpr_string_t = std::remove_cvref_t<decltype(constexpr_string)>;
using magic_string_allocator_t = decltype(constexpr_string)::allocator_type;

static_assert(
        std::is_same_v<
            constexpr_string_t,
            std::basic_string<char, std::char_traits<char>, magic_string_allocator_t>
        >,
        "type of constexpr_string is not std::basic_string specialization"
);

template<typename T1, typename T2>
constexpr std::string concat(const T1& s1, const T2& s2)
{
    std::string result;

    result += s1;
    result += s2;

    return result;
}

constexpr std::string_view concat1 = "It is string which was builded ";
constexpr std::string_view concat2 = "as concatenation of two strings";
constexpr std::string_view concat3 = ", no, as concatenation of 3 strings";

constexpr auto concat_result_1 = cant::too_constexpr(
        []() -> std::string
        {
            return std::string(concat1) + std::string(concat2);
        }
);

constexpr auto concat_result_full = cant::too_constexpr(
        []() -> std::string
        {
            return concat(concat_result_1, concat3);
        }
);

static_assert(concat_result_1 == "It is string which was builded as concatenation of two strings", "Strings are not same");
static_assert(concat_result_full == "It is string which was builded as concatenation of two strings, no, as concatenation of 3 strings", "Strings are not same");

constexpr std::string_view string_for_remove = "It is string which builded (remove me)as constexpr";

constexpr std::string remove_trash(std::string_view origin)
{
    std::string result = std::string{ string_for_remove.begin(), string_for_remove.end() };
    std::string_view for_remove = "(remove me)";

    auto it = result.find(for_remove);
    if (it != std::string::npos)
    {
        result.erase(it, for_remove.size());
    }

    return result;
}

constexpr auto remove_result = cant::too_constexpr([]() { return remove_trash(string_for_remove); });
static_assert(remove_result == "It is string which builded as constexpr", "Strings are not same");

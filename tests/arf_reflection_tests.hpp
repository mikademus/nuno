#ifndef ARF_TESTS_REFLECTION__
#define ARF_TESTS_REFLECTION__

/* *************************************************************************
 *  This test suite enforces semantic contracts of the ARF reflection layer *
 *  Test names describe address semantics, not implementation behaviour.    *
 ************************************************************************* */

#include "arf_test_harness.hpp"
#include "../include/arf.hpp"
#include "../include/arf_reflect.hpp"

namespace arf::tests
{
    #define CAT(x)   arf::reflect::category_step{x}
    #define KEY(x)   arf::reflect::key_step{x}
    #define INDEX(x) arf::reflect::array_index_step{x}
    #define TABLE(x) arf::reflect::table_step{x}
    #define ROW(x)   arf::reflect::row_step{x}
    #define COL(x)   arf::reflect::column_step{x}

//------------------------------------------------------------
// Address resolution basics
//------------------------------------------------------------

static bool reflect_empty_address_yields_no_value()
{
    constexpr std::string_view src =
        "a:\n"
        "  x = 1\n";

    auto ctx = load(src);

    arf::reflect::address addr;
    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(!v.has_value(), "empty address must not resolve to a value");
    return true;
}

//------------------------------------------------------------
// Category navigation
//------------------------------------------------------------

static bool reflect_category_navigation_succeeds()
{
    constexpr std::string_view src =
        "a:\n"
        "  b:\n"
        "    x = 1\n";

    auto ctx = load(src);

    arf::reflect::address addr;
    addr / CAT("a") / CAT("b") / KEY("x");

    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(v.has_value() && v.value() != nullptr, "failed to resolve nested category key");
    EXPECT((**v).type == value_type::integer, "wrong value type");
    return true;
}

static bool reflect_invalid_category_fails()
{
    constexpr std::string_view src =
        "a:\n"
        "  x = 1\n";

    auto ctx = load(src);

    arf::reflect::address addr;
    addr / CAT("nope") / KEY("x");

    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(!v.has_value(), "invalid category should not resolve");
    return true;
}

//------------------------------------------------------------
// Key access
//------------------------------------------------------------

static bool reflect_key_access_succeeds()
{
    constexpr std::string_view src =
        "a:\n"
        "  x = 42\n";

    auto ctx = load(src);

    arf::reflect::address addr;
    addr / CAT("a") / KEY("x");

    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(v.has_value(), "key not resolved");
    EXPECT((**v).type == value_type::integer, "wrong key type");
    return true;
}

static bool reflect_invalid_key_fails()
{
    constexpr std::string_view src =
        "a:\n"
        "  x = 1\n";

    auto ctx = load(src);

    arf::reflect::address addr;
    addr / CAT("a") / KEY("y");

    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(!v.has_value(), "invalid key should not resolve");
    return true;
}

//------------------------------------------------------------
// Table navigation
//------------------------------------------------------------

static bool reflect_table_by_ordinal_succeeds()
{
    constexpr std::string_view src =
        "a:\n"
        " # x\n"
        "   1\n";

    auto ctx = load(src);

    arf::reflect::address addr;
    addr / CAT("a") / TABLE(0);

    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(!v.has_value(), "table step alone must not resolve to a value");
    return true;
}

static bool reflect_invalid_table_ordinal_fails()
{
    constexpr std::string_view src =
        "a:\n"
        "  # x\n"
        "    1\n";

    auto ctx = load(src);

    arf::reflect::address addr;
    addr / CAT("a") / TABLE(1);

    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(!v.has_value(), "invalid table ordinal should fail");
    return true;
}

//------------------------------------------------------------
// Row and column navigation
//------------------------------------------------------------

static bool reflect_cell_value_succeeds()
{
    constexpr std::string_view src =
        "a:\n"
        "  # x  y\n"
        "    1  2\n";

    auto ctx = load(src);

    auto table = ctx.document.category("a")->tables()[0];
    auto row_id = ctx.document.table(table)->rows()[0];

    arf::reflect::address addr;
    addr / CAT("a") / TABLE(0) / ROW(row_id) / COL("y");

    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(v.has_value(), "cell value not resolved");
    EXPECT((**v).type == value_type::integer, "wrong cell type");
    return true;
}

static bool reflect_invalid_column_fails()
{
    constexpr std::string_view src =
        "a:\n"
        "  # x\n"
        "    1\n";

    auto ctx = load(src);

    auto table = ctx.document.category("a")->tables()[0];
    auto row_id = ctx.document.table(table)->rows()[0];

    arf::reflect::address addr;
    addr / CAT("a") / TABLE(0) / ROW(row_id) / COL("nope");

    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(!v.has_value(), "invalid column should not resolve");
    return true;
}

//------------------------------------------------------------
// Array indexing
//------------------------------------------------------------

static bool reflect_array_index_succeeds()
{
    constexpr std::string_view src =
        "a:\n"
        "  x:int[] = 1|2|3\n";

    auto ctx = load(src);

    arf::reflect::address addr;
    addr / CAT("a") / KEY("x") / INDEX(1);

    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(v.has_value(), "array element not resolved");
    EXPECT((**v).type == value_type::integer, "wrong array element type");
    return true;
}

static bool reflect_array_index_out_of_range_fails()
{
    constexpr std::string_view src =
        "a:\n"
        "  x:int[] = 1\n";

    auto ctx = load(src);

    arf::reflect::address addr;
    addr / CAT("a") / KEY("x") / INDEX(5);

    auto v = arf::reflect::resolve(ctx.document, addr);

    EXPECT(!v.has_value(), "out-of-range array index should fail");
    return true;
}

//------------------------------------------------------------
// Runner
//------------------------------------------------------------

inline void run_reflection_tests()
{
SUBCAT("Reflection");
    RUN_TEST(reflect_empty_address_yields_no_value);
    RUN_TEST(reflect_category_navigation_succeeds);
    RUN_TEST(reflect_invalid_category_fails);
    RUN_TEST(reflect_key_access_succeeds);
    RUN_TEST(reflect_invalid_key_fails);
    RUN_TEST(reflect_table_by_ordinal_succeeds);
    RUN_TEST(reflect_invalid_table_ordinal_fails);
    RUN_TEST(reflect_cell_value_succeeds);
    RUN_TEST(reflect_invalid_column_fails);
    RUN_TEST(reflect_array_index_succeeds);
    RUN_TEST(reflect_array_index_out_of_range_fails);
}

} // namespace arf::tests

#endif // ARF_TESTS_REFLECTION__

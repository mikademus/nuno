// arf_tests.cpp - Comprehensive Test Suite for Arf!
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licensed as-is under the MIT license.

#include "include/arf_document.hpp"
#include <iostream>

using namespace arf;

static int failures = 0;

#define EXPECT(cond)                                                     \
    do                                                                   \
    {                                                                    \
        if (!(cond))                                                     \
        {                                                                \
            ++failures;                                                  \
            std::cerr << "FAILED: " << __FILE__ << ":" << __LINE__        \
                      << " -> " #cond << "\n";                           \
        }                                                                \
    } while (0)

//------------------------------------------------------------

static void test_root_exists()
{
    auto doc = load("");
    EXPECT(doc.has_value());
    EXPECT(doc->category_count() >= 1);
    EXPECT(doc->root().has_value());
}

static void test_simple_category()
{
    constexpr std::string_view src =
        "category:\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    EXPECT(doc->category_count() == 2); // root + category

    auto root = doc->root();
    EXPECT(root.has_value());
}

static void test_nested_categories()
{
    constexpr std::string_view src =
        "outer:\n"
        "    :inner\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    EXPECT(doc->category_count() == 3); // root, outer, inner
}

static void test_category_implicit_closure()
{
    constexpr std::string_view src =
        "a:\n"
        "    :b\n"
        ":c\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    // root, a, b, c
    EXPECT(doc->category_count() == 4);
}

static void test_simple_table()
{
    constexpr std::string_view src =
        "data:\n"
        "    # a  b\n"
        "      1  2\n"
        "      3  4\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    EXPECT(doc->table_count() == 1);
    EXPECT(doc->row_count() == 2);
}

static void test_table_in_subcategory()
{
    constexpr std::string_view src =
        "top:\n"
        "    :sub\n"
        "        # x y\n"
        "          a b\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    EXPECT(doc->table_count() == 1);
    EXPECT(doc->row_count() == 1);
}

//------------------------------------------------------------

int main()
{
    test_root_exists();
    test_simple_category();
    test_nested_categories();
    test_category_implicit_closure();
    test_simple_table();
    test_table_in_subcategory();

    if (failures == 0)
    {
        std::cout << "All foundation tests passed.\n";
        return 0;
    }

    std::cerr << failures << " test(s) failed.\n";
    return 1;
}

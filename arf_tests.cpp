// arf_tests.cpp - Comprehensive Test Suite for Arf!
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licensed as-is under the MIT license.

#include "include/arf_document.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace arf;

struct test_result
{
    std::string name;
    bool passed;
};

static std::vector<test_result> results;

#define EXPECT(cond)                                   \
    do                                                 \
    {                                                  \
        if (!(cond))                                   \
            return false;                              \
    } while (0)

#define RUN_TEST(fn)                                   \
    do                                                 \
    {                                                  \
        bool ok = fn();                                \
        results.push_back({ #fn, ok });                \
        std::cout << (ok ? "[PASS] " : "[FAIL] ")      \
                  << #fn << "\n";                      \
    } while (0)


//------------------------------------------------------------

static bool test_parser_marks_tacit_types_unresolved()
{
    constexpr std::string_view src =
        "a = 42\n"
        "# x y\n"
        "  1 2\n";

    auto ctx = parse(src);
    EXPECT(ctx.errors.empty());

    // Key
    auto& ev = find_event(ctx, parse_event_kind::key_value);
    EXPECT(ev.declared_type == std::nullopt);
    EXPECT(ev.value_type == value_type::unresolved);

    // Columns
    auto cols = extract_table_columns(ctx);
    EXPECT(cols[0].type == value_type::unresolved);
    EXPECT(cols[1].type == value_type::unresolved);

    return true;
}

static bool test_parser_records_declared_types_without_validation()
{
    constexpr std::string_view src =
        "x:int = hello\n"
        "# a:float\n"
        "  world\n";

    auto ctx = parse(src);
    EXPECT(ctx.errors.empty()); // no semantic checks here

    return true;
}

static bool test_root_exists()
{
    auto doc = load("");
    EXPECT(doc.has_value());
    EXPECT(doc->category_count() >= 1);
    EXPECT(doc->root().has_value());
    return true;
}

static bool test_simple_category()
{
    constexpr std::string_view src =
        "category:\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    EXPECT(doc->category_count() == 2); // root + category

    auto root = doc->root();
    EXPECT(root.has_value());
    return true;
}

static bool test_nested_categories()
{
    constexpr std::string_view src =
        "outer:\n"
        "    :inner\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    EXPECT(doc->category_count() == 3); // root, outer, inner
    return true;
}

static bool test_category_implicit_closure()
{
    constexpr std::string_view src =
        "a:\n"
        "    :b\n"
        ":c\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    // root, a, b, c
    EXPECT(doc->category_count() == 4);
    return true;
}

static bool test_simple_table()
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
    return true;
}

static bool test_table_in_subcategory()
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
    return true;
}

static bool test_multiple_root_tables()
{
    constexpr std::string_view src =
        "# a b\n"
        "  1 2\n"
        "\n"
        "# x y\n"
        "  3 4\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    EXPECT(doc->table_count() == 2);
    EXPECT(doc->row_count() == 2);

    auto root = doc->root();
    EXPECT(root.has_value());
    EXPECT(root->tables().size() == 2);
    return true;
}

static bool test_keys_and_tables_interleaved()
{
    constexpr std::string_view src =
        "top:\n"
        "    a = 1\n"
        "    # x y\n"
        "      2 3\n"
        "    b = 4\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    auto root = doc->root();
    EXPECT(root.has_value());

    EXPECT(doc->table_count() == 1);
    EXPECT(doc->row_count() == 1);

    // category access by traversal, not name (yet)
    EXPECT(doc->category_count() == 2);
    return true;
}

static bool test_duplicate_key_rejected()
{
    constexpr std::string_view src =
        "a = 1\n"
        "a = 2\n";

    auto doc = load(src);
    EXPECT(!doc.has_value());
    return true;
}

static bool test_same_key_in_different_categories_allowed()
{
    constexpr std::string_view src =
        "a = 1\n"
        "cat:\n"
        "    a = 2\n";

    auto doc = load(src);
    EXPECT(doc.has_value());
    return true;
}

static bool test_root_and_subcategory_keys()
{
    constexpr std::string_view src =
        "x = 1\n"
        "c:\n"
        "    y = 2\n";

    auto doc = load(src);
    EXPECT(doc.has_value());
    EXPECT(doc->category_count() == 2);
    return true;
}

static bool test_deep_implicit_closure()
{
    constexpr std::string_view src =
        "a:\n"
        "  :b\n"
        "    :c\n"
        "d:\n";

    auto doc = load(src);
    EXPECT(doc.has_value());
    EXPECT(doc->category_count() == 5); // root + a + b + c + d
    return true;
}

static bool test_key_type_inference()
{
    constexpr std::string_view src =
        "a = 42\n"
        "b = 3.14\n"
        "c = hello\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    auto root = doc->root();
    EXPECT(root.has_value());

    auto key_ids = root->keys();
    EXPECT(key_ids.size() == 3);

    auto k0 = doc->key(key_ids[0]);
    auto k1 = doc->key(key_ids[1]);
    auto k2 = doc->key(key_ids[2]);

    EXPECT(k0.has_value());
    EXPECT(k1.has_value());
    EXPECT(k2.has_value());

    EXPECT(k0->value().type == value_type::integer);
    EXPECT(k1->value().type == value_type::decimal);
    EXPECT(k2->value().type == value_type::string);

    return true;
}

static bool test_declared_key_type()
{
    constexpr std::string_view src =
        "x:float = 42\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    auto root = doc->root();
    EXPECT(root.has_value());

    auto key_ids = root->keys();
    EXPECT(key_ids.size() == 1);

    auto key = doc->key(key_ids.front());
    EXPECT(key.has_value());

    EXPECT(key->value().type == value_type::decimal);
    EXPECT(key->value().type_source == type_ascription::declared);

    return true;
}

static bool test_table_column_inference()
{
    constexpr std::string_view src =
        "# a  b\n"
        "  1  hello\n";

    auto doc = load(src);
    EXPECT(doc.has_value());
    EXPECT(doc->table_count() == 1);

    auto tbl = doc->table(table_id{0});
    EXPECT(tbl.has_value());

    auto cols = tbl->columns();
    EXPECT(cols[0].type == value_type::integer);
    EXPECT(cols[1].type == value_type::string);
    return true;
}

static bool test_declared_table_column()
{
    constexpr std::string_view src =
        "# a:int  b\n"
        "  3.14   hello\n";

    auto doc = load(src);
    auto tbl = doc->table(table_id{0});

    auto cols = tbl->columns();
    EXPECT(cols[0].type == value_type::integer);
    EXPECT(cols[0].type_source == type_ascription::declared);
    return true;
}

static bool test_declared_key_type_mismatch()
{
    constexpr std::string_view src =
        "x:int = hello\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors());
    return true;
}

static bool test_declared_column_type_mismatch()
{
    constexpr std::string_view src =
        "# a:int\n"
        "  hello\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors());
    EXPECT(ctx.errors[0].kind == semantic_error_kind::type_mismatch);
    return true;
}

static bool test_named_collapse_closes_multiple_scopes()
{
    constexpr std::string_view src =
        ":a\n"
        "  :b\n"
        "    :c\n"
        "/a\n";

    auto ctx = load(src);
    EXPECT(!ctx.has_errors());
    EXPECT(ctx.document.category_count() == 4); // root + a + b + c
    return true;
}

static bool test_invalid_named_close_is_error()
{
    constexpr std::string_view src =
        ":a\n"
        "/b\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors());
    EXPECT(ctx.errors[0].kind == semantic_error_kind::invalid_category_close);
    return true;
}

static bool test_malformed_array_survives()
{
    constexpr std::string_view src =
        "a = 1||2|||3\n";

    auto ctx = load(src);
    EXPECT(!ctx.has_errors());

    auto root = ctx.document.root();
    auto key  = root->keys()[0];

    auto arr = key.value().as_array<int>();
    EXPECT(arr.size() == 4);
    EXPECT(!arr[2].has_value()); // empty element
    return true;
}

static bool test_max_nesting_depth_enforced()
{
    parse_options opts;
    opts.max_category_depth = 2;

    constexpr std::string_view src =
        ":a\n"
        "  :b\n"
        "    :c\n";

    auto ctx = load(src, opts);
    EXPECT(ctx.has_errors());
    EXPECT(ctx.errors[0].kind == semantic_error_kind::max_depth_exceeded);
    return true;
}


//------------------------------------------------------------

int main()
{
//--Parser tests
    RUN_TEST(test_parser_marks_tacit_types_unresolved);
    RUN_TEST(test_parser_records_declared_types_without_validation);

//--Materialiser tests
    RUN_TEST(test_root_exists);
    RUN_TEST(test_simple_category);
    RUN_TEST(test_nested_categories);
    RUN_TEST(test_category_implicit_closure);
    RUN_TEST(test_simple_table);
    RUN_TEST(test_table_in_subcategory);
    RUN_TEST(test_multiple_root_tables);
    RUN_TEST(test_keys_and_tables_interleaved);
    RUN_TEST(test_root_and_subcategory_keys);
    RUN_TEST(test_deep_implicit_closure);

    RUN_TEST(test_duplicate_key_rejected);
    RUN_TEST(test_declared_key_type_mismatch);
    RUN_TEST(test_declared_column_type_mismatch);
    RUN_TEST(test_named_collapse_closes_multiple_scopes);
    RUN_TEST(test_invalid_named_close_is_error);
    RUN_TEST(test_max_nesting_depth_enforced);
    
    RUN_TEST(test_same_key_in_different_categories_allowed);
    
    RUN_TEST(test_key_type_inference);
    RUN_TEST(test_declared_key_type);
    RUN_TEST(test_table_column_inference);
    RUN_TEST(test_declared_table_column);
    RUN_TEST(test_malformed_array_survives);

    size_t failed = 0;
    for (auto& r : results)
        if (!r.passed)
            ++failed;

    std::cout << "----------------------------------------\n";
    std::cout << "Tests run: " << results.size() << "\n";
    std::cout << "Passed   : " << (results.size() - failed) << "\n";
    std::cout << "Failed   : " << failed << "\n";

    return failed == 0 ? 0 : 1;
}
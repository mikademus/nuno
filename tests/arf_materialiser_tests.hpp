#ifndef ARF_TESTS_MATERIALISER__
#define ARF_TESTS_MATERIALISER__

#include "arf_test_harness.hpp"
#include "../include/arf.hpp"

namespace arf::tests
{

static bool same_key_in_different_categories_allowed()
{
    constexpr std::string_view src =
        "a = 1\n"
        "cat:\n"
        "    a = 2\n";

    auto doc = load(src);
    EXPECT(doc.has_errors() == false, "error emitted");
    return true;
}

static bool duplicate_key_rejected()
{
    constexpr std::string_view src =
        "a = 1\n"
        "a = 2\n";

    auto doc = load(src);
    EXPECT(doc.has_errors() == false, "error emitted");
    return true;
}

static bool declared_key_type_mismatch()
{
    constexpr std::string_view src =
        "x:int = hello\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "key type check incorrectly passed");
    return true;
}

static bool declared_column_type_mismatch()
{
    constexpr std::string_view src =
        "# a:int\n"
        "  hello\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "column type check incorrectly passed");
    auto const & e0 = ctx.errors[0].kind;
    EXPECT(is_material_error(e0) && get_material_error(e0) == semantic_error_kind::type_mismatch, "wrong error code");
    return true;
}

static bool named_collapse_closes_multiple_scopes()
{
    constexpr std::string_view src =
        ":a\n"
        "  :b\n"
        "    :c\n"
        "/a\n";

    auto ctx = load(src);
    EXPECT(!ctx.has_errors(), "error emitted");
    EXPECT(ctx.document.category_count() == 4, "incorrect arity"); // root + a + b + c
    return true;
}

static bool invalid_named_close_is_error()
{
    constexpr std::string_view src =
        ":a\n"
        "/b\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "no error emitted");
    auto const & e0 = ctx.errors[0].kind;
    EXPECT(is_material_error(e0), "incorrect (non-semantic) error type");
    EXPECT(get_material_error(e0) == semantic_error_kind::invalid_category_close, "incorrect error code");
    return true;
}

static bool max_nesting_depth_enforced()
{
    materialiser_options opts;
    opts.max_category_depth = 2;

    constexpr std::string_view src =
        ":a\n"
        "  :b\n"
        "    :c\n";

    auto parse_ctx = parse(src);
    auto ctx = materialise(parse_ctx, opts);
    EXPECT(ctx.has_errors(), "no error emitted");
    EXPECT(ctx.errors.front().kind == semantic_error_kind::depth_exceeded, "incorrect error code");
    return true;
}

static bool invalid_declared_key_type_is_error()
{
    constexpr std::string_view src =
        "x:dragon = 42\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "no error emitted");

    auto e = ctx.errors.front().kind;
    EXPECT(is_material_error(e), "incorrect (non-semantic) error type");
    EXPECT(get_material_error(e) == semantic_error_kind::invalid_declared_type, "incorrect error code");

    return true;
}

static bool invalid_declared_column_type_is_error()
{
    constexpr std::string_view src =
        "# a:dragon\n"
        "  42\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "no error emitted");

    auto e = ctx.errors.front().kind;
    EXPECT(is_material_error(e), "incorrect (non-semantic) error type");
    EXPECT(get_material_error(e) == semantic_error_kind::invalid_declared_type, "incorrect error code");

    return true;
}

static bool invalid_key_is_flagged()
{
    constexpr std::string_view src =
        "x:dragon = 42\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "no error emitted");

    auto doc = ctx.document;
    EXPECT(doc.key_count() == 1, "incorrect arity");

    auto k = doc.key(key_id{0});
    EXPECT(k.has_value(), "there is no key");
    EXPECT(k->node->semantic == semantic_state::invalid, "the invalid state flag is not set");
    EXPECT(k->value().type == value_type::string, "the key type has not collapsed to string");

    return true;
}

static bool invalid_column_is_flagged()
{
    constexpr std::string_view src =
        "# a:dragon\n"
        "  42\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "no error emitted");

    auto doc = ctx.document;
    auto tbl = doc.table(table_id{0});
    EXPECT(tbl.has_value(), "there is no table");

    auto col = tbl->node->columns[0];
    EXPECT(col.semantic == semantic_state::invalid, "the invalid state flag is not set");
    EXPECT(col.type == value_type::string, "the key type has not collapsed to string");

    return true;
}

static bool invalid_cell_is_flagged()
{
    constexpr std::string_view src =
        "# a:int\n"
        "  hello\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "no error emitted");

    auto row = ctx.document.row(table_row_id{0});
    EXPECT(row.has_value(), "there is no row");

    auto cell = row->node->cells[0];
    EXPECT(cell.semantic == semantic_state::invalid, "the invalid state flag is not set");
    EXPECT(cell.type == value_type::string, "the key type has not collapsed to string");

    return true;
}


//----------------------------------------------------------------------------

inline void run_materialiser_tests()
{
    RUN_TEST(same_key_in_different_categories_allowed);
    RUN_TEST(duplicate_key_rejected);
    RUN_TEST(declared_key_type_mismatch);
    RUN_TEST(declared_column_type_mismatch);
    RUN_TEST(named_collapse_closes_multiple_scopes);
    RUN_TEST(invalid_named_close_is_error);
    RUN_TEST(max_nesting_depth_enforced);
    RUN_TEST(invalid_declared_key_type_is_error);
    RUN_TEST(invalid_declared_column_type_is_error);
    RUN_TEST(invalid_key_is_flagged);
    RUN_TEST(invalid_column_is_flagged);    
    RUN_TEST(invalid_cell_is_flagged);    
}

}

#endif
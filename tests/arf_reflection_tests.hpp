#ifndef ARF_TESTS_REFLECTION__
#define ARF_TESTS_REFLECTION__

#include "arf_test_harness.hpp"
#include "../include/arf.hpp"
#include "../include/arf_reflect.hpp"

namespace arf::tests
{

static bool reflect_empty_address_is_root()
{
    using namespace arf::reflect;

    auto ctx = load("a:\n  x = 1\n");

    inspect_context ictx;
    ictx.doc = &ctx.document;

    address addr = root();
    auto result = inspect(ictx, addr);

    EXPECT(addr.steps.empty(), "empty address has no steps");
    EXPECT(std::holds_alternative<document::category_view>(result.item),
        "empty address should yield root category"
    );

    return true;
}



static bool reflect_top_level_category_key()
{
    using namespace arf::reflect;

    auto ctx = load("a:\n  x = 1\n");

    inspect_context ictx;
    ictx.doc = &ctx.document;

    address addr =
        root()
            .top("a")
            .key("x");

    auto result = inspect(ictx, addr);

    EXPECT(addr.steps.size() == 2, "expected two steps");
    EXPECT(addr.steps[0].diagnostic.state == step_state::ok, "top step failed");
    EXPECT(addr.steps[1].diagnostic.state == step_state::ok, "key step failed");

    EXPECT(result.value != nullptr, "no value produced");
    EXPECT(result.value->type == value_type::integer, "wrong value type");

    return true;
}

static bool reflect_explicit_subcategory_key()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  :b\n"
        "    x = 1\n"
    );

    inspect_context ictx;
    ictx.doc = &ctx.document;

    address addr =
        root()
            .top("a")
            .sub("b")
            .key("x");

    auto result = inspect(ictx, addr);

    EXPECT(result.value != nullptr, "nested subcategory key not resolved");
    EXPECT(result.value->type == value_type::integer, "wrong value type");
    EXPECT(addr.ok(), "unexpected error during inspection");

    return true;
}

static bool reflect_subcategory_without_context_fails()
{
    using namespace arf::reflect;

    auto ctx = load("a:\n  x = 1\n");

    inspect_context ictx;
    ictx.doc = &ctx.document;

    address addr = root().sub("a");
    auto result = inspect(ictx, addr);

    EXPECT(addr.steps.size() == 1, "expected one step");
    EXPECT(addr.steps[0].diagnostic.state == step_state::error, "step should fail");
    EXPECT(addr.steps[0].diagnostic.error == step_error::sub_category_under_root, "wrong error kind");
    EXPECT(result.value == nullptr, "no value should be produced");

    return true;
}

static bool reflect_top_level_category_does_not_nest()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  :b\n"
        "    x = 1\n"
    );

    inspect_context ictx;
    ictx.doc = &ctx.document;

    address addr = root().top("a").top("b").key("x");
    auto result = inspect(ictx, addr);

    EXPECT(addr.steps[0].diagnostic.state == step_state::ok, "first step should pass");
    EXPECT(addr.steps[1].diagnostic.state == step_state::error, "second step should fail");
    EXPECT(addr.steps[1].diagnostic.error == step_error::top_category_after_category, "wrong error kind");
    EXPECT(result.value == nullptr, "no value should be produced");

    return true;
}

static bool reflect_table_cell_by_column_name()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  # x  y\n"
        "    1  2\n"
    );

    auto cat = ctx.document.category("a");
    auto tid = cat->tables()[0];
    auto rid = ctx.document.table(tid)->rows()[0];

    inspect_context rctx;
    rctx.doc = &ctx.document;

    address addr =
        root()
            .top("a")
            .local_table(0)
            .row(rid)
            .column("y");

    auto res = inspect(rctx, addr);

    EXPECT(res.value != nullptr, "cell not resolved");
    EXPECT(addr.ok(), "unexpected error during table cell resolution");

    auto cell_type = res.value->type;
    EXPECT(cell_type != value_type::unresolved, "cell must be materialised");

    return true;
}

static bool reflect_invalid_column_fails()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  # x\n"
        "    1\n"
    );

    auto cat = ctx.document.category("a");
    auto tid = cat->tables()[0];
    auto rid = ctx.document.table(tid)->rows()[0];

    inspect_context rctx;
    rctx.doc = &ctx.document;

    address addr =
        root()
            .top("a")
            .table(tid)
            .row(rid)
            .column("nope");

    auto res = inspect(rctx, addr);

    EXPECT(res.value == nullptr, "invalid column must not resolve");
    EXPECT(!addr.ok(), "invalid column must produce an error");

    const auto& diag = addr.steps.back().diagnostic;
    EXPECT(diag.error == step_error::column_not_found, "wrong error kind for invalid column");

    return true;
}

static bool reflect_array_index()
{
    using namespace arf::reflect;

    auto ctx = load("a:\n  x:int[] = 1|2|3\n");

    inspect_context rctx;
    rctx.doc = &ctx.document;

    address addr =
        root()
            .top("a")
            .key("x")
            .index(2);

    auto res = inspect(rctx, addr);

    EXPECT(res.value != nullptr, "array element not resolved");
    EXPECT(addr.ok(), "unexpected errors during array indexing");
    EXPECT(res.value->type == value_type::integer, "wrong array element type");

    return true;
}

static bool reflect_array_index_out_of_bounds_fails()
{
    using namespace arf::reflect;

    auto ctx = load("a:\n  x:int[] = 1|2|3\n");

    inspect_context rctx;
    rctx.doc = &ctx.document;

    address addr =
        root()
            .top("a")
            .key("x")
            .index(99);

    auto res = inspect(rctx, addr);

    EXPECT(!addr.ok(), "missing error for out-of-bounds index");

    const auto& diag = addr.steps.back().diagnostic;
    EXPECT(diag.error == step_error::index_out_of_bounds,
           "wrong error kind for index out of bounds"
    );

    return true;
}

inline void run_reflection_tests()
{
    SUBCAT("Reflection");
    RUN_TEST(reflect_empty_address_is_root);
    RUN_TEST(reflect_top_level_category_key);
    RUN_TEST(reflect_explicit_subcategory_key);
    RUN_TEST(reflect_subcategory_without_context_fails);
    RUN_TEST(reflect_top_level_category_does_not_nest);
    RUN_TEST(reflect_table_cell_by_column_name);
    RUN_TEST(reflect_invalid_column_fails);
    RUN_TEST(reflect_array_index);
    RUN_TEST(reflect_array_index_out_of_bounds_fails);
}

} // namespace arf::tests

#endif

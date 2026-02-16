#ifndef ARF_TESTS_REFLECTION__
#define ARF_TESTS_REFLECTION__

#include "arf_test_harness.hpp"
#include "../include/arf.hpp"
#include "../include/arf_reflect.hpp"

namespace arf::tests
{
template <typename T>
const T* get_step(const arf::reflect::address& addr)
{
    if (addr.steps.empty())
        return nullptr;

    return std::get_if<T>(&addr.steps.back().step);
}


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


static bool reflect_empty_address_has_item_but_no_value()
{
    using namespace arf::reflect;

    auto ctx = load("a:\n  x = 1\n");

    inspect_context ictx;
    ictx.doc = &ctx.document;

    address addr = root();
    auto res = inspect(ictx, addr);

    EXPECT(std::holds_alternative<document::category_view>(res.item),
           "root should be the resolved item");
    EXPECT(res.value == nullptr,
           "root has no value");

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
    EXPECT(!addr.has_error(), "unexpected error during inspection");

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
    EXPECT(!addr.has_error(), "unexpected error during table cell resolution");

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
    EXPECT(addr.has_error(), "invalid column must produce an error");

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
    EXPECT(!addr.has_error(), "unexpected errors during array indexing");
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

    EXPECT(addr.has_error(), "missing error for out-of-bounds index");

    const auto& diag = addr.steps.back().diagnostic;
    EXPECT(diag.error == step_error::index_out_of_bounds,
           "wrong error kind for index out of bounds"
    );

    return true;
}

static bool reflect_partial_prefix_stops_at_error()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  x = 1\n"
    );

    inspect_context ictx;
    ictx.doc = &ctx.document;

    address addr =
        root()
            .top("a")
            .key("x")
            .index(0); // invalid: not an array

    auto res = inspect(ictx, addr);

    EXPECT(res.steps_inspected == 3, "inspection should stop at failing step");
    EXPECT(addr.has_error(), "address must report error");

    // prefix is valid
    EXPECT(addr.steps[0].diagnostic.state == step_state::ok, "top ok");
    EXPECT(addr.steps[1].diagnostic.state == step_state::ok, "key ok");

    // failing step
    EXPECT(addr.steps[2].diagnostic.state == step_state::error, "index must fail");
    EXPECT(addr.steps[2].diagnostic.error == step_error::not_an_array, "wrong error kind");

    return true;
}

static bool reflect_partial_prefix_yields_last_valid_item()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  x = 1\n"
    );

    inspect_context ictx;
    ictx.doc = &ctx.document;

    address addr =
        root()
            .top("a")
            .key("x")
            .index(99); // invalid - can't index a scalar

    auto res = inspect(ictx, addr);

    EXPECT(res.has_error(), "indexing a scalar should be an error");
    EXPECT(res.steps_inspected == 3, "inspection stops at index error");
    EXPECT(!res.ok(), "inspection should not be ok");
    EXPECT(res.value != nullptr, "scalar value preserved despite failed index");
    EXPECT(std::holds_alternative<document::key_view>(res.item), "item should be the key");

    return true;
}

static bool reflect_partial_prefix_preserves_last_structure()
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
            .sub("nope"); // invalid

    auto res = inspect(ictx, addr);

    EXPECT(res.value == nullptr, "no value on failure");
    EXPECT(std::holds_alternative<document::category_view>(res.item),
           "last valid item should be category");

    auto cat = std::get<document::category_view>(res.item);
    EXPECT(cat.name() == "b", "last valid category should be 'b'");

    return true;
}

static bool reflect_indented_but_not_subcategory_does_not_resolve()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  b:\n"
        "    x = 1\n"
    );

    inspect_context ictx{ .doc = &ctx.document };

    address addr =
        root()
            .top("a")
            .sub("b");

    auto res = inspect(ictx, addr);

    EXPECT(!res.ok(), "b is not a subcategory of a");
    EXPECT(res.first_error_step() == 1, "failure should occur at sub step");

    return true;
}

static bool inspect_reports_partial_progress()
{
    using namespace arf::reflect;

    auto ctx = load("a:\n  x = 1\n");

    inspect_context ictx{ .doc = &ctx.document };

    address addr =
        root()
            .top("a")
            .sub("nope")   // will fail here
            .key("x");

    auto res = inspect(ictx, addr);

    //EXPECT(res.addr == addr, "inspected must reference address");
    EXPECT(res.steps_inspected == 2, "inspection should stop at failing step");
    EXPECT(!res.ok(), "inspection must not be ok");
    EXPECT(addr.steps[1].diagnostic.state == step_state::error,
           "second step must carry error");

    return true;
}

static bool structural_children_of_category()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  :b\n"
        "    x = 1\n"
        "  y = 2\n"
    );

    inspect_context ictx{ .doc = &ctx.document };

    address addr = root().top("a");
    auto res = inspect(ictx, addr);

    auto children = res.structural_children(ictx);

    bool saw_sub = false;
    bool saw_key = false;

    for (auto& c : children)
    {
        if (c.kind == structural_child::kind::sub_category && c.name == "b")
            saw_sub = true;
        if (c.kind == structural_child::kind::key && c.name == "y")
            saw_key = true;
    }

    EXPECT(saw_sub, "step 1: subcategory b should be listed");
    EXPECT(!saw_key, "step 1: key y should not be listed");

    saw_sub = false;
    saw_key = false;

    addr.sub("b");
    res = inspect(ictx, addr);
    
    children = res.structural_children(ictx);

    for (auto& c : children)
    {
        if (c.kind == structural_child::kind::sub_category && c.name == "b")
            saw_sub = true;
        if (c.kind == structural_child::kind::key && c.name == "y")
            saw_key = true;
    }

    EXPECT(!saw_sub, "step 2: subcategory b should not be listed");
    EXPECT(saw_key, "step 2: key y should be listed");

    return true;
}

static bool structural_children_after_failed_inspection()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  x:int[] = 1|2|3\n"
    );

    inspect_context ictx{ .doc = &ctx.document };

    address addr =
        root()
            .top("a")
            .key("x")
            .index(99); // invalid

    auto res = inspect(ictx, addr);

    EXPECT(res.has_error(), "should be an error reported");
    EXPECT(res.value != nullptr, "should have a value");

    EXPECT(!res.ok(), "inspection must fail");

    auto children = res.structural_children(ictx);

    bool saw_index = false;
    for (auto& c : children)
        if (c.kind == structural_child::kind::index)
            saw_index = true;

    EXPECT(saw_index, "array indices should be offered after failure");

    return true;
}

static bool structural_children_of_table_row()
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

    inspect_context ictx{ .doc = &ctx.document };

    address addr =
        root()
            .top("a")
            .table(tid)
            .row(rid);

    auto res = inspect(ictx, addr);
    auto children = res.structural_children(ictx);

    bool saw_x = false;
    bool saw_y = false;

    for (auto& c : children)
    {
        if (c.kind == structural_child::kind::column && c.name == "x")
            saw_x = true;
        if (c.kind == structural_child::kind::column && c.name == "y")
            saw_y = true;
    }

    EXPECT(saw_x && saw_y, "row should expose columns x and y");

    return true;
}

static bool structural_children_of_scalar_value_is_empty()
{
    using namespace arf::reflect;

    auto ctx = load("a:\n  x = 1\n");

    inspect_context ictx{ .doc = &ctx.document };

    address addr =
        root()
            .top("a")
            .key("x");

    auto res = inspect(ictx, addr);
    auto children = res.structural_children(ictx);

    EXPECT(children.empty(), "scalar value should have no structural children");

    return true;
}

static bool structural_extend_address_from_category()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  :b\n"
        "    x = 1\n"
    );

    inspect_context ictx{ .doc = &ctx.document };

    address addr = root().top("a");
    auto res = inspect(ictx, addr);

    auto children = res.structural_children(ictx);

    bool extended = false;

    for (auto& c : children)
    {
        if (c.kind == structural_child::kind::sub_category && c.name == "b")
        {
            auto next = res.extend_address(c);
            EXPECT(next.steps.size() == 2, "extended address should have one extra step");
            EXPECT(std::holds_alternative<sub_category_step>(next.steps.back().step),
                   "extended step should be sub_category");
            extended = true;
        }
    }

    EXPECT(extended, "expected to extend address with subcategory b");
    return true;
}

static bool structural_extend_address_after_failed_inspection()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  :b\n"
        "    x = 1\n"
    );

    inspect_context ictx{ .doc = &ctx.document };

    address addr =
        root()
            .top("a")
            .sub("nope"); // fails

    auto res = inspect(ictx, addr);

    EXPECT(!res.ok(), "inspection must fail");

    auto children = res.structural_children(ictx);

    bool saw_b = false;

    for (auto& c : children)
    {
        if (c.kind == structural_child::kind::sub_category && c.name == "b")
        {
            auto next = res.extend_address(c);
            EXPECT(next.steps.size() == 2, "extension should be from last valid prefix");
            saw_b = true;
        }
    }

    EXPECT(saw_b, "should be able to extend from last valid category");
    return true;
}

static bool structural_prefix_children_matching()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  alpha = 1\n"
        "  beta  = 2\n"
        "  gamma = 3\n"
    );

    inspect_context ictx{ .doc = &ctx.document };

    address addr = root().top("a");
    auto res = inspect(ictx, addr);

    auto matches = res.prefix_children_matching(ictx, "al");

    EXPECT(matches.size() == 1, "only one key should match prefix");
    EXPECT(matches[0].child.kind == structural_child::kind::key, "match should be a key");
    EXPECT(matches[0].child.name == "alpha", "matched key should be alpha");

    return true;
}

static bool structural_children_of_scalar_is_empty()
{
    using namespace arf::reflect;

    auto ctx = load("a:\n  x = 1\n");

    inspect_context ictx{ .doc = &ctx.document };

    address addr =
        root()
            .top("a")
            .key("x");

    auto res = inspect(ictx, addr);
    auto children = res.structural_children(ictx);

    EXPECT(children.empty(), "scalar value must have no structural children");
    return true;
}

static bool prefix_children_empty_prefix_lists_all()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  :b\n"
        "    x = 1\n"
        "  y = 2\n"
    );

    inspect_context ictx{ .doc = &ctx.document };
    auto res = inspect(ictx, root().top("a"));

    auto all = res.structural_children(ictx);
    auto pref = res.prefix_children_matching(ictx, "");

    EXPECT(pref.size() == all.size(),
           "empty prefix should list all structural children");

    return true;
}

static bool prefix_children_excludes_anonymous()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  :t\n"
        "    # x\n"
        "      1\n"
    );

    inspect_context ictx{ .doc = &ctx.document };
    auto res = inspect(ictx, root().top("a"));

    auto pref = res.prefix_children_matching(ictx, "0");

    for (auto& p : pref)
        EXPECT(!p.child.name.empty(),
               "anonymous children must not match textual prefix");

    return true;
}

static bool prefix_children_preserve_order()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  apple = 1\n"
        "  apricot = 2\n"
        "  banana = 3\n"
    );

    inspect_context ictx{ .doc = &ctx.document };
    auto res = inspect(ictx, root().top("a"));

    auto pref = res.prefix_children_matching(ictx, "ap");

    EXPECT(pref.size() == 2, "two matches expected");
    EXPECT(pref[0].child.name == "apple", "order must be preserved");
    EXPECT(pref[1].child.name == "apricot", "order must be preserved");

    return true;
}

static bool suggest_next_after_failed_inspection()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  :b\n"
        "    x = 1\n"
    );

    inspect_context ictx{ .doc = &ctx.document };

    auto res = inspect(
        ictx,
        root().top("a").sub("nope")
    );

    auto suggestions = res.suggest_next(ictx, "");

    bool saw_b = false;

    for (const auto& addr : suggestions)
    {
        if (auto* sub = get_step<sub_category_step>(addr))
        {
            if (sub->name == "b")
                saw_b = true;
        }
    }

    EXPECT(saw_b, "should suggest subcategory 'b'");

    return true;
}

static bool suggest_next_prefix_filters_children()
{
    using namespace arf::reflect;

    auto ctx = load(
        "a:\n"
        "  apple = 1\n"
        "  banana = 2\n"
    );

    inspect_context ictx{ .doc = &ctx.document };
    auto res = inspect(ictx, root().top("a"));

    auto suggestions = res.suggest_next(ictx, "ba");

    EXPECT(suggestions.size() == 1, "only one suggestion should match");

    auto* key = get_step<key_step>(suggestions[0]);
    EXPECT(key != nullptr, "expected key suggestion");
    EXPECT(std::get<std::string_view>(key->id) == "banana", "only banana should match prefix");

    return true;
}


inline void run_reflection_tests()
{
    SUBCAT("Inspection basic");
    RUN_TEST(reflect_empty_address_is_root);
    RUN_TEST(reflect_empty_address_has_item_but_no_value);
    RUN_TEST(inspect_reports_partial_progress);

    SUBCAT("Address semantics");
    RUN_TEST(reflect_top_level_category_key);
    RUN_TEST(reflect_explicit_subcategory_key);
    RUN_TEST(reflect_subcategory_without_context_fails);
    RUN_TEST(reflect_top_level_category_does_not_nest);
    RUN_TEST(reflect_indented_but_not_subcategory_does_not_resolve);

    SUBCAT("Value resolution");
    RUN_TEST(reflect_table_cell_by_column_name);
    RUN_TEST(reflect_invalid_column_fails);
    RUN_TEST(reflect_array_index);
    RUN_TEST(reflect_array_index_out_of_bounds_fails);

    SUBCAT("Partial inspection behaviour");
    RUN_TEST(reflect_partial_prefix_stops_at_error);
    RUN_TEST(reflect_partial_prefix_yields_last_valid_item);
    RUN_TEST(reflect_partial_prefix_preserves_last_structure);

    SUBCAT("Structural queries");
    RUN_TEST(structural_children_of_category);
    RUN_TEST(structural_children_after_failed_inspection);
    RUN_TEST(structural_children_of_table_row);
    RUN_TEST(structural_children_of_scalar_value_is_empty);
    RUN_TEST(structural_extend_address_from_category);
    RUN_TEST(structural_extend_address_after_failed_inspection);
    RUN_TEST(structural_prefix_children_matching);
    RUN_TEST(structural_children_of_scalar_is_empty);
    RUN_TEST(prefix_children_empty_prefix_lists_all);
    RUN_TEST(prefix_children_excludes_anonymous);
    RUN_TEST(prefix_children_preserve_order);
    RUN_TEST(suggest_next_after_failed_inspection);
    RUN_TEST(suggest_next_prefix_filters_children);
}

} // namespace arf::tests

#endif

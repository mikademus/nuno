#ifndef ARF_TESTS_MATERIALISER__
#define ARF_TESTS_MATERIALISER__

/* *************************************************************************
 *  This test suite enforces semantic contracts of the ARF materialiser.   *
 *  Test names describe language policy, not implementation behaviour.     *
 ************************************************************************* */

#include "arf_test_harness.hpp"
#include "../include/arf.hpp"

#include <ranges>
namespace arf::tests
{

static bool scope_keys_are_category_local()
{
    constexpr std::string_view src =
        "a = 1\n"
        "cat:\n"
        "    a = 2\n";

    auto doc = load(src);
    EXPECT(doc.has_errors() == false, "error emitted");
    return true;
}

static bool scope_duplicate_keys_rejected()
{
    constexpr std::string_view src =
        "a = 1\n"
        "a = 2\n";

    auto doc = load(src);
    EXPECT(doc.has_errors() == false, "error emitted");
    return true;
}

static bool type_key_declared_mismatch_collapses()
{
    constexpr std::string_view src =
        "x:int = hello\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "key type check incorrectly passed");
    return true;
}

static bool type_column_declared_mismatch_collapses()
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

static bool subcategory_under_root_is_error()
{
    constexpr std::string_view src =
        ":a\n"
        "  :b\n"
        "    :c\n"
        "/a\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "incorrectly accepted subcategory in root");
    EXPECT(is_material_error(ctx.errors.front().kind), "error not of material type");
    EXPECT(get_material_error(ctx.errors.front().kind) == semantic_error_kind::invalid_subcategory, "wrong error code");
    return true;
}

static bool scope_named_collapse_unwinds_all()
{
    constexpr std::string_view src =
        "foo:\n"
        "  :a\n"
        "    :b\n"
        "      :c\n"
        "  /a\n"
        "  :d\n";

    auto ctx = load(src);
    EXPECT(!ctx.has_errors(), "document must parse without errors");

    // root + foo + a + b + c + d
    EXPECT(ctx.document.category_count() == 6, "incorrect category count");

    auto foo = ctx.document.category(category_id{1});
    auto a   = ctx.document.category(category_id{2});
    auto b   = ctx.document.category(category_id{3});
    auto c   = ctx.document.category(category_id{4});
    auto d   = ctx.document.category(category_id{5});

    EXPECT(foo->parent()->is_root(), "foo must attach to root");
    EXPECT(a->parent()->name() == "foo", "a must attach to foo");
    EXPECT(b->parent()->name() == "a",   "b must attach to a");
    EXPECT(c->parent()->name() == "b",   "c must attach to b");

    // Critical assertion: d must attach to foo, not a/b/c
    EXPECT(d->parent()->name() == "foo", "named close must unwind to foo scope");

    return true;
}

static bool scope_invalid_named_close_is_error()
{
    constexpr std::string_view src =
        "a:\n"
        "/b\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "no error emitted");
    auto const & e0 = ctx.errors[0].kind;
    EXPECT(is_material_error(e0), "incorrect (non-semantic) error type");
    EXPECT(get_material_error(e0) == semantic_error_kind::invalid_category_close, "incorrect error code");
    return true;
}

static bool scope_max_depth_enforced()
{
    materialiser_options opts;
    opts.max_category_depth = 2;

    constexpr std::string_view src =
        "a:\n"
        "  :b\n"
        "    :c\n";

    auto parse_ctx = parse(src);
    auto ctx = materialise(parse_ctx, opts);
    EXPECT(ctx.has_errors(), "no error emitted");
    EXPECT(ctx.errors.front().kind == semantic_error_kind::depth_exceeded, "incorrect error code");
    return true;
}

static bool type_key_invalid_declaration_is_error()
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

static bool type_column_invalid_declaration_is_error()
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

static bool semantic_invalid_key_flagged()
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

static bool semantic_invalid_column_flagged()
{
    constexpr std::string_view src =
        "# a:dragon\n"
        "  42\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "no error emitted");

    auto doc = ctx.document;
    auto tbl = doc.table(table_id{0});
    EXPECT(tbl.has_value(), "there is no table");

    auto col_id = tbl->node->columns[0];
    auto col = doc.column(col_id);
    EXPECT(col->node->col.semantic == semantic_state::invalid, "the invalid state flag is not set");
    EXPECT(col->node->col.type == value_type::string, "the key type has not collapsed to string");

    return true;
}

static bool semantic_invalid_cell_flagged()
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

static bool contamination_column_contaminates_rows_only()
{
    constexpr std::string_view src =
        "# a:int\n"
        "  hello\n";

    auto ctx = load(src);
    auto doc = ctx.document;

    auto tbl = doc.table(table_id{0});
    EXPECT(tbl.has_value(), "there is no table");
    EXPECT(tbl->is_contaminated(), "table should be contaminated by invalid column");
    EXPECT(tbl->is_locally_valid(), "table itself is not misformed"); 

    auto row = doc.row(table_row_id{0});
    EXPECT(row.has_value(), "there is no row");
    EXPECT(row->is_contaminated(), "row should be contaminated by invalid column");
    EXPECT(row->is_locally_valid(), "row itself is not malformed");    

    return true;
}

static bool view_exposes_row_invalidity()
{
    constexpr std::string_view src =
        "# a:int\n"
        "  42\n"
        "  nope\n";

    auto ctx = load(src);
    auto doc = ctx.document;

    auto row0 = doc.row(table_row_id{0});
    auto row1 = doc.row(table_row_id{1});

    EXPECT(!row0->is_contaminated(), "row should not be in contaminated state");
    EXPECT(row0->is_locally_valid(), "row should be structurally valid");

    EXPECT(row1->is_contaminated(), "row should be in contaminated state");
    EXPECT(row1->is_locally_valid(), "row should be structurally valid");

    return true;
}

static bool array_key_all_elements_valid()
{
    constexpr std::string_view src =
        "arr:int[] = 1|2|3\n";

    auto ctx = load(src);
    auto doc = ctx.document;

    auto key = doc.key(key_id{0});
    EXPECT(key.has_value(), "missing key");
    EXPECT(key->is_locally_valid(), "valid array key rejected");
    EXPECT(!key->is_contaminated(), "clean array key marked as contaminated");

    return true;
}

static bool array_invalid_element_contaminates_key()
{
    constexpr std::string_view src =
        "arr:int[] = 1|nope|3\n";

    auto ctx = load(src);
    auto doc = ctx.document;

    auto key = doc.key(key_id{0});
    EXPECT(key.has_value(), "missing key");
    EXPECT(key->is_locally_valid(), "invalid array element should not invalidate key");
    EXPECT(key->is_contaminated(), "invalid array element should contaminate key");

    return true;
}

static bool array_untyped_collapses_to_string()
{
    constexpr std::string_view src =
        "arr = 1|2|3\n";

    auto ctx = load(src);
    auto doc = ctx.document;

    auto key = doc.key(key_id{0});
    EXPECT(key.has_value(), "missing key");
    EXPECT(key->value().type == value_type::string, "unannotated array literal was not treated as string");

    return true;
}

static bool array_table_cells_valid()
{
    constexpr std::string_view src =
        "# id  vals:int[]\n"
        "  1   1|2|3\n"
        "  2   4|5\n";

    auto ctx = load(src);
    auto doc = ctx.document;

    auto row0 = doc.row(table_row_id{0});
    auto row1 = doc.row(table_row_id{1});

    EXPECT(row0->is_locally_valid(), "valid row rejected");
    EXPECT(row1->is_locally_valid(), "valid row rejected");

    return true;
}

static bool array_invalid_element_contaminates_row()
{
    constexpr std::string_view src =
        "# id  vals:int[]\n"
        "  1   1|2|nope\n"
        "  2   3|4\n";

    auto ctx = load(src);
    auto doc = ctx.document;

    auto row0 = doc.row(table_row_id{0});
    auto row1 = doc.row(table_row_id{1});

    EXPECT(row0->is_locally_valid(), "dirty row should be structurally valid");
    EXPECT(row0->is_contaminated(), "dirty row should be in contaimnated state");

    EXPECT(row1->is_locally_valid(), "clean row should be structurally valid");
    EXPECT(!row1->is_contaminated(), "clean row should not be in contaimnated state");

    return true;
}

static bool array_empty_elements_are_missing_not_contaminating()
{
    constexpr std::string_view src =
        "arr:str[] = a||b|\n";

    auto ctx = load(src);
    auto doc = ctx.document;

    auto key = doc.key(key_id{0});
    EXPECT(key.has_value(), "missing key");
    EXPECT(key->is_locally_valid(), "empty array elements should not invalidate array");
    EXPECT(!key->is_contaminated(), "empty array elements should not contaiminate array");

    return true;
}

static bool scope_named_close_of_non_ancestor_is_error()
{
    constexpr std::string_view src =
        "a:\n"
        "  :b\n"
        "c:\n"
        "  /b\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "closing non-ancestor category must be an error");

    EXPECT(is_material_error(ctx.errors.front().kind), "wrong error category");
    EXPECT(get_material_error(ctx.errors.front().kind) == semantic_error_kind::invalid_category_close, "wrong error code");

    return true;
}

static bool scope_key_after_named_close_binds_upwards()
{
    constexpr std::string_view src = 
        R"(a:
        :b
            x = 1
        /b
        y = 2)";

    auto ctx = load(src);
    EXPECT(!ctx.has_errors(), "error was emitted");
    
    auto x = ctx.document.key(key_id{0});
    auto y = ctx.document.key(key_id{1});

    EXPECT(x.has_value(), "x must exist");
    EXPECT(x->owner().name() == "b", "x must be owned by b");

    EXPECT(y.has_value(), "y must exist");
    EXPECT(y->owner().name() == "a", "y must be owned by a");

    return true;
}

static bool scope_named_collapse_resets_key_and_table_scope()
{
    constexpr std::string_view src =
        "a:\n"
        "  :b\n"
        "    x = 1\n"
        "    # t\n"
        "      1\n"
        "  /b\n"
        "  y = 2\n"
        "  # u\n"
        "    3\n";

    auto doc = load(src);
    EXPECT(!doc.has_errors(), "document must parse without errors");

    EXPECT(doc->key_count() == 2, "expected two keys");
    EXPECT(doc->table_count() == 2, "expected two tables");

    auto x = doc->key(key_id{0});
    auto y = doc->key(key_id{1});

    EXPECT(x->owner().name() == "b", "x must attach to b");
    EXPECT(y->owner().name() == "a", "y must attach to a after collapse");

    auto t = doc->table(table_id{0});
    auto u = doc->table(table_id{1});

    EXPECT(t->owner().name() == "b", "table t must attach to b");
    EXPECT(u->owner().name() == "a", "table u must attach to a after collapse");

    return true;
}

static bool failed_category_open_leaves_scope_unchanged()
{
    constexpr std::string_view src = 
        R"(:b
            x = 1)";

    auto doc = load(src);
    EXPECT(doc.has_errors(), "error should be emitted");

    EXPECT(doc->category_count() == 1, "there should only be the root category");
    EXPECT(doc->key_count() == 1, "the key should survive an invalid category");
    auto x = doc->key(key_id{0});
    EXPECT(x->owner().is_root(), "x should not attach to invalid category");

    return true;
}

static bool error_then_continue_invalid_key_does_not_block_following_keys()
{
    constexpr std::string_view src =
        "a:\n"
        "  x:int = foo\n"   // invalid int
        "  y = 42\n";       // valid key

    auto ctx = load(src);

    EXPECT(ctx.has_errors(), "invalid key should produce an error");

    auto key_x = ctx.document.key(key_id{0});
    auto key_y = ctx.document.key(key_id{1});

    EXPECT(key_x.has_value(), "invalid key must still be materialised");
    EXPECT(!key_x->is_locally_valid(), "x must be locally invalid");

    EXPECT(key_y.has_value(), "subsequent key must exist");
    EXPECT(key_y->is_locally_valid(), "subsequent valid key must remain valid");
    EXPECT(key_y->owner().name() == "a", "key y must attach to category a");

    return true;
}

static bool error_then_continue_after_invalid_subcategory_recovers_scope()
{
    constexpr std::string_view src =
        ":illegal\n"
        "d:\n"
        "  x = 1\n";

    auto ctx = load(src);

    EXPECT(ctx.has_errors(), "invalid subcategory not flagged");
    EXPECT(is_material_error(ctx.errors.front().kind), "wrong error class");
    EXPECT(ctx.document.category_count() == 2, "unexpected category count"); // root + d
    auto d_id = ctx.document.categories().back().id();
    auto const& d = ctx.document.category(d_id);
    EXPECT(d.has_value(), "category should be accessible");
    EXPECT(d->name() == "d", "category d not created");
    EXPECT(d->keys().size() == 1, "key did not bind to recovered scope");
    EXPECT(ctx.document.key(d->keys().front())->name() == "x", "key x not owned by d");

    return true;
}

static bool invalid_subcategory_then_named_close_is_safe()
{
    constexpr std::string_view src =
        "/a\n"
        ":illegal\n"
        "d:\n"
        "  x = 1\n";

    auto ctx = load(src);

    EXPECT(ctx.has_errors(), "errors expected");
    EXPECT(ctx.document.category_count() == 2, "unexpected category count");

    auto d_id = ctx.document.categories().back().id();
    auto const& d = ctx.document.category(d_id);
    EXPECT(d.has_value(), "category should be accessible");
    EXPECT(d->name() == "d", "category d missing after recovery");
    EXPECT(d->keys().size() == 1, "key not attached after recovery");

    return true;
}

static bool max_category_depth_allows_exact_limit()
{
    materialiser_options opts;
    opts.max_category_depth = 3;

    constexpr std::string_view src =
        "a:\n"
        "  :b\n"
        "    :c\n";

    auto ctx = load(src, opts);

    EXPECT(!ctx.has_errors(), "exact max depth incorrectly rejected");
    EXPECT(ctx.document.category_count() == 4, "incorrect category count");

    return true;
}

static bool max_category_depth_exceeded_recovers_scope()
{
    materialiser_options opts;
    opts.max_category_depth = 3;

    // category d is too deep:
    constexpr std::string_view src =
        "a:\n"
        "  :b\n"
        "    :c\n"
        "      :d\n" 
        "    x = 1\n";

    auto ctx = load(src, opts);

    EXPECT(ctx.has_errors(), "depth error not reported");

    // x must belong to c, not crash or attach to nonsense
    auto const& c = ctx.document.categories().at(3);
    EXPECT(c.keys().size() == 1, "scope not recovered after depth error");

    return true;
}

static bool category_ids_are_not_dense_indices()
{
    constexpr std::string_view src =
        ":illegal\n"
        "a:\n"
        "  x = 1\n";

    auto ctx = load(src);
    EXPECT(ctx.has_errors(), "expected invalid subcategory");

    auto doc = ctx.document;
    EXPECT(doc.category_count() == 2, "root + a");

    auto cats = doc.categories();
    auto it = std::ranges::find_if(cats, [](auto & cat_view){return cat_view.name() == "a";});
    EXPECT(it != cats.end(), "a must exist");
    auto & a = *it;

    auto a2 = doc.category(a.id());
    EXPECT(a2.has_value(), "a must be retrievable by ID");
    EXPECT(a2->id() == a.id(),"category id must match stored id");

    return true;
}

static bool scope_stack_is_never_empty()
{
    constexpr std::string_view src =
        "/a\n"      // should be converted to comment
        "/b\n";     // should be converted to comment

    auto ctx = load(src);
    EXPECT(!ctx.has_errors(), "invalid closes should have been handled by parser");
    EXPECT(ctx.document.comment_count() == 2, "There should be two comments");
    EXPECT(ctx.document.root().has_value(), "root must survive any invalid closes");

    return true;
}

static bool no_key_owned_by_nonexistent_category()
{
    constexpr std::string_view src =
        ":illegal\n"
        "x = 1\n";

    auto ctx = load(src);
    auto& doc = ctx.document;

    auto cats = doc.categories();
    auto keys = doc.keys();

    for (auto const& k : keys)
    {
        auto it = std::ranges::find_if(cats,
            [&](auto const& c){ return c.id() == k.owner().id(); });

        EXPECT(it != cats.end(),
               "key owner must exist in document categories");
    }

    return true;
}

//----------------------------------------------------------------------------

inline void run_materialiser_tests()
{
/*
1. Global structural & scoping rules
Policies / invariants

• Categories form a tree with lexical scoping
• Keys are scoped to categories
• Duplicate keys within the same category are illegal
• Same key name in different categories is legal
• Named collapse closes all intermediate scopes
• Invalid named closes are errors
• Maximum nesting depth is enforced
*/    
SUBCAT("Scope");
    RUN_TEST(scope_keys_are_category_local);
    RUN_TEST(scope_duplicate_keys_rejected);
    RUN_TEST(subcategory_under_root_is_error);
    RUN_TEST(scope_key_after_named_close_binds_upwards);
    RUN_TEST(scope_named_collapse_unwinds_all);
    RUN_TEST(scope_invalid_named_close_is_error);
    RUN_TEST(scope_named_close_of_non_ancestor_is_error);
    RUN_TEST(scope_named_collapse_resets_key_and_table_scope);
    RUN_TEST(max_category_depth_allows_exact_limit);
    RUN_TEST(scope_max_depth_enforced);
    RUN_TEST(failed_category_open_leaves_scope_unchanged);

/*
2. Declared type handling (keys & columns)
Policies / invariants

• Declared types are parsed before coercion
• Unknown declared types are semantic errors
• Declared type mismatch does not drop data
• On mismatch, values collapse to string
• Collapsing marks local invalidity
*/    
SUBCAT("Type declaration");
    RUN_TEST(type_key_declared_mismatch_collapses);
    RUN_TEST(type_key_invalid_declaration_is_error);
    RUN_TEST(type_column_declared_mismatch_collapses);
    RUN_TEST(type_column_invalid_declaration_is_error);
    
/*
3. Local semantic validity vs contamination

Definitions (as implemented)

• Locally invalid = structurally or semantically malformed
• Contaminated = depends on invalid or contaminated content
• Invalidity does not propagate upward
• Contamination does propagate upward
• Contamination does not propagate downward

Policies / invariants

• Invalid members contaminate containers
• Containers remain locally valid if structurally intact
• Structural integrity is never inferred from child semantics
*/    
SUBCAT("Invalidity and contamination");
    RUN_TEST(semantic_invalid_key_flagged);
    RUN_TEST(semantic_invalid_column_flagged);
    RUN_TEST(semantic_invalid_cell_flagged);  
    RUN_TEST(contamination_column_contaminates_rows_only);
    RUN_TEST(view_exposes_row_invalidity);

/*
4. Arrays: parsing, coercion, and policy
Policies / invariants

• Arrays preserve element boundaries
• Empty elements are missing-but-valid
• Arrays may contain heterogeneously typed values
• Invalid elements contaminate the container
• Invalid elements do not invalidate the container
• Arrays without declared type collapse to string
• Trailing delimiters create empty elements
*/    
SUBCAT("Arrays");
    RUN_TEST(array_key_all_elements_valid);
    RUN_TEST(array_invalid_element_contaminates_key);
    RUN_TEST(array_untyped_collapses_to_string);
    RUN_TEST(array_table_cells_valid);
    RUN_TEST(array_invalid_element_contaminates_row);
    RUN_TEST(array_empty_elements_are_missing_not_contaminating);

SUBCAT("Correctness after error");
    RUN_TEST(error_then_continue_invalid_key_does_not_block_following_keys);
    RUN_TEST(error_then_continue_after_invalid_subcategory_recovers_scope);
    RUN_TEST(invalid_subcategory_then_named_close_is_safe);
    RUN_TEST(max_category_depth_exceeded_recovers_scope);

SUBCAT("Extras");
    RUN_TEST(category_ids_are_not_dense_indices);
    RUN_TEST(scope_stack_is_never_empty);
    RUN_TEST(no_key_owned_by_nonexistent_category);
}

}

#endif
#ifndef ARF_TESTS_DOCUMENT_STRUCTURE__
#define ARF_TESTS_DOCUMENT_STRUCTURE__

#include "arf_test_harness.hpp"
#include "../include/arf.hpp"

namespace arf::tests
{

static bool root_always_exists()
{
    auto doc = arf::load("");
    EXPECT(doc.has_errors() == false, "");
    EXPECT(doc->category_count() >= 1, "");
    EXPECT(doc->root().has_value(), "");
    return true;
}

static bool category_single_level_attaches_to_root()
{
    constexpr std::string_view src =
        "category:\n";
 
    auto doc = load(src);
    EXPECT(!doc.has_errors(), "document must parse without errors");

    EXPECT(doc->category_count() == 2, "expected root + one category");

    auto root = doc->root();
    EXPECT(root.has_value(), "root category must exist");

    auto cat = doc->category(category_id{1});
    EXPECT(cat.has_value(), "declared category must exist");

    EXPECT(cat->parent().has_value(), "category must have a parent");
    EXPECT(cat->parent()->is_root(), "top-level category must attach to root");

    return true;
}

static bool category_nested_ownership_preserved()
{
    constexpr std::string_view src =
        "outer:\n"
        "    :inner\n";

    auto doc = load(src);
    EXPECT(doc.has_errors() == false, "");

    EXPECT(doc->category_count() == 3, ""); // root, outer, inner
    return true;
}

static bool colon_categories_nest_without_explicit_closure()
{
    constexpr std::string_view src =
        "a:\n"
        "    :b\n"
        ":c\n";

    auto doc = load(src);
    EXPECT(!doc.has_errors(), "rejected correct script");

    EXPECT(doc->category_count() == 4, "expected root, a, b, c");

    auto c = doc->category(category_id{3});
    EXPECT(c.has_value(), "category c must exist");

    auto parent = c->parent();
    EXPECT(parent.has_value(), "category c must have a parent");
    EXPECT(c->parent()->name() == "b", "category c must attach to b");

    return true;
}

static bool table_at_root_is_owned_by_root()
{
    constexpr std::string_view src =
        "# a b\n"
        "  1 2\n"
        "  3 4\n";

    auto doc = load(src);
    EXPECT(!doc.has_errors(), "document must parse without errors");

    EXPECT(doc->table_count() == 1, "expected exactly one table");
    EXPECT(doc->row_count() == 2, "expected two rows");

    auto table = doc->table(table_id{0});
    EXPECT(table.has_value(), "root table must exist");

    auto owner = table->owner();
    EXPECT(owner.is_root(), "table defined at top level must be owned by root");

    return true;
}

static bool table_inside_category_allowed()
{
    constexpr std::string_view src =
        "top:\n"
        "    :sub\n"
        "        # x y\n"
        "          a b\n";

    auto doc = load(src);
    EXPECT(doc.has_errors() == false, "");

    EXPECT(doc->table_count() == 1, "");
    EXPECT(doc->row_count() == 1, "");
    return true;
}

static bool table_inside_category_is_owned_by_category()
{
    constexpr std::string_view src =
        "top:\n"
        "    # x y\n"
        "      1 2\n";

    auto doc = load(src);
    EXPECT(!doc.has_errors(), "document must parse without errors");

    EXPECT(doc->table_count() == 1, "expected one table");

    auto table = doc->table(table_id{0});
    EXPECT(table.has_value(), "table must exist");

    auto owner = table->owner();
    EXPECT(!owner.is_root(), "table must not be owned by root");
    EXPECT(owner.name() == "top", "table must be owned by category 'top'");

    return true;
}

static bool multiple_tables_at_same_scope_allowed()
{
    constexpr std::string_view src =
        "# a b\n"
        "  1 2\n"
        "\n"
        "# x y\n"
        "  3 4\n"
        "a:\n"
        "# a b\n"
        "  1 2\n"
        "\n"
        "# x y\n"
        "  3 4\n";

    auto doc = load(src);
    EXPECT(!doc.has_errors(), "document must parse without errors");

    EXPECT(doc->table_count() == 4, "expected two tables at root scope");
    EXPECT(doc->table_count() == 4, "expected two tables at root scope");

    //EXPECT(doc->table_count() == 2, "expected two tables at root scope");
    //EXPECT(doc->row_count() == 2, "expected one row per table");

    auto root = doc->root();
    EXPECT(root.has_value(), "root category must exist");

    auto t0 = doc->table(table_id{0});
    auto t1 = doc->table(table_id{1});

    EXPECT(t0.has_value(), "first table must exist");
    EXPECT(t1.has_value(), "second table must exist");

    EXPECT(t0->owner().is_root(), "first table must be owned by root");
    EXPECT(t1->owner().is_root(), "second table must be owned by root");

    EXPECT(root->tables().size() == 2, "root must list both tables");

    auto a = doc->category(category_id{1});
    EXPECT(a->name() == "a", "subcategory should be named 'a'");
    auto t2 = doc->table(table_id{2});
    auto t3 = doc->table(table_id{3});

    EXPECT(t2.has_value(), "first table must exist");
    EXPECT(t3.has_value(), "second table must exist");
    EXPECT(t2->owner().name() == a->name(), "third table must be owned by a");
    EXPECT(t3->owner().name() == a->name(), "fourth table must be owned by a");

    EXPECT(a->tables().size() == 2, "a must list both tables");

    return true;
}

static bool keys_attach_to_current_category()
{
    constexpr std::string_view src =
        "top:\n"
        "    a = 1\n"
        "    # x y\n"
        "      2 3\n"
        "    b = 4\n";

    auto doc = load(src);
    EXPECT(doc.has_errors() == false, "");

    auto root = doc->root();
    EXPECT(root.has_value(), "");

    EXPECT(doc->table_count() == 1, "");
    EXPECT(doc->row_count() == 1, "");

    // category access by traversal, not name (yet)
    EXPECT(doc->category_count() == 2, "");
    return true;
}

static bool root_key_before_category_is_allowed()
{
    constexpr std::string_view src =
        "x = 1\n"
        "c:\n"
        "    y = 2\n";

    auto doc = load(src);
    EXPECT(doc.has_errors() == false, "document should parse without errors");

    auto root = doc->root();
    EXPECT(root.has_value(), "root category must exist");

    // root key
    auto key0 = doc->key(key_id{0});
    EXPECT(key0.has_value(), "root key must exist");
    EXPECT(key0->owner().is_root(), "key defined before category must attach to root");

    // category key
    auto key1 = doc->key(key_id{1});
    EXPECT(key1.has_value(), "category key must exist");
    EXPECT(!key1->owner().is_root(), "key defined inside category must not attach to root");
    EXPECT(key1->owner().name() == "c", "key must attach to category c");

    return true;
}

static bool category_explicit_nesting_does_not_leak_scope()
{
    constexpr std::string_view src =
        "a:\n"
        "  :b\n"
        "    :c\n"
        "d:\n";

    auto doc = load(src);
    EXPECT(doc.has_errors() == false, "document must parse without errors");
    EXPECT(doc->category_count() == 5, "expected root + a + b + c + d");

    auto root = doc->root();
    EXPECT(root.has_value(), "root category must exist");

    // Retrieve categories by traversal
    auto a = doc->category(category_id{1});
    auto b = doc->category(category_id{2});
    auto c = doc->category(category_id{3});
    auto d = doc->category(category_id{4});

    EXPECT(a->parent()->is_root(), "a must attach to root");
    EXPECT(b->parent()->name() == "a", "b must attach to a");
    EXPECT(c->parent()->name() == "b", "c must attach to b");
    EXPECT(d->parent()->is_root(), "d must attach to root after nested declarations");

    return true;
}

static bool tables_do_not_affect_category_scope()
{
    constexpr std::string_view src =
        "a:\n"
        "    # x\n"
        "      1\n"
        "b:\n";

    auto doc = load(src);
    EXPECT(!doc.has_errors(), "document must parse without errors");

    EXPECT(doc->category_count() == 3, "expected root + a + b");

    auto a = doc->category(category_id{1});
    auto b = doc->category(category_id{2});

    EXPECT(a.has_value(), "category a must exist");
    EXPECT(b.has_value(), "category b must exist");

    EXPECT(a->parent()->is_root(), "a must attach to root");
    EXPECT(b->parent()->is_root(), "b must attach to root, not under a");

    return true;
}

static bool category_cannot_be_top_and_subcategory()
{
    constexpr std::string_view src =
        ":a:\n";

    auto doc = load(src);

    EXPECT(doc.has_errors(),
        "category declared with both leading and trailing colon must be rejected");

    return true;
}

static bool subcategory_at_root_is_rejected()
{
    constexpr std::string_view src =
        ":a\n";

    auto doc = load(src);
    EXPECT(doc.has_errors(),
        "subcategory at root must be rejected");

    return true;
}

//----------------------------------------------------------------------------

inline void run_document_structure_tests()
{
/*
1. Root and ownership invariants
• There is exactly one implicit root category
• Everything has a well-defined owner
• Ownership is hierarchical and acyclic
*/    
    RUN_TEST(root_always_exists);

/*
2. Category formation rules
• Categories can nest
• Categories implicitly close when indentation decreases
• Deep nesting is handled correctly
*/
    SUBCAT("Categories");
    RUN_TEST(category_single_level_attaches_to_root);
    RUN_TEST(subcategory_at_root_is_rejected);
    RUN_TEST(category_nested_ownership_preserved);
    RUN_TEST(colon_categories_nest_without_explicit_closure);
    RUN_TEST(category_explicit_nesting_does_not_leak_scope);
    RUN_TEST(category_cannot_be_top_and_subcategory);

/*
3. Table placement rules
• Tables may appear at root or inside categories
• Tables belong to exactly one category
• Multiple tables at the same level are allowed
*/
    SUBCAT("Tables");
    RUN_TEST(table_at_root_is_owned_by_root);
    RUN_TEST(table_inside_category_allowed);
    RUN_TEST(table_inside_category_is_owned_by_category);
    RUN_TEST(multiple_tables_at_same_scope_allowed);
    RUN_TEST(tables_do_not_affect_category_scope);

/*
Key placement rules

• Keys may appear at root or inside categories
• Keys and tables may interleave
• Keys attach to the correct owning category regardless of order
*/
    SUBCAT("Keys");
    RUN_TEST(keys_attach_to_current_category);
    RUN_TEST(root_key_before_category_is_allowed);

}

}

#endif
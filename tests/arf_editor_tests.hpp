#ifndef ARF_TESTS_EDITOR__
#define ARF_TESTS_EDITOR__

#include "arf_test_harness.hpp"
#include "../include/arf_editor.hpp"
#include "../include/arf.hpp"

namespace arf::tests
{
using namespace arf;

#define with(x) if (x; []{ return true; }())

inline bool update_single_typed_key()
{
    constexpr std::string_view src =
        "a:int = 42\n"
        "# x  y\n"
        "  1  2\n"
        "  3  4\n";

    auto ctx = load(src);
    auto & doc = ctx.document;
        
    EXPECT(ctx.errors.empty(), "error emitted");
    
    auto ed = editor(doc);

    auto key_view = doc.key("a");
    EXPECT(key_view.has_value(), "The key 'a' should exist");
    
    auto & val = key_view->value();
    ed.set_key_value(key_view->id(), 13);

    // Check document node consistency:
    {
        auto kn = ed._unsafe_access_internal_document_container(key_view->id());
        EXPECT (kn != nullptr, "Should find key by ID in key nodes");
        EXPECT(val.held_type() == kn->type, "The held type of the value and the node type should be the same");
        EXPECT(val.held_type() == kn->value.type, "The held type of the value and the value type should be the same");
    }

    // Check value consistency:
    EXPECT(val.held_type() == value_type::integer, "Node should be of integer type");
    EXPECT(std::get<int64_t>(val.val) == 13, "Node value should be 13");

    return true;
}

inline bool change_key_type()
{
    constexpr std::string_view src =
        "a:int = 42\n";

    auto ctx = load(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    auto & doc = ctx.document;
    auto ed = editor(doc);

    auto key_view = doc.key("a");
    EXPECT(key_view.has_value(), "Missing key");

    ed.set_key_value(key_view->id(), std::string("hello"));

    auto & val = key_view->value();

    EXPECT(val.held_type() == value_type::string, "Type should change to string");
    EXPECT(std::get<std::string>(val.val) == "hello", "Value mismatch");

    return true;
}

inline bool key_scalar_to_array()
{
    constexpr std::string_view src =
        "a:int = 1\n";

    auto ctx = load(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    auto & doc = ctx.document;
    auto ed = editor(doc);

    auto key_view = doc.key("a");
    EXPECT(key_view.has_value(), "Missing key");

    std::vector<value> vals = { 1, 2, 3 };
    ed.set_key_value(key_view->id(), vals);

    auto & val = key_view->value();
    EXPECT(val.held_type() == value_type::integer_array, "Should become integer array");

    auto & vec = std::get<std::vector<typed_value>>(val.val);
    EXPECT(vec.size() == 3, "Array size mismatch");

    return true;
}

inline bool mutate_key_array_element()
{
    constexpr std::string_view src =
        "a:int = 1\n";

    auto ctx = load(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    auto & doc = ctx.document;
    auto ed = editor(doc);

    auto key_view = doc.key("a");
    std::vector<value> vals = { 1, 2, 3 };
    ed.set_key_value(key_view->id(), vals);

    ed.set_array_element(key_view->id(), 1, 42);

    auto & vec = std::get<std::vector<typed_value>>(key_view->value().val);
    EXPECT(std::get<int64_t>(vec[1].val) == 42, "Array element not updated");

    return true;
}

inline bool append_key_array_element()
{
    constexpr std::string_view src =
        "a:int = 1\n";

    auto ctx = load(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    auto & doc = ctx.document;
    auto ed = editor(doc);

    auto key_view = doc.key("a");
    ed.set_key_value(key_view->id(), std::vector<value>{1});

    ed.append_array_element(key_view->id(), 5);

    auto & vec = std::get<std::vector<typed_value>>(key_view->value().val);
    EXPECT(vec.size() == 2, "Append failed");
    EXPECT(std::get<int64_t>(vec[1].val) == 5, "Wrong appended value");

    return true;
}

inline bool set_table_cell_value()
{
    constexpr std::string_view src =
        "# a  b\n"
        "  1  2\n";

    auto ctx = load(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    auto & doc = ctx.document;
    editor ed = editor(doc);

    auto tbl = doc.table(table_id{0});
    auto row = doc.row(tbl->rows().front());
    auto target = tbl->column("a");
    EXPECT(target.has_value(), "should be a column 'a'");
    ed.set_cell_value(row->id(), target->id(), 99);

    auto const & cell = row->cells().front();
    EXPECT(std::get<int64_t>(cell.val) == 99, "Cell not updated");

    return true;
}

inline bool table_cell_scalar_to_array()
{
    constexpr std::string_view src =
        "# a\n"
        "  1\n";

    auto ctx = load(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    auto & doc = ctx.document;
    auto ed = editor(doc);

    auto tbl = doc.table(table_id{0});
    auto row = doc.row(tbl->rows().front());

    ed.set_cell_value(row->id(), tbl->column("a")->id(), std::vector<value>{1,2});

    auto const & cell = row->cells().front();
    EXPECT(cell.held_type() == value_type::integer_array, "Type mismatch");

    return true;
}

inline bool append_table_array_element()
{
    constexpr std::string_view src =
        "# a\n"
        "  1\n";

    auto ctx = load(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    auto & doc = ctx.document;
    auto ed = editor(doc);

    auto tbl = doc.table(table_id{0});
    auto row = doc.row(tbl->rows().front());

    ed.set_cell_value(row->id(), tbl->column("a")->id(), std::vector<value>{1});
    ed.append_array_element(row->id(), tbl->column("a")->id(), 7);

    with (auto vec = std::get<std::vector<typed_value>>(row->cells().front().val))
        EXPECT(vec.size() == 2, "Append failed");

    return true;
}

inline bool append_row_test()
{
    constexpr std::string_view src =
        "# a b\n"
        "  1 2\n";

    auto ctx = load(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    auto & doc = ctx.document;
    auto ed = editor(doc);

    auto tbl = doc.table(table_id{0});
    auto original = tbl->row_count();

    ed.append_row(tbl->id(), {}); // empty vector will yield row filled with invalid cells

    EXPECT(tbl->row_count() == original + 1, "Row not appended");

    return true;
}

inline bool append_column_test()
{
    constexpr std::string_view src =
        "# a\n"
        "  1\n";

    auto ctx = load(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    auto & doc = ctx.document;
    auto ed = editor(doc);

    auto tbl = doc.table(table_id{0});
    auto original = tbl->column_count();

    ed.append_column(tbl->id(), "b", value_type::integer);

    EXPECT(tbl->column_count() == original + 1, "Column not appended");

    return true;
}

inline bool invalid_array_contamination_key()
{
    constexpr std::string_view src =
        "a:int = 1\n";

    auto ctx = load(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    auto & doc = ctx.document;
    auto ed = editor(doc);

    auto key_view = doc.key("a");

    ed.set_key_value(key_view->id(), std::vector<value>{1, std::string("x")});

    EXPECT(key_view->value().semantic == semantic_state::invalid,
        "Key should be invalid");

    return true;
}

inline bool column_type_change_invalidates_rows_untyped_col()
{
    constexpr std::string_view src = 
        "# a\n"      // Untyped column
        "  1\n";     // String value "1"
    
    auto ctx = load(src);
    auto & doc = ctx.document;
    auto ed = editor(doc);
    
    auto tbl = doc.table(table_id{0});
    auto col = tbl->column("a");
    
    // Change to INTEGER type (cell holds string "1")
    ed.set_column_type(col->id(), value_type::integer);  // ← Mismatch!
    
    auto row = doc.row(tbl->rows().front());
    
    EXPECT(row->is_contaminated(),
        "Row should be contaminated by invalid cell");
    EXPECT(row->is_locally_valid(),
        "Row structure is still valid");
    
    EXPECT(row->cells()[0].semantic == semantic_state::invalid,
        "Cell should be invalid due to type mismatch");
    
    return true;
}

inline bool column_type_change_invalidates_rows_typed_col()
{
    constexpr std::string_view src = 
        "# a:int\n"   // Integer column
        "  1\n";      // Integer value
    
    auto ctx = load(src);
    auto & doc = ctx.document;
    auto ed = editor(doc);
    
    auto tbl = doc.table(table_id{0});
    auto col = tbl->column("a");
    
    // Change to STRING type (cell holds int64_t)
    ed.set_column_type(col->id(), value_type::string);
    
    auto row = doc.row(tbl->rows().front());
    
    EXPECT(row->is_contaminated(),
        "Row should be contaminated by invalid cell");
    
    return true;
}

// Test insertion ordering
inline bool insert_key_maintains_order()
{
    auto ctx = load("a = 1\nc = 3\n");
    editor ed(ctx.document);
    
    auto key_a = ctx.document.key("a");
    ed.insert_key_after(key_a->id(), "b", 2);
    
    // Verify order by walking ordered_items
    auto cat = ctx.document.root();
    auto* cat_node = ed._unsafe_access_internal_document_container(cat->id());
    
    std::vector<std::string> names;
    for (auto const& item : cat_node->ordered_items)
    {
        if (std::holds_alternative<key_id>(item.id))
        {
            auto kid = std::get<key_id>(item.id);
            auto k = ctx.document.key(kid);
            if (k) names.push_back(std::string(k->name()));
        }
    }
    
    EXPECT(names.size() == 3, "Wrong key count");
    EXPECT(names[0] == "a", "Order wrong: first should be 'a'");
    EXPECT(names[1] == "b", "Order wrong: second should be 'b'");
    EXPECT(names[2] == "c", "Order wrong: third should be 'c'");
    
    return true;
}

// Test erasure
bool erase_key_test()
{
    auto ctx = load("a = 1\nb = 2\nc = 3\n");
    editor ed(ctx.document);
    
    auto key_b = ctx.document.key("b");
    bool result = ed.erase_key(key_b->id());
    
    EXPECT(result == true, "Erase should succeed");
    EXPECT(!ctx.document.key("b").has_value(), "Key should be gone");
    EXPECT(ctx.document.key_count() == 2, "Count wrong");
    
    return true;
}

// Test comment/paragraph
bool comment_and_paragraph_operations()
{
    document doc;
    doc.create_root();
    editor ed(doc);
    
    auto root_id = doc.root()->id();
    
    auto cid = ed.append_comment(root_id, "// test");
    auto pid = ed.append_paragraph(root_id, "Some text");
    
    EXPECT(valid(cid), "Comment creation failed");
    EXPECT(valid(pid), "Paragraph creation failed");
    
    ed.set_comment(cid, "// modified");
    auto* cn = ed._unsafe_access_internal_document_container(cid);
    EXPECT(cn->text == "// modified", "Comment not updated");
    
    return true;
}

// Test table insertion
bool insert_table_test()
{
    auto ctx = load("a = 1\n");
    editor ed(ctx.document);
    
    auto root = ctx.document.root();
    auto tid = ed.append_table(root->id(), std::vector<std::string>{"x", "y"});
    
    EXPECT(valid(tid), "Table creation failed");
    auto tbl = ctx.document.table(tid);
    EXPECT(tbl->column_count() == 2, "Column count wrong");
    
    return true;
}

// Test column operations
bool column_insertion_and_deletion()
{
    auto ctx = load("# a\n  1\n");
    editor ed(ctx.document);
    
    auto tbl = ctx.document.table(table_id{0});
    auto col_a = tbl->column("a");
    
    // Insert column after a
    auto col_b = ed.insert_column_after(col_a->id(), "b", value_type::integer);
    EXPECT(valid(col_b), "Column insert failed");
    EXPECT(tbl->column_count() == 2, "Column count wrong");
    
    // Verify row was expanded
    auto row = ctx.document.row(tbl->rows()[0]);
    EXPECT(row->cells().size() == 2, "Row not expanded");
    
    // Erase column
    bool erased = ed.erase_column(col_b);
    EXPECT(erased, "Column erase failed");
    EXPECT(tbl->column_count() == 1, "Column still present");
    
    return true;
}

inline bool minimal_create_categories()
{
    document doc;
    doc.create_root();
    editor ed(doc);

    EXPECT(doc.categories().size() == 1, "Document with only root should have 1 category");
    auto r = doc.root();
    EXPECT(r.has_value(), "Root not found");
    auto cid = ed.append_category(r->id(), "subcat");
    EXPECT(doc.categories().size() == 2, "Document should have 2 categories");
    auto ch = doc.category(cid);
    EXPECT(ch.has_value(), "Just created category should exist");
    EXPECT(ch->name() == "subcat", "Created category has wrong name");
    auto p = ch->parent();
    EXPECT(p.has_value(), "Child should have a valid parent");
    EXPECT(p->is_root(), "Subcategory should be a child of root");

    return true;
}

inline bool category_creation_and_nesting()
{
    document doc;
    doc.create_root();
    editor ed(doc);
    
    auto root = doc.root()->id();
    auto orig_count = doc.category_count();
    
    // Create top-level category (child of root)
    auto cat1 = ed.append_category(root, "settings");
    EXPECT(valid(cat1), "Category creation failed");
    EXPECT(doc.category_count() ==  orig_count + 1, "Wrong category count");
    
    // Create subcategory (child of non-root category)
    auto sub1 = ed.append_category(cat1, "advanced");
    EXPECT(valid(sub1), "Subcategory creation failed");
    EXPECT(doc.category_count() ==  orig_count + 2, "Wrong category count");

    auto sub = doc.category(sub1);
    EXPECT(sub.has_value(), "Created category not found");
    EXPECT(sub->parent()->id() == cat1, "Wrong parent");
    
    // Verify structure
    auto cat1_node = ed._unsafe_access_internal_document_container(cat1);
    EXPECT(cat1_node->name == "settings", "Wrong node name");
    EXPECT(cat1_node->children.size() == 1, "Child not added");
    EXPECT(cat1_node->children[0] == sub1, "Wrong child");
    
    // Verify parent relationships
    auto sub1_node = ed._unsafe_access_internal_document_container(sub1);
    EXPECT(sub1_node->parent == cat1, "Wrong parent for subcategory");
    
    auto cat1_view = doc.category(cat1);
    EXPECT(cat1_view->parent()->is_root(), "Wrong parent for top-level category");
    
    // Cannot delete non-empty category
    bool cannot_delete = ed.erase_category(cat1);
    EXPECT(!cannot_delete, "Should not delete category with children");
    
    // Can delete empty category
    auto empty_cat = ed.append_category(root, "empty");
    bool can_delete = ed.erase_category(empty_cat);
    EXPECT(can_delete, "Should delete empty category");
    
    return true;
}

//============================================================================
// Test Runner
//============================================================================

inline void run_editor_tests()
{
    SUBCAT("Baseline");
    RUN_TEST(update_single_typed_key);

    SUBCAT("Key mutation tests");
    RUN_TEST(change_key_type);
    RUN_TEST(key_scalar_to_array);
    RUN_TEST(mutate_key_array_element);
    RUN_TEST(append_key_array_element);

    SUBCAT("Table cell mutation");
    RUN_TEST(set_table_cell_value);
    RUN_TEST(table_cell_scalar_to_array);
    RUN_TEST(append_table_array_element);

    SUBCAT("Structural edits");
    RUN_TEST(append_row_test);
    RUN_TEST(append_column_test);

    SUBCAT("Contamination / invalidation");
    RUN_TEST(invalid_array_contamination_key);
    RUN_TEST(column_type_change_invalidates_rows_untyped_col);
    RUN_TEST(column_type_change_invalidates_rows_typed_col);

    SUBCAT("Insertion / deletion");
    RUN_TEST(insert_key_maintains_order);
    RUN_TEST(erase_key_test);
    RUN_TEST(comment_and_paragraph_operations);
    RUN_TEST(insert_table_test);
    RUN_TEST(column_insertion_and_deletion);
    RUN_TEST(minimal_create_categories);
    RUN_TEST(category_creation_and_nesting);
}

}

#endif
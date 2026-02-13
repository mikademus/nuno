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
    EXPECT(val.held_type() == value_type::int_array, "Should become integer array");

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
    EXPECT(cell.held_type() == value_type::int_array, "Type mismatch");

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
}

}

#endif
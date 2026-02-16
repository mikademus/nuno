#ifndef ARF_TESTS_SERIALIZER__
#define ARF_TESTS_SERIALIZER__

#include "arf_test_harness.hpp"
#include "../include/arf_serializer.hpp"
#include "../include/arf_editor.hpp"
#include "../include/arf.hpp"

namespace arf::tests
{
using namespace arf;

#define with(x) if (x; []{ return true; }())

static bool roundtrip()
{
    constexpr std::string_view src =
        "a = 42\n"
        "# x  y\n"
        "  1  2\n"
        "  3  4\n";

    auto ctx = load(src);
        
    EXPECT(ctx.errors.empty(), "error emitted");

    std::ostringstream out;
    auto s = serializer(ctx.document);
    s.write(out);

    EXPECT(out.str() == src, "Authored source not preserved");

    return true;
}  

//============================================================================
// CATEGORY 1: Roundtrip / Preservation Tests
//============================================================================

static bool roundtrip_simple_key()
{
    constexpr std::string_view src = "a = 42\n";
    
    auto ctx = load(src);
    EXPECT(!ctx.has_errors(), "parse error");
    
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == src, "simple key not preserved");
    return true;
}

static bool roundtrip_typed_key()
{
    constexpr std::string_view src = "x:int = 42\n";
    
    auto ctx = load(src);
    EXPECT(!ctx.has_errors(), "parse error");
    
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == src, "typed key not preserved");
    return true;
}

static bool roundtrip_multiple_keys()
{
    constexpr std::string_view src = 
        "a = 1\n"
        "b = 2\n"
        "c = 3\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == src, "multiple keys not preserved");
    return true;
}

static bool roundtrip_category_simple()
{
    constexpr std::string_view src = 
        "top:\n"
        "    a = 1\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);

    EXPECT(out.str() == src, "simple category not preserved");
    return true;
}

static bool roundtrip_nested_categories()
{
    constexpr std::string_view src = 
        "a:\n"
        "    :b\n"
        "        x = 1\n"
        "    /b\n"
        "/a\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == src, "nested categories not preserved");
    return true;
}

static bool roundtrip_table()
{
    constexpr std::string_view src = 
        "# a  b\n"
        "  1  2\n"
        "  3  4\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == src, "table not preserved");
    return true;
}

static bool roundtrip_typed_table()
{
    constexpr std::string_view src = 
        "# name:str  hp:int\n"
        "  alice     100\n"
        "  bob       75\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == src, "typed table not preserved");
    return true;
}

static bool roundtrip_table_in_category()
{
    constexpr std::string_view src = 
        "npcs:\n"
        "    # name  hp\n"
        "      npc1  10\n"
        "      npc2  20\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
        
    EXPECT(out.str() == src, "table in category not preserved");
    return true;
}

static bool roundtrip_comments()
{
    constexpr std::string_view src = 
        "// Header comment\n"
        "a = 1\n"
        "// Footer comment\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == src, "comments not preserved");
    return true;
}

static bool roundtrip_paragraphs()
{
    constexpr std::string_view src = 
        "This is prose text.\n"
        "\n"
        "a = 1\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == src, "paragraphs not preserved");
    return true;
}

static bool roundtrip_array_key()
{
    constexpr std::string_view src = "arr:int[] = 1|2|3\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == src, "array key not preserved");
    return true;
}

static bool roundtrip_complex_document()
{
    constexpr std::string_view src = 
        "// Configuration file\n"
        "version = 1.0\n"
        "\n"
        "server:\n"
        "    host = localhost\n"
        "    port:int = 8080\n"
        "    \n"
        "    :ssl\n"
        "        enabled = true\n"
        "        cert = /path/to/cert\n"
        "    /ssl\n"
        "/server\n"
        "\n"
        "users:\n"
        "    # name   role    active\n"
        "      alice  admin   true\n"
        "      bob    user    false\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
        
    EXPECT(out.str() == src, "complex document not preserved");
    return true;
}

//============================================================================
// CATEGORY 2: Edit Detection Tests
//============================================================================

static bool edited_key_reconstructed()
{
    constexpr std::string_view src = "x = 1\n";
    
    auto ctx = load(src);
    
    // Edit the key
    auto keys = ctx.document.keys();
    EXPECT(keys.size() == 1, "There should be exactly one key");
    auto * node = const_cast<document::key_node *>(keys.front().node);
    node->value.val = int64_t(42);
    node->is_edited = true;
    
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == "x = 42\n", "edited key not reconstructed");
    return true;
}

static bool edited_key_type_shown()
{
    constexpr std::string_view src = "x = 1\n";
    
    auto ctx = load(src);
    
    // Edit and change type
    auto keys = ctx.document.keys();
    EXPECT(keys.size() == 1, "There should be exactly one key");
    auto * key = const_cast<document::key_node *>(keys.front().node);

    key->value.type = value_type::integer;
    key->value.type_source = type_ascription::declared;
    key->value.val = int64_t(42);
    key->is_edited = true;
    
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == "x:int = 42\n", "edited key type annotation incorrect");
    return true;
}

static bool edited_row_reconstructed()
{
    constexpr std::string_view src = 
        "# a  b\n"
        "  1  2\n";
    
    auto ctx = load(src);

    // Columns are UNTYPED, so editing should emit actual variant
    auto rows = ctx.document.rows();
    EXPECT(rows.size() == 1, "There should be exactly one key");
    auto * row = const_cast<document::row_node *>(rows.front().node);
 
    // auto& row = ctx.document.access_row_nodes()[0];
    row->cells[0].val = int64_t(99);  // Direct assignment OK
    row->is_edited = true;
    
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    std::string expected = 
        "# a  b\n"
        "  99  2\n";  // Emits "99" because variant holds int64_t
    
    EXPECT(out.str() == expected, "edited row not reconstructed");
    return true;
}

static bool edited_typed_row_respects_declaration()
{
    constexpr std::string_view src = 
        "# name:str  hp:int\n"
        "  alice     100\n";
    
    auto ctx = load(src);

    // Edit with WRONG type - but column is declared
    auto rows = ctx.document.rows();
    EXPECT(rows.size() == 1, "There should be exactly one key");
    auto * row = const_cast<document::row_node *>(rows.front().node);

    row->cells[1].val = std::string("99");  // String in int column!
    row->is_edited = true;
    
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
            
    // Column is :int, so must emit as integer
    std::string expected = 
        "# name:str  hp:int\n"
        "  alice  99\n";  // "99" converted from string to int
    
    EXPECT(out.str() == expected, "declared type not respected");
    return true;
}

static bool mixed_edited_and_authored()
{
    constexpr std::string_view src = 
        "a = 1\n"
        "b = 2\n"
        "c = 3\n";
    
    auto ctx = load(src);
    
    // Edit only the middle key
    auto keys = ctx.document.keys();
    EXPECT(keys.size() == 3, "There should be exactly three keys");
    auto * key_b = const_cast<document::key_node *>(keys[1].node);

    key_b->value.val = int64_t(99);
    key_b->is_edited = true;
    
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    std::string expected = 
        "a = 1\n"
        "b = 99\n"  // Reconstructed
        "c = 3\n";   // Preserved
    
    EXPECT(out.str() == expected, "mixed edited/authored incorrect");
    return true;
}

//============================================================================
// CATEGORY 3: Generated Document Tests (no source)
//============================================================================


static bool generated_document_simple()
{
    auto doc = create_document();
    auto ed = editor(doc);

    ed.append_key(category_id{0}, "test", int64_t(42), true);
    
    std::ostringstream out;
    serializer s(doc);
    s.write(out);
    
    EXPECT(out.str() == "test = 42\n", "generated key incorrect");
    return true;
}

static bool generated_category()
{
    auto doc = create_document();
    auto ed = editor(doc);
    
    auto cat_id = ed.append_category(category_id{0}, "test");
    ed.append_key(cat_id, "x", int64_t(1), true);
    
    EXPECT(doc.category_count() == 2, "Should be two categories");
    EXPECT(doc.key_count() == 1, "should be one key");
    EXPECT(doc.categories().back().name() == "test", "wrong category name");
    EXPECT(doc.keys().front().name() == "x", "wrong category name");  
  
    std::ostringstream out;
    serializer s(doc);
    s.write(out);
    
    std::string expected = "test:\n    x = 1\n";
    EXPECT(out.str() == expected, "generated category incorrect");
    return true;
}

//============================================================================
// CATEGORY 4: Serializer Options Tests
//============================================================================

static bool option_force_explicit_types()
{
    constexpr std::string_view src = "x = 42\n";
    
    auto ctx = load(src);
    
    serializer_options opts;
    opts.types = serializer_options::type_policy::force_explicit;
    
    std::ostringstream out;
    serializer s(ctx.document, opts);
    s.write(out);
        
    EXPECT(out.str() == "x:int = 42\n", "force_explicit types failed");
    return true;
}

static bool option_force_tacit_types()
{
    constexpr std::string_view src = "x:int = 42\n";
    
    auto ctx = load(src);
    
    serializer_options opts;
    opts.types = serializer_options::type_policy::force_tacit;
    
    std::ostringstream out;
    serializer s(ctx.document, opts);
    s.write(out);
            
    EXPECT(out.str() == "x = 42\n", "force_tacit types failed");
    return true;
}

static bool option_skip_comments()
{
    constexpr std::string_view src = 
        "// Comment\n"
        "a = 1\n";
    
    auto ctx = load(src);
    
    serializer_options opts;
    opts.emit_comments = false;
    
    std::ostringstream out;
    serializer s(ctx.document, opts);
    s.write(out);
    
    EXPECT(out.str() == "a = 1\n", "skip comments failed");
    return true;
}

static bool option_compact_blank_lines()
{
    constexpr std::string_view src = 
        "a = 1\n"
        "\n"
        "\n"
        "b = 2\n";
    
    auto ctx = load(src);
    
    serializer_options opts;
    opts.blank_lines = serializer_options::blank_line_policy::compact;
    
    std::ostringstream out;
    serializer s(ctx.document, opts);
    s.write(out);
        
    std::string expected = "a = 1\nb = 2\n";
    EXPECT(out.str() == expected, "compact blank lines failed");
    return true;
}

//============================================================================
// CATEGORY 5: Edge Cases
//============================================================================

static bool empty_document()
{
    auto doc = create_document();
    
    std::ostringstream out;
    serializer s(doc);
    s.write(out);
    
    EXPECT(out.str() == "", "empty document should produce empty output");
    return true;
}

static bool document_without_source()
{
    // Load then clear source to simulate generated doc
    auto ctx = load("a = 1\n", {.own_parser_data = false});
    
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == "a = 1\n", "document without source should reconstruct");
    return true;
}

static bool array_with_empty_elements()
{
    constexpr std::string_view src = "arr:str[] = a||b|\n";
    
    auto ctx = load(src);
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str() == src, "array with empty elements not preserved");
    return true;
}

static bool indent_inferred_from_sibling_key()
{
    // Create a minimal document with source context
    constexpr std::string_view src = 
        "test:\n"
        "    authored_key = 1\n";
    
    auto ctx = load(src);
    EXPECT(!ctx.has_errors(), "document should parse without errors");
    
    // Now add a generated key to the same category
    auto cat = ctx.document.category("test");
    EXPECT(cat.has_value(), "test category must exist");

    auto ed = editor(ctx.document);
    ed.append_key(cat->id(), "generated_key", int64_t(2), true);

    // Serialize
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    std::string expected = 
        "test:\n"
        "    authored_key = 1\n"
        "    generated_key = 2\n";  // Should infer 4-space indent from sibling

    EXPECT(out.str() == expected, "generated key should infer indent from authored sibling");
    
    return true;
}

static bool indent_fallback_when_no_siblings()
{
    // Create document programmatically (no source context)
    auto doc = create_document();
    editor ed(doc);
    
    // auto cat_id = doc.create_category("test", category_id{0});
    auto cat_id = ed.append_category(category_id{0}, "test");
    
    // Create generated key with no siblings
    ed.append_key(cat_id, "solo_key", int64_t(42), true);

    // Serialize
    std::ostringstream out;
    serializer s(doc);
    s.write(out);
    
    std::string expected = 
        "test:\n"
        "    solo_key = 42\n";
    
    EXPECT(out.str() == expected, "fallback indent incorrect");
    
    return true;
}

//============================================================================
// Test Runner
//============================================================================

inline void run_seriealizer_tests()
{
    SUBCAT("Roundtrip - Preservation");
    RUN_TEST(roundtrip);
    RUN_TEST(roundtrip_simple_key);
    RUN_TEST(roundtrip_typed_key);
    RUN_TEST(roundtrip_multiple_keys);
    RUN_TEST(roundtrip_category_simple);
    RUN_TEST(roundtrip_nested_categories);
    RUN_TEST(roundtrip_table);
    RUN_TEST(roundtrip_typed_table);
    RUN_TEST(roundtrip_table_in_category);
    RUN_TEST(roundtrip_comments);
    RUN_TEST(roundtrip_paragraphs);
    RUN_TEST(roundtrip_array_key);
    RUN_TEST(roundtrip_complex_document);
    
    SUBCAT("Edit Detection");
    RUN_TEST(edited_key_reconstructed);
    RUN_TEST(edited_key_type_shown);
    RUN_TEST(edited_row_reconstructed);
    RUN_TEST(edited_typed_row_respects_declaration);
    RUN_TEST(mixed_edited_and_authored);
    
    SUBCAT("Generated Documents");
    RUN_TEST(generated_document_simple);
    RUN_TEST(generated_category);
    
    SUBCAT("Serializer Options");
    RUN_TEST(option_force_explicit_types);
    RUN_TEST(option_force_tacit_types);
    RUN_TEST(option_skip_comments);
    RUN_TEST(option_compact_blank_lines);

    SUBCAT("Indentation inference");
    RUN_TEST(indent_inferred_from_sibling_key);
    RUN_TEST(indent_fallback_when_no_siblings);  
    
    SUBCAT("Edge Cases");
    RUN_TEST(empty_document);
    RUN_TEST(document_without_source);
    RUN_TEST(array_with_empty_elements);
}

} // ns arf::tests

#endif
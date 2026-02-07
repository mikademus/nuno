#ifndef ARF_TESTS_SERIALIZER__
#define ARF_TESTS_SERIALIZER__

#include "arf_test_harness.hpp"
#include "../include/arf_serializer.hpp"
#include "../include/arf.hpp"

namespace arf::tests
{
using namespace arf;

#define with(x) if (x; []{ return true; }())

static bool foo()
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
    auto& key = ctx.document.access_key_nodes()[0];
    key.value.val = int64_t(42);
    key.is_edited = true;
    
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
    auto& key = ctx.document.access_key_nodes()[0];
    key.value.type = value_type::integer;
    key.value.type_source = type_ascription::declared;
    key.value.val = int64_t(42);
    key.is_edited = true;
    
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
    auto& row = ctx.document.access_row_nodes()[0];
    row.cells[0].val = int64_t(99);  // Direct assignment OK
    row.is_edited = true;
    
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    std::string expected = 
        "# a  b\n"
        "    99  2\n";  // Emits "99" because variant holds int64_t
    
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
    auto& row = ctx.document.access_row_nodes()[0];
    row.cells[1].val = std::string("99");  // String in int column!
    row.is_edited = true;
    
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    // Column is :int, so must emit as integer
    std::string expected = 
        "# name:str  hp:int\n"
        "    alice  99\n";  // "99" converted from string to int
    
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
    auto& key_b = ctx.document.access_key_nodes()[1];
    key_b.value.val = int64_t(99);
    key_b.is_edited = true;
    
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
    document doc;
    doc.create_root();
    
    // Manually create a key
    document::key_node k;
    k.id = key_id{0};
    k.name = "test";
    k.owner = category_id{0};
    k.value.type = value_type::integer;
    k.value.val = int64_t(42);
    k.value.type_source = type_ascription::tacit;
    
    doc.access_key_nodes().push_back(k);
    with (auto & root = doc.access_category_nodes()[0])
    {
        root.keys.push_back(key_id{0});
        root.ordered_items.push_back(document::source_item_ref{key_id{0}});
    }
    
    std::ostringstream out;
    serializer s(doc);
    s.write(out);
    
    EXPECT(out.str() == "test = 42\n", "generated key incorrect");
    return true;
}

static bool generated_category()
{
    document doc;
    doc.create_root();
    
    auto cat_id = doc.create_category(category_id{1}, "test", category_id{0});
    
    // Add key to category
    document::key_node k;
    k.id = key_id{0};
    k.name = "x";
    k.owner = cat_id;
    k.value.type = value_type::integer;
    k.value.val = int64_t(1);
    
    doc.access_key_nodes().push_back(k);
    with (auto & cat = doc.access_category_nodes())
    {
        cat[1].keys.push_back(key_id{0});
        cat[0].ordered_items.push_back(document::source_item_ref{cat_id});
        cat[1].ordered_items.push_back(document::source_item_ref{key_id{0}});
    }
    
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
    document doc;
    doc.create_root();
    
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

//============================================================================
// Test Runner
//============================================================================

inline void run_seriealizer_tests()
{
    SUBCAT("Roundtrip - Preservation");
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
    
    SUBCAT("Edge Cases");
    RUN_TEST(empty_document);
    RUN_TEST(document_without_source);
    RUN_TEST(array_with_empty_elements);
}

} // ns arf::tests

#endif
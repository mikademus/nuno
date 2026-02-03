#ifndef ARF_TESTS_PARSER__
#define ARF_TESTS_PARSER__

#include "arf_test_harness.hpp"
#include "../include/arf_parser.hpp"

namespace arf::tests
{
//------------------------------------------
// HELPERS
//------------------------------------------

    inline const cst_key& first_key(const parse_context& ctx)
    {
        if (ctx.document.keys.empty()) 
            throw std::out_of_range("cst document empty!");
        return ctx.document.keys.front();
    }    

    inline std::vector<column> extract_table_columns(const parse_context& ctx)
    {
        std::vector<column> out;

        for (const auto& tbl : ctx.document.tables)
        {
            for (const auto& col : tbl.columns)
                out.push_back(col);
        }

        return out;
    }

//------------------------------------------
// TESTS
//------------------------------------------

static bool test_parser_marks_tacit_types_unresolved()
{
    constexpr std::string_view src =
        "a = 42\n"
        "# x  y\n"
        "  1  2\n";

    auto ctx = parse(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    // Key
    const auto& key = first_key(ctx);
    EXPECT(!key.declared_type.has_value(), "type was declared");

    // Columns
    auto cols = extract_table_columns(ctx);
    EXPECT(cols.size() == 2, "incorrect table arity");
    EXPECT(cols[0].type == value_type::unresolved, "type not designated unresolved");
    EXPECT(cols[1].type == value_type::unresolved, "type not designated unresolved");

    return true;
}

static bool test_parser_records_declared_types_without_validation()
{
    constexpr std::string_view src =
        "x:int = hello\n"
        "# a:float\n"
        "  world\n";

    auto ctx = parse(src);
    EXPECT(ctx.errors.empty(), "error emitted");

    const auto& key = first_key(ctx);
    EXPECT(key.declared_type.has_value(), "no declared type for key");
    EXPECT(*key.declared_type == "int", "incorrect declared type for key");

    auto cols = extract_table_columns(ctx);
    EXPECT(cols[0].declared_type.has_value(), "no declared type for column");
    EXPECT(*cols[0].declared_type == "float", "incorrect declared type for column");

    return true;
}    

//----------------------------------------------------------------------
// Comment event emission
//----------------------------------------------------------------------

static bool parser_emits_single_comment()
{
    constexpr std::string_view src = "// This is a comment\n";
    
    auto ctx = parse(src);
    
    EXPECT(ctx.document.events.size() == 1, "Expected one event");
    EXPECT(ctx.document.events[0].kind == parse_event_kind::comment, "Expected comment event");
    EXPECT(ctx.document.events[0].text == "// This is a comment", "Comment text incorrect");
    
    return true;
}

static bool parser_blobs_consecutive_comments()
{
    constexpr std::string_view src = 
        "// Comment line 1\n"
        "// Comment line 2\n"
        "// Comment line 3\n";
    
    auto ctx = parse(src);
    
    EXPECT(ctx.document.events.size() == 1, "Expected single blobbed comment");
    EXPECT(ctx.document.events[0].kind == parse_event_kind::comment, 
           "Expected comment event");
    
    std::string expected = "// Comment line 1\n// Comment line 2\n// Comment line 3";
    EXPECT(ctx.document.events[0].text == expected, 
           "Multi-line comment blob incorrect");
    
    return true;
}

static bool parser_flushes_comment_on_structural_token()
{
    constexpr std::string_view src = 
        "// Comment\n"
        "key = value\n";
    
    auto ctx = parse(src);
    
    EXPECT(ctx.document.events.size() == 2, "Expected comment + key events");
    EXPECT(ctx.document.events[0].kind == parse_event_kind::comment, 
           "First event should be comment");
    EXPECT(ctx.document.events[1].kind == parse_event_kind::key_value, 
           "Second event should be key");
    
    return true;
}

//----------------------------------------------------------------------
// Paragraph event emission
//----------------------------------------------------------------------

static bool parser_emits_paragraph_for_nongrammar_text()
{
    constexpr std::string_view src = "This is not a valid Arf construct\n";
    
    auto ctx = parse(src);
    
    EXPECT(ctx.document.events.size() == 1, "Expected one event");
    EXPECT(ctx.document.events[0].kind == parse_event_kind::paragraph, 
           "Expected paragraph event");
    EXPECT(ctx.document.events[0].text == "This is not a valid Arf construct", 
           "Paragraph text incorrect");
    
    return true;
}

static bool parser_emits_paragraph_for_empty_line()
{
    constexpr std::string_view src = "\n";
    
    auto ctx = parse(src);
    
    EXPECT(ctx.document.events.size() == 1, "Expected one event");
    EXPECT(ctx.document.events[0].kind == parse_event_kind::paragraph, 
           "Expected paragraph event for empty line");
    EXPECT(ctx.document.events[0].text == "", 
           "Empty paragraph should have empty text");
    
    return true;
}

static bool parser_blobs_consecutive_paragraphs()
{
    constexpr std::string_view src = 
        "Paragraph line 1\n"
        "Paragraph line 2\n"
        "Paragraph line 3\n";
    
    auto ctx = parse(src);
    
    EXPECT(ctx.document.events.size() == 1, "Expected single blobbed paragraph");
    EXPECT(ctx.document.events[0].kind == parse_event_kind::paragraph, 
           "Expected paragraph event");
    
    std::string expected = "Paragraph line 1\nParagraph line 2\nParagraph line 3";
    EXPECT(ctx.document.events[0].text == expected, 
           "Multi-line paragraph blob incorrect");
    
    return true;
}

static bool parser_flushes_paragraph_on_structural_token()
{
    constexpr std::string_view src = 
        "Some text\n"
        "key = value\n";
    
    auto ctx = parse(src);
    
    EXPECT(ctx.document.events.size() == 2, "Expected paragraph + key events");
    EXPECT(ctx.document.events[0].kind == parse_event_kind::paragraph, 
           "First event should be paragraph");
    EXPECT(ctx.document.events[1].kind == parse_event_kind::key_value, 
           "Second event should be key");
    
    return true;
}

//----------------------------------------------------------------------
// Malformed structural tokens preserved as paragraphs
//----------------------------------------------------------------------

static bool parser_preserves_malformed_key_as_paragraph()
{
    constexpr std::string_view src = 
        "a:\n"
        "    invalid key without equals\n"
        "    b = 2\n";
    
    auto ctx = parse(src);
    
    // Should have: category_open, paragraph, key_value
    bool found_paragraph = false;
    for (const auto& ev : ctx.document.events)
    {
        if (ev.kind == parse_event_kind::paragraph && 
            ev.text.find("invalid key without equals") != std::string::npos)
        {
            found_paragraph = true;
        }
    }
    
    EXPECT(found_paragraph, "Malformed key should become paragraph");
    
    return true;
}

static bool parser_preserves_malformed_category_close_as_comment()
{
    constexpr std::string_view src = 
        "/invalid_close_at_root\n"  // this becomes a comment
        "key = value\n";
    
    auto ctx = parse(src);
    
    // Malformed close becomes a comment, not discarded
    EXPECT(ctx.document.events.size() == 2, "Expected comment + key events");
    EXPECT(ctx.document.events[0].kind == parse_event_kind::comment, 
           "Malformed close should become comment");
    EXPECT(ctx.document.events[1].kind == parse_event_kind::key_value, 
           "Key event should follow");
    
    return true;
}

//----------------------------------------------------------------------
// Comment/paragraph alternation
//----------------------------------------------------------------------

static bool parser_alternates_comments_and_paragraphs()
{
    constexpr std::string_view src = 
        "// Comment 1\n"
        "Paragraph 1\n"
        "// Comment 2\n"
        "Paragraph 2\n";
    
    auto ctx = parse(src);
    
    EXPECT(ctx.document.events.size() == 4, "Expected 4 separate blobs");
    EXPECT(ctx.document.events[0].kind == parse_event_kind::comment, "Event 0");
    EXPECT(ctx.document.events[1].kind == parse_event_kind::paragraph, "Event 1");
    EXPECT(ctx.document.events[2].kind == parse_event_kind::comment, "Event 2");
    EXPECT(ctx.document.events[3].kind == parse_event_kind::paragraph, "Event 3");
    
    return true;
}

//----------------------------------------------------------------------
// End-of-document flush
//----------------------------------------------------------------------

static bool parser_flushes_pending_at_eof()
{
    constexpr std::string_view src = 
        "key = value\n"
        "// Comment at end";  // No trailing newline
    
    auto ctx = parse(src);
    
    EXPECT(ctx.document.events.size() == 2, "Expected key + comment");
    EXPECT(ctx.document.events[0].kind == parse_event_kind::key_value, "Event 0");
    EXPECT(ctx.document.events[1].kind == parse_event_kind::comment, "Event 1");
    EXPECT(ctx.document.events[1].text == "// Comment at end", 
           "Final comment should be flushed");
    
    return true;
}

//----------------------------------------------------------------------
// Integration: mixed content
//----------------------------------------------------------------------

static bool parser_handles_mixed_content()
{
    constexpr std::string_view src = 
        "// File header comment\n"
        "// comment stretches across two lines\n"
        "\n"
        "This is a preamble paragraph.\n"
        "\n"
        "settings:\n"
        "    // Nested comment\n"
        "    key = value\n"
        "\n"
        "# table  header\n"
        "  row1   data1\n"
        "  // Row comment\n"
        "  row2   data2\n";
    
    auto ctx = parse(src);
    
    // Verify we have a reasonable mix of events
    size_t comment_count = 0;
    size_t paragraph_count = 0;
    size_t structural_count = 0;
    
    for (const auto& ev : ctx.document.events)
    {
        if (ev.kind == parse_event_kind::comment) ++comment_count;
        else if (ev.kind == parse_event_kind::paragraph) ++paragraph_count;
        else ++structural_count;
    }
    
    EXPECT(comment_count == 3, "Expected 3 comments");
    EXPECT(paragraph_count == 2, "Expected 2 paragraphs (blobbed)");
    EXPECT(structural_count > 0, "Expected structural events");
    
    return true;
}

static bool parser_key_terminates_table()
{
    constexpr std::string_view src = 
        "# a  b\n"
        "  1  2\n"
        "  key = value\n"  // This terminates the table
        "  3  4\n";        // This becomes a paragraph
    
    auto ctx = parse(src);
    
    // Should have: table_header, row, key_value, paragraph
    size_t table_rows = 0;
    size_t keys = 0;
    size_t paragraphs = 0;
    
    for (const auto& ev : ctx.document.events)
    {
        if (ev.kind == parse_event_kind::table_row) ++table_rows;
        if (ev.kind == parse_event_kind::key_value) ++keys;
        if (ev.kind == parse_event_kind::paragraph) ++paragraphs;
    }
    
    EXPECT(table_rows == 1, "Should have one table row");
    EXPECT(keys == 1, "Should have one key");
    EXPECT(paragraphs == 1, "Orphaned row data becomes paragraph");
    
    return true;
}

// Todo:
// * comments inside tables
// * malformed rows still producing events
// * Windows vs Unix line endings
// * Unicode preservation
// * malformed arrays surviving tokenisation

//----------------------------------------------------------------------------

inline void run_parser_tests()
{
    RUN_TEST(test_parser_marks_tacit_types_unresolved);
    RUN_TEST(test_parser_records_declared_types_without_validation);

    SUBCAT("Comment emission");
    RUN_TEST(parser_emits_single_comment);
    RUN_TEST(parser_blobs_consecutive_comments);
    RUN_TEST(parser_flushes_comment_on_structural_token);
    
    SUBCAT("Paragraph emission");
    RUN_TEST(parser_emits_paragraph_for_nongrammar_text);
    RUN_TEST(parser_emits_paragraph_for_empty_line);
    RUN_TEST(parser_blobs_consecutive_paragraphs);
    RUN_TEST(parser_flushes_paragraph_on_structural_token);
    
    SUBCAT("Malformed tokens");
    RUN_TEST(parser_preserves_malformed_key_as_paragraph);
    RUN_TEST(parser_preserves_malformed_category_close_as_comment);
    RUN_TEST(parser_key_terminates_table);
    
    SUBCAT("Alternation and flushing");
    RUN_TEST(parser_alternates_comments_and_paragraphs);
    RUN_TEST(parser_flushes_pending_at_eof);
    
    SUBCAT("Integration");
    RUN_TEST(parser_handles_mixed_content);
}

} // ns arf::tests

#endif
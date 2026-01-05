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
}

} // ns arf::tests

#endif
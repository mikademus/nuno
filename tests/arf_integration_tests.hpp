#ifndef ARF_TESTS_COMPREHENSIVE__
#define ARF_TESTS_COMPREHENSIVE__

#include "arf_test_harness.hpp"
#include "../include/arf_query.hpp"
#include "../include/arf_editor.hpp"
#include "../include/arf_serializer.hpp"
#include "../include/arf.hpp"

namespace arf::tests
{
using namespace arf;

//#define with(x) if (x; []{ return true; }())

bool workflow_query_edit_serialize()
{
    constexpr std::string_view src = 
        "config:\n"
        "    version = 1\n"
        "    debug = true\n";
    
    // Parse
    auto ctx = load(src);
    EXPECT(!ctx.has_errors(), "Parse failed");
    
    // Query
    auto handle = query(ctx.document, "config.version");
    auto version = handle.as_integer();
    EXPECT(version.has_value(), "Query failed");
    
    // Get ID for editing
    auto kid = handle.key_id();
    EXPECT(kid.has_value(), "Should resolve to key");
    
    // Edit
    editor ed(ctx.document);
    ed.set_key_value(*kid, 2);
    
    // Serialize
    std::ostringstream out;
    serializer s(ctx.document);
    s.write(out);
    
    EXPECT(out.str().find("version = 2") != std::string::npos, 
           "Serialization didn't reflect edit");
    
    return true;
}

// Test: Generate programmatically → Serialize → Parse → Query
bool workflow_generate_serialize_parse()
{
    std::string serialized;
    {
        // Generate
        document doc;
        doc.create_root();
        editor ed(doc);
        
        auto root = doc.root()->id();
        auto cat = ed.append_category(root, "data");
        ed.append_key(cat, "count", 100);
        ed.append_key(cat, "items", std::vector<value>{1, 2, 3});
        
        // Serialize
        std::ostringstream out;
        serializer s(doc);
        s.write(out);
        
        serialized = out.str();
    }
    
    // Re-parse
    auto ctx = load(serialized);
    auto & doc = ctx.document;
    EXPECT(!ctx.has_errors(), "Re-parse failed");
    
    // Query
    auto handle = query(doc, "data.count");
    EXPECT(!handle.locations().empty(), "Query should have resolved");
    EXPECT(!handle.ambiguous(), "Query should be unambiguous");
    auto count = handle.as_integer();
    EXPECT(count.has_value(), "Query should be extractable as intenger");
    EXPECT(*count == 100, "Value lost");
    
    return true;
}

// Test: Complex multi-edit scenario
bool workflow_complex_document_construction()
{
    document doc;
    doc.create_root();
    editor ed(doc);
    
    auto root = doc.root()->id();
    
    // Build structure
    ed.append_comment(root, "// Configuration file");
    auto cat_db = ed.append_category(root, "database");
    ed.append_key(cat_db, "host", std::string("localhost"));
    ed.append_key(cat_db, "port", 5432);
    
    auto tid = ed.append_table(root, {
        {"name", value_type::string},
        {"age", value_type::integer}
    });
    
    ed.append_row(tid, {std::string("Alice"), 30});
    ed.append_row(tid, {std::string("Bob"), 25});
    
    // Verify structure
    EXPECT(doc.category_count() == 2, "Category count wrong");  // root + database
    EXPECT(doc.table_count() == 1, "Table count wrong");
    
    auto tbl = doc.table(tid);
    EXPECT(tbl->row_count() == 2, "Row count wrong");
    
    return true;
}

//============================================================================
// Test Runner
//============================================================================

inline void run_integration_tests()
{
    SUBCAT("Cross-API Workflows");
    RUN_TEST(workflow_query_edit_serialize);
    RUN_TEST(workflow_generate_serialize_parse);
    RUN_TEST(workflow_complex_document_construction);
}

}
#endif

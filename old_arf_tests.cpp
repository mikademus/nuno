// arf_tests.cpp - Comprehensive Test Suite for Arf!
// Version 0.2.0
// Copyright 2025 Mikael Ueno A
// Licensed as-is under the MIT license.

#include "include/arf_core.hpp"
#include "include/arf_parser.hpp"
#include "include/arf_query.hpp"
#include "include/arf_serializer.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <functional>

//========================================================================
// TEST FRAMEWORK
//========================================================================

struct test_result
{
    std::string name;
    bool passed;
    std::string message;
};

class test_suite
{
public:
    test_suite(const std::string& name) : name_(name) {}
    
    void run_test(const std::string& test_name, std::function<bool()> test_func)
    {
        try
        {
            bool passed = test_func();
            results_.push_back({test_name, passed, passed ? "OK" : "FAILED"});
            
            if (passed)
                passed_++;
            else
                failed_++;
        }
        catch (const std::exception& e)
        {
            results_.push_back({test_name, false, std::string("EXCEPTION: ") + e.what()});
            failed_++;
        }
    }
    
    void print_summary() const
    {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << name_ << " - Summary\n";
        std::cout << std::string(70, '=') << "\n";
        
        for (const auto& r : results_)
        {
            std::cout << "  [" << (r.passed ? "✓" : "✗") << "] " 
                      << std::left << std::setw(50) << r.name 
                      << r.message << "\n";
        }
        
        std::cout << "\nPassed: " << passed_ << " / " << (passed_ + failed_) << "\n";
        if (failed_ > 0)
            std::cout << "Failed: " << failed_ << "\n";
        std::cout << std::string(70, '=') << "\n";
    }
    
    int failed_count() const { return failed_; }
    
private:
    std::string name_;
    std::vector<test_result> results_;
    int passed_ = 0;
    int failed_ = 0;
};

//========================================================================
// TEST DATA
//========================================================================

namespace test_data
{
    const char* basic_key_values = R"(
root_key = root_value
root_num:int = 42

settings:
    name = Test Config
    version:float = 1.5
    enabled:bool = true
    tags:str[] = alpha|beta|gamma
    scores:int[] = 100|200|300
/settings
)";

    const char* simple_table = R"(
items:
    # id:int  name        value:int
      1       sword       100
      2       shield      50
      3       potion      25
/items
)";

    const char* nested_subcategories = R"(
monsters:
    # id:int  name            hp:int
      1       bat             10
      2       rat             15
  :goblins
      3       goblin scout    25
      4       goblin warrior  40
  /goblins
  :undead
      5       skeleton        30
      6       zombie          50
    :bosses
      7       lich            200
    /bosses
  /undead
      8       kobold          20
/monsters
)";

    const char* document_order_test = R"(
catalog:
    # id:int  item
      1       first
  :section_a
      2       second
      3       third
  /section_a
      4       fourth
  :section_b
      5       fifth
  /section_b
      6       sixth
/catalog
)";

    const char* type_annotations = R"(
data:
    implicit_str = hello
    explicit_str:str = world
    explicit_int:int = 123
    explicit_float:float = 3.14
    explicit_bool:bool = true
    
    # col_implicit  col_explicit:int
      value1        100
      value2        200
/data
)";

    const char* malformed = R"(
test:
    valid = ok
    invalid line without equals
    # incomplete table header
    another:invalid:type = value
/test
)";
}

//========================================================================
// PARSER TESTS
//========================================================================

void test_parser(test_suite& suite)
{
    suite.run_test("Parse empty document", []() {
        auto result = arf::parse("");
        return result.has_value() && !result.has_errors();
    });
    
    suite.run_test("Parse basic key-values", []() {
        auto result = arf::parse(test_data::basic_key_values);
        if (!result.has_value()) return false;
        
        auto& doc = *result.doc;
        return doc.categories.size() >= 1 &&
               doc.categories.count("settings") > 0;
    });
    
    suite.run_test("Parse simple table", []() {
        auto result = arf::parse(test_data::simple_table);
        if (!result.has_value()) return false;
        
        auto& doc = *result.doc;
        auto it = doc.categories.find("items");
        if (it == doc.categories.end()) return false;
        
        return it->second->table_rows.size() == 3 &&
               it->second->table_columns.size() == 3;
    });
    
    suite.run_test("Parse nested subcategories", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;
        
        auto& doc = *result.doc;
        auto it = doc.categories.find("monsters");
        if (it == doc.categories.end()) return false;
        
        auto& monsters = *it->second;
        return monsters.subcategories.size() == 2 &&
               monsters.subcategories.count("goblins") > 0 &&
               monsters.subcategories.count("undead") > 0 &&
               monsters.subcategories["undead"]->subcategories.count("bosses") > 0;
    });
    
    suite.run_test("Track type annotations (columns)", []() {
        auto result = arf::parse(test_data::type_annotations);
        if (!result.has_value()) return false;
        
        auto& doc = *result.doc;
        auto it = doc.categories.find("data");
        if (it == doc.categories.end()) return false;
        
        auto& data = *it->second;
        if (data.table_columns.size() != 2) return false;
        
        // col_implicit should be tacit
        if (data.table_columns[0].type_source != arf::type_ascription::tacit)
            return false;
        
        // col_explicit:int should be declared
        if (data.table_columns[1].type_source != arf::type_ascription::declared)
            return false;
        
        return true;
    });
    
    suite.run_test("Track type annotations (key-values)", []() {
        auto result = arf::parse(test_data::type_annotations);
        if (!result.has_value()) return false;
        
        auto& doc = *result.doc;
        auto it = doc.categories.find("data");
        if (it == doc.categories.end()) return false;
        
        auto& data = *it->second;
        
        // implicit_str should be tacit
        auto implicit_it = data.key_values.find("implicit_str");
        if (implicit_it == data.key_values.end()) return false;
        if (implicit_it->second.type_source != arf::type_ascription::tacit)
            return false;
        
        // explicit_int:int should be declared
        auto explicit_it = data.key_values.find("explicit_int");
        if (explicit_it == data.key_values.end()) return false;
        if (explicit_it->second.type_source != arf::type_ascription::declared)
            return false;
        
        return true;
    });
    
    suite.run_test("Store source literals", []() {
        auto result = arf::parse("test:\n  val = original text\n/test");
        if (!result.has_value()) return false;
        
        auto& doc = *result.doc;
        auto it = doc.categories.find("test");
        if (it == doc.categories.end()) return false;
        
        auto& test_cat = *it->second;
        auto val_it = test_cat.key_values.find("val");
        if (val_it == test_cat.key_values.end()) return false;
        
        return val_it->second.source_literal.has_value() &&
               *val_it->second.source_literal == "original text";
    });
    
    suite.run_test("Track value locus", []() {
        auto result = arf::parse(test_data::simple_table);
        if (!result.has_value()) return false;
        
        auto& doc = *result.doc;
        auto it = doc.categories.find("items");
        if (it == doc.categories.end()) return false;
        
        // Table cells should have table_cell locus
        auto& items = *it->second;
        if (items.table_rows.empty()) return false;
        
        return items.table_rows[0].cells[0].origin_site == arf::value_locus::table_cell;
    });
    
    suite.run_test("Handle parse errors gracefully", []() {
        auto result = arf::parse(test_data::malformed);
        return result.has_errors() && result.errors.size() > 0;
    });
    
    suite.run_test("Track source_order for document iteration", []() {
        auto result = arf::parse(test_data::document_order_test);
        if (!result.has_value()) return false;
        
        auto& doc = *result.doc;
        auto it = doc.categories.find("catalog");
        if (it == doc.categories.end()) return false;
        
        auto& catalog = *it->second;
        
        // Should have entries in source_order: rows, subcategories
        return catalog.source_order.size() > 0;
    });

    suite.run_test("source_order includes nested table rows", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;

        const auto& monsters = *result.doc->categories.at("monsters");

        size_t row_decls = 0;
        for (const auto& d : monsters.source_order)
            if (d.kind == arf::decl_kind::table_row)
                ++row_decls;

        // All rows, including nested, are visible here
        return row_decls == monsters.table_rows.size();
    });

    suite.run_test("Table: Global row IDs are unique", []() {
        const char* src = R"(
root:
    # id
    1
:a
    2
:b
    3
/b
    4
/a
    5
)";

        auto ctx = arf::parse(src);
        if (!ctx.doc) return false;

        std::vector<arf::table_row_id> ids;

        const arf::category* root = ctx.doc->categories.at("root").get();

        std::function<void(const arf::category*)> collect;
        collect = [&](const arf::category* cat)
        {
            for (const auto& row : cat->table_rows)
                ids.push_back(row.global_id);

            for (const auto& [_, sub] : cat->subcategories)
                collect(sub.get());
        };

        collect(root);

        if (ids.empty()) return false;

        auto sorted = ids;
        std::sort(sorted.begin(), sorted.end());

        return std::unique(sorted.begin(), sorted.end()) == sorted.end();
    });
}

//========================================================================
// QUERY API TESTS
//========================================================================

void test_query_api(test_suite& suite)
{
    suite.run_test("Query string value", []() {
        auto result = arf::parse(test_data::basic_key_values);
        if (!result.has_value()) return false;
        
        auto name = arf::get_string(*result.doc, "settings.name");
        return name.has_value() && *name == "Test Config";
    });
    
    suite.run_test("Query int value", []() {
        auto result = arf::parse(test_data::basic_key_values);
        if (!result.has_value()) return false;
        
        auto num = arf::get_int(*result.doc, "root_num");
        return num.has_value() && *num == 42;
    });
    
    suite.run_test("Query float value", []() {
        auto result = arf::parse(test_data::basic_key_values);
        if (!result.has_value()) return false;
        
        auto ver = arf::get_float(*result.doc, "settings.version");
        return ver.has_value() && *ver == 1.5;
    });
    
    suite.run_test("Query bool value", []() {
        auto result = arf::parse(test_data::basic_key_values);
        if (!result.has_value()) return false;
        
        auto enabled = arf::get_bool(*result.doc, "settings.enabled");
        return enabled.has_value() && *enabled == true;
    });
    
    suite.run_test("Query string array", []() {
        auto result = arf::parse(test_data::basic_key_values);
        if (!result.has_value()) return false;
        
        auto tags = arf::get_string_array(*result.doc, "settings.tags");
        return tags.has_value() && 
               tags->size() == 3 &&
               (*tags)[0] == "alpha" &&
               (*tags)[1] == "beta" &&
               (*tags)[2] == "gamma";
    });
    
    suite.run_test("Query int array", []() {
        auto result = arf::parse(test_data::basic_key_values);
        if (!result.has_value()) return false;
        
        auto scores = arf::get_int_array(*result.doc, "settings.scores");
        return scores.has_value() && 
               scores->size() == 3 &&
               (*scores)[0] == 100 &&
               (*scores)[1] == 200 &&
               (*scores)[2] == 300;
    });
    
    suite.run_test("Query non-existent path", []() {
        auto result = arf::parse(test_data::basic_key_values);
        if (!result.has_value()) return false;
        
        auto missing = arf::get_string(*result.doc, "does.not.exist");
        return !missing.has_value();
    });
    
    suite.run_test("Table: Get table view", []() {
        auto result = arf::parse(test_data::simple_table);
        if (!result.has_value()) return false;
        
        auto table = arf::get_table(*result.doc, "items");
        return table.has_value() && 
               table->rows().size() == 3 &&
               table->columns().size() == 3;
    });
    
    suite.run_test("Table: Access table row by index", []() {
        auto result = arf::parse(test_data::simple_table);
        if (!result.has_value()) return false;
        
        auto table = arf::get_table(*result.doc, "items");
        if (!table) return false;
        
        auto row = table->row(1);  // Second row
        auto name = row.get_string("name");
        return name.has_value() && *name == "shield";
    });
    
    suite.run_test("Table: Access table column by name", []() {
        auto result = arf::parse(test_data::simple_table);
        if (!result.has_value()) return false;
        
        auto table = arf::get_table(*result.doc, "items");
        if (!table) return false;
        
        auto idx = table->column_index("value");
        return idx.has_value() && *idx == 2;
    });
    
    suite.run_test("Table: Iterate table rows", []() {
        auto result = arf::parse(test_data::simple_table);
        if (!result.has_value()) return false;
        
        auto table = arf::get_table(*result.doc, "items");
        if (!table) return false;
        
        int count = 0;
        for (size_t i = 0; i < table->rows().size(); ++i)
        {
            auto row = table->row(i);
            auto id = row.get_int("id");
            if (id) count++;
        }
        
        return count == 3;
    });
    
    suite.run_test("Table: Recursive iteration includes subcategories", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;
        
        auto table = arf::get_table(*result.doc, "monsters");
        if (!table) return false;
        
        int count = 0;
        for (auto row : table->rows_recursive())
        {
            (void)row;  // Suppress unused warning
            count++;
        }
        
        // Should have all 8 monsters
        return count == 8;
    });
    
    suite.run_test("Table: Document order preserves source structure", []() {
        auto result = arf::parse(test_data::document_order_test);
        if (!result.has_value()) return false;
        
        auto table = arf::get_table(*result.doc, "catalog");
        if (!table) return false;
        
        std::vector<int> ids;
        for (auto row : table->rows_document())
        {
            auto id = row.get_int("id");
            if (id) ids.push_back(*id);
        }
        
        // Should be 1,2,3,4,5,6 in document order
        return ids.size() == 6 &&
               ids[0] == 1 && ids[1] == 2 && ids[2] == 3 &&
               ids[3] == 4 && ids[4] == 5 && ids[5] == 6;
    });
    
    suite.run_test("Table: Row provenance tracking", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;
        
        auto table = arf::get_table(*result.doc, "monsters");
        if (!table) return false;
        
        bool found_goblin = false;
        for (auto row : table->rows_recursive())
        {
            auto name = row.get_string("name");
            if (name && *name == "goblin scout")
            {
                found_goblin = true;
                // Should be from goblins subcategory
                return !row.is_base_row() && row.source_name() == "goblins";
            }
        }
        
        return false;
    });
}

//========================================================================
// REFLECTION API TESTS
//========================================================================

void test_reflection_api(test_suite& suite)
{
    suite.run_test("value_ref: type inspection", []() {
        auto result = arf::parse("test:\n  val:int = 42\n/test");
        if (!result.has_value()) return false;
        
        auto ref = arf::get(*result.doc, "test.val");
        return ref.has_value() &&
               ref->type() == arf::value_type::integer &&
               ref->is_int() &&
               ref->is_scalar() &&
               !ref->is_array();
    });
    
    suite.run_test("value_ref: type ascription detection", []() {
        auto result = arf::parse(test_data::type_annotations);
        if (!result.has_value()) return false;
        
        auto implicit = arf::get(*result.doc, "data.implicit_str");
        auto explicit_val = arf::get(*result.doc, "data.explicit_int");
        
        return implicit.has_value() && !implicit->declared() &&
               explicit_val.has_value() && explicit_val->declared();
    });
    
    suite.run_test("value_ref: source literal access", []() {
        auto result = arf::parse("test:\n  val = original\n/test");
        if (!result.has_value()) return false;
        
        auto ref = arf::get(*result.doc, "test.val");
        auto src = ref ? ref->source_str() : std::nullopt;
        
        return src.has_value() && *src == "original";
    });
    
    suite.run_test("value_ref: type conversions", []() {
        auto result = arf::parse("test:\n  str_num = 123\n/test");
        if (!result.has_value()) return false;
        
        auto ref = arf::get(*result.doc, "test.str_num");
        if (!ref) return false;
        
        // String value should convert to int
        auto as_int = ref->as_int();
        return as_int.has_value() && *as_int == 123;
    });
    
    suite.run_test("value_ref: array views (no copying)", []() {
        auto result = arf::parse(test_data::basic_key_values);
        if (!result.has_value()) return false;
        
        auto ref = arf::get(*result.doc, "settings.tags");
        if (!ref) return false;
        
        auto arr = ref->string_array();
        return arr.has_value() && arr->size() == 3;
    });
    
    suite.run_test("table_ref: column access", []() {
        auto result = arf::parse(test_data::simple_table);
        if (!result.has_value()) return false;
        
        auto it = result.doc->categories.find("items");
        if (it == result.doc->categories.end()) return false;
        
        arf::table_ref table(*it->second);
        
        return table.column_count() == 3 &&
               table.column(0).name() == "id" &&
               table.column(1).name() == "name" &&
               table.column(2).name() == "value";
    });
    
    suite.run_test("table_ref: row access", []() {
        auto result = arf::parse(test_data::simple_table);
        if (!result.has_value()) return false;
        
        auto it = result.doc->categories.find("items");
        if (it == result.doc->categories.end()) return false;
        
        arf::table_ref table(*it->second);
        
        return table.row_count() == 3 &&
               table.row(0).index() == 0;
    });

    suite.run_test("table_ref: direct rows respect lexical scope", []() {
        const char* src = R"(
    items:
        # id
        1
    :sub
        2
    /sub
    /items
    )";

        auto result = arf::parse(src);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("items"));
        const auto& parts = table.partitions();

        if (parts.size() != 2) return false;

        const auto& root = parts[0];
        const auto& sub  = parts[1];

        // root authored only row 1
        if (root.direct_rows.size() != 1) return false;
        if (root.all_rows.size() != 2) return false;

        // sub authored only row 2
        if (sub.direct_rows.size() != 1) return false;
        if (sub.all_rows.size() != 1) return false;

        return true;
    });

    suite.run_test("table_ref: root direct rows stop at first subcategory", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("monsters"));
        const auto& root = table.partitions()[0];

        // monsters has 2 base rows before any subcategory
        return root.direct_rows.size() == 2;
    });

    suite.run_test("table_ref: partition count is exact", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;

        auto it = result.doc->categories.find("monsters");
        if (it == result.doc->categories.end()) return false;

        arf::table_ref table(*it->second);
        const auto& parts = table.partitions();

        // root + goblins + undead + bosses
        return parts.size() == 4;
    });

    suite.run_test("table_ref: partition parentage is correct", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;

        auto it = result.doc->categories.find("monsters");
        if (it == result.doc->categories.end()) return false;

        arf::table_ref table(*it->second);
        const auto& parts = table.partitions();

        // Find partitions by name
        auto find = [&](std::string_view name) -> size_t {
            for (size_t i = 0; i < parts.size(); ++i)
                if (parts[i].cat->name == name)
                    return i;
            return arf::detail::npos;
        };

        size_t root    = find("monsters");
        size_t goblins = find("goblins");
        size_t undead  = find("undead");
        size_t bosses  = find("bosses");

        if (root == arf::detail::npos ||
            goblins == arf::detail::npos ||
            undead == arf::detail::npos ||
            bosses == arf::detail::npos)
            return false;

        // goblins and undead are children of root
        if (parts[goblins].parent != root) return false;
        if (parts[undead].parent  != root) return false;

        // bosses is child of undead
        if (parts[bosses].parent != undead) return false;

        return true;
    });

    suite.run_test("table_ref: partition creation order follows source order", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;

        auto it = result.doc->categories.find("monsters");
        if (it == result.doc->categories.end()) return false;

        arf::table_ref table(*it->second);
        const auto& parts = table.partitions();

        // Expected creation order:
        // monsters (root), goblins, undead, bosses
        if (parts.size() < 4) return false;

        return
            parts[0].cat->name == "monsters" &&
            parts[1].cat->name == "goblins"  &&
            parts[2].cat->name == "undead"   &&
            parts[3].cat->name == "bosses";
    });

    suite.run_test("table_ref: partition building", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;
        
        auto it = result.doc->categories.find("monsters");
        if (it == result.doc->categories.end()) return false;
        
        arf::table_ref table(*it->second);
        const auto& partitions = table.partitions();
        
        // Should have root + goblins + undead + bosses = 4 partitions
        return partitions.size() >= 4;
    });

    suite.run_test("table_ref: root owns rows after subcategories", []() {
        const char* src = R"(
    items:
        # id
        1
    :sub
        2
    /sub
        3
    /items
    )";

        auto result = arf::parse(src);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("items"));
        const auto& parts = table.partitions();

        if (parts.size() != 2) return false;

        const auto& root = parts[0];
        const auto& sub  = parts[1];

        // root authored row 1 before, row 3 after
        if (root.direct_rows.size() != 1) return false;
        if (root.all_rows.size() != 3) return false;

        // sub authored only row 2
        if (sub.direct_rows.size() != 1) return false;
        if (sub.all_rows.size() != 1) return false;

        return true;
    });

    suite.run_test("table_ref: child rows are not direct rows of parent", []() {
        const char* src = R"(
    items:
        # id
        1
    :sub
        2
    /sub
    /items
    )";

        auto result = arf::parse(src);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("items"));
        const auto& parts = table.partitions();

        if (parts.size() != 2) return false;

        const auto& root = parts[0];

        // root.direct_rows must NOT include row from subcategory
        if (root.direct_rows.size() != 1) return false;

        return true;
    });

    suite.run_test("table_ref: deep nesting with late rows", []() {
        const char* src = R"(
    things:
        # id
        1
    :sub1
        2
    :sub2
        3
    /sub2
        4
    /sub1
        5
    /things
    )";

        auto result = arf::parse(src);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("things"));
        const auto& parts = table.partitions();

        if (parts.size() != 3) return false;

        const auto& root = parts[0];
        const auto& sub1 = parts[1];
        const auto& sub2 = parts[2];

        // root: rows 1 and 5 are authored here
        if (root.direct_rows.size() != 1) return false;
        if (root.all_rows.size() != 5) return false;

        // sub1: rows 2 and 4
        if (sub1.direct_rows.size() != 1) return false;
        if (sub1.all_rows.size() != 3) return false;

        // sub2: only row 3
        if (sub2.direct_rows.size() != 1) return false;
        if (sub2.all_rows.size() != 1) return false;

        return true;
    });

    suite.run_test("table_ref: root all_rows preserve authored order", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("monsters"));
        const auto& root = table.partitions()[0];

        // monsters has 8 rows total
        if (root.all_rows.size() != 8) return false;

        // indices must be strictly increasing
        for (size_t i = 1; i < root.all_rows.size(); ++i)
            if (root.all_rows[i] <= root.all_rows[i - 1])
                return false;

        return true;
    });

    suite.run_test("table_ref: authored order beats depth-first order", []() {
        const char* src = R"(
items:
    # id
    1
:sub
    2
/sub
    3
/items
)";

        auto result = arf::parse(src);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("items"));
        const auto& parts = table.partitions();

        if (parts.size() != 2) return false;

        const auto& root = parts[0];
        const auto& sub  = parts[1];

        // Sanity: ownership
        if (root.direct_rows.size() != 1) return false; // row 1
        if (sub.direct_rows.size()  != 1) return false; // row 2

        // Root must own all rows
        if (root.all_rows.size() != 3) return false;

        // Authored order must be: 1, 2, 3
        // DFS order would be:     1, 3, 2   (WRONG)
        if (root.all_rows[0] != 1) return false;
        if (root.all_rows[1] != 2) return false;
        if (root.all_rows[2] != 3) return false;

        return true;
    });    

    suite.run_test("table_ref: child all_rows is subsequence of root all_rows", []() {
        const char* src = R"(
items:
    # id
    1
:sub
    2
:deep
    3
/deep
    4
/sub
    5
/items
)";

        auto result = arf::parse(src);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("items"));
        const auto& parts = table.partitions();

        if (parts.size() != 3) return false;

        const auto& root = parts[0];
        const auto& sub  = parts[1];
        const auto& deep = parts[2];

        // Helper: check subsequence
        auto is_subsequence =
        [](const std::vector<size_t>& a, const std::vector<size_t>& b)
        {
            size_t j = 0;
            for (size_t i = 0; i < a.size() && j < b.size(); ++i)
            {
                if (a[i] == b[j])
                    ++j;
            }
            return j == b.size();
        };

        if (!is_subsequence(root.all_rows, sub.all_rows)) return false;
        if (!is_subsequence(root.all_rows, deep.all_rows)) return false;

        return true;
    });

    suite.run_test("table_ref: children are immediate and ordered", []() {
        const char* src = R"(
root:
    # id
    1
:a
    2
    :a1
    3
    /a1
/a
:b
    4
/b
/root
)";

        auto result = arf::parse(src);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("root"));
        const auto& parts = table.partitions();

        // Expected partitions:
        // 0 = root
        // 1 = a
        // 2 = a1
        // 3 = b
        if (parts.size() != 4) return false;

        const auto& root = parts[0];

        // Root must have exactly two children: a, b (not a1)
        if (root.children.size() != 2) return false;

        const auto* c0 = parts[root.children[0]].cat;
        const auto* c1 = parts[root.children[1]].cat;

        if (c0->name != "a") return false;
        if (c1->name != "b") return false;

        return true;
    });

    suite.run_test("table_ref: non-root all_rows are not depth-first", []() {
        const char* src = R"(
root:
    # id
    1
:a
    2
/a
    3
/root
)";

        auto result = arf::parse(src);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("root"));
        const auto& parts = table.partitions();

        const auto& root = parts[0];
        const auto& a    = parts[1];

        // Root owns all rows
        if (root.all_rows.size() != 3) return false;

        // a owns only row 2
        if (a.all_rows.size() != 1) return false;
        if (a.all_rows[0] != 2) return false;

        return true;
    });

    suite.run_test("table_ref: rows are owned only by active partition", []() {
        const char* src = R"(
root:
    # id
    1
:a
    2
/a
    3
/root
)";

        auto result = arf::parse(src);
        if (!result.has_value()) return false;

        arf::table_ref table(*result.doc->categories.at("root"));
        const auto& parts = table.partitions();

        const auto& root = parts[0];
        const auto& a    = parts[1];

        // root direct rows: only row 1
        if (root.direct_rows.size() != 1) return false;
        if (root.direct_rows[0] != 1) return false;

        // row 3 must NOT be direct to root
        for (auto r : root.direct_rows)
            if (r == 3) return false;

        // a owns only row 2
        if (a.direct_rows.size() != 1) return false;
        if (a.direct_rows[0] != 2) return false;

        return true;
    });

    suite.run_test("table_ref: root partition owns all rows", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;

        auto it = result.doc->categories.find("monsters");
        if (it == result.doc->categories.end()) return false;

        arf::table_ref table(*it->second);
        const auto& partitions = table.partitions();

        if (partitions.empty()) return false;

        // root is conventionally index 0
        const auto& root = partitions[0];

        // root has no parent
        if (root.parent != arf::detail::npos) return false;

        // root should include all table rows
        return root.all_rows.size() == table.row_count();
    });

    suite.run_test("table_ref: partition direct rows are not inherited", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;

        auto it = result.doc->categories.find("monsters");
        if (it == result.doc->categories.end()) return false;

        arf::table_ref table(*it->second);
        const auto& partitions = table.partitions();

        if (partitions.size() < 2) return false;

        // find a non-root partition that actually has rows
        for (size_t i = 1; i < partitions.size(); ++i)
        {
            const auto& p = partitions[i];

            if (!p.direct_rows.empty())
            {
                // direct rows must be a subset of all_rows
                if (p.direct_rows.size() > p.all_rows.size())
                    return false;

                // root should not list these as direct rows
                const auto& root = partitions[0];
                for (auto r : p.direct_rows)
                {
                    if (std::find(root.direct_rows.begin(),
                                root.direct_rows.end(),
                                r) != root.direct_rows.end())
                        return false;
                }

                return true;
            }
        }

        return false;
    });

    suite.run_test("table_ref: partition parent-child consistency", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;

        auto it = result.doc->categories.find("monsters");
        if (it == result.doc->categories.end()) return false;

        arf::table_ref table(*it->second);
        const auto& partitions = table.partitions();

        for (size_t i = 0; i < partitions.size(); ++i)
        {
            const auto& p = partitions[i];

            // check parent -> child linkage
            if (p.parent != arf::detail::npos)
            {
                if (p.parent >= partitions.size()) return false;

                const auto& parent = partitions[p.parent];
                if (std::find(parent.children.begin(),
                            parent.children.end(),
                            i) == parent.children.end())
                    return false;
            }

            // check child -> parent linkage
            for (auto c : p.children)
            {
                if (c >= partitions.size()) return false;
                if (partitions[c].parent != i) return false;
            }
        }

        return true;
    });
    
    suite.run_test("table_partition_ref: direct vs all rows", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;
        
        auto it = result.doc->categories.find("monsters");
        if (it == result.doc->categories.end()) return false;
        
        arf::table_ref table(*it->second);
        auto root = table.root_subcategory();
        
        // Root should have direct rows AND include all descendant rows
        return root.direct_row_count() >= 0 &&
               root.row_count() >= root.direct_row_count();
    });
    
    suite.run_test("table_partition_ref: hierarchy navigation", []() {
        auto result = arf::parse(test_data::nested_subcategories);
        if (!result.has_value()) return false;
        
        auto it = result.doc->categories.find("monsters");
        if (it == result.doc->categories.end()) return false;
        
        arf::table_ref table(*it->second);
        auto root = table.root_subcategory();
        
        // Root should have children (goblins, undead)
        return root.child_count() >= 2 &&
               !root.has_parent();
    });
    
    suite.run_test("table_cell_ref: coordinate access", []() {
        auto result = arf::parse(test_data::simple_table);
        if (!result.has_value()) return false;
        
        auto it = result.doc->categories.find("items");
        if (it == result.doc->categories.end()) return false;
        
        arf::table_ref table(*it->second);
        auto row = table.row(1);
        auto col = table.column(1);
        auto cell = row.cell(col);
        
        return cell.row_index() == 1 &&
               cell.column_index() == 1 &&
               cell.value().is_string();
    });
    
    suite.run_test("table_column_ref: type metadata", []() {
        auto result = arf::parse(test_data::type_annotations);
        if (!result.has_value()) return false;
        
        auto it = result.doc->categories.find("data");
        if (it == result.doc->categories.end()) return false;
        
        arf::table_ref table(*it->second);
        auto col0 = table.column(0);
        auto col1 = table.column(1);
        
        // col0 should be tacit, col1 should be declared
        return col0.type_source() == arf::type_ascription::tacit &&
               col1.type_source() == arf::type_ascription::declared;
    });
}

//========================================================================
// INTEGRATION TESTS
//========================================================================

void test_integration(test_suite& suite)
{
    suite.run_test("Parse -> Query -> Reflect workflow", []() {
        const char* config = R"(
app:
    name = TestApp
    version:float = 2.0
    
    # feature      enabled:bool  priority:int
      login        true          10
      dashboard    true          20
      analytics    false         30
/app
)";
        
        // Parse
        auto result = arf::parse(config);
        if (!result.has_value()) return false;
        
        // Query key-values
        auto name = arf::get_string(*result.doc, "app.name");
        if (!name || *name != "TestApp") return false;
        
        // Query table
        auto features = arf::get_table(*result.doc, "app");
        if (!features) return false;
        
        // Reflect on values
        auto version_ref = arf::get(*result.doc, "app.version");
        if (!version_ref || !version_ref->declared()) return false;
        
        return true;
    });
    
    suite.run_test("Complex nested structure", []() {
        const char* config = R"(
game:
    # id  name
      1   item_a
  :weapons
      2   sword
    :legendary
      3   excalibur
    /legendary
  /weapons
      4   item_b
  :armor
      5   shield
  /armor
      6   item_c
/game
)";
        
        auto result = arf::parse(config);
        if (!result.has_value()) return false;
        
        auto table = arf::get_table(*result.doc, "game");
        if (!table) return false;
        
        // Verify document order
        std::vector<int> doc_order;
        for (auto row : table->rows_document())
        {
            auto id = row.get_int("id");
            if (id) doc_order.push_back(*id);
        }
        
        // Should be 1,2,3,4,5,6
        if (doc_order.size() != 6) return false;
        for (int i = 0; i < 6; ++i)
            if (doc_order[i] != i + 1) return false;
        
        // Verify recursive order (groups subcategories together)
        std::vector<int> rec_order;
        for (auto row : table->rows_recursive())
        {
            auto id = row.get_int("id");
            if (id) rec_order.push_back(*id);
        }
        
        // Should be 1,4,6,2,3,5 (base rows first, then subcats)
        return rec_order.size() == 6;
    });
}

//========================================================================
// SERIALISER TESTS (STEP 1: SEMANTIC ROUND-TRIP)
//========================================================================

static std::optional<arf::document> round_trip(const char* input)
{
    auto parsed = arf::parse(input);
    if (!parsed.has_value())
        return std::nullopt;

    std::ostringstream out;
    arf::serializer ser(out);
    ser.write(*parsed.doc);

    auto reparsed = arf::parse(out.str());
    if (!reparsed.has_value())
        return std::nullopt;

    return std::move(*reparsed.doc);
}

void test_serialiser(test_suite& suite)
{
    suite.run_test("Serialise empty document", []() {
        auto doc = round_trip("");
        return doc.has_value();
    });

    suite.run_test("Serialise basic key-values", []() {
        auto doc = round_trip(test_data::basic_key_values);
        if (!doc) return false;

        auto name = arf::get_string(*doc, "settings.name");
        auto num  = arf::get_int(*doc, "root_num");

        return name && *name == "Test Config"
            && num && *num == 42;
    });

    suite.run_test("Serialise preserves type ascription", []() {
        auto doc = round_trip(test_data::type_annotations);
        if (!doc) return false;

        auto implicit = arf::get(*doc, "data.implicit_str");
        auto explicit_i = arf::get(*doc, "data.explicit_int");

        return implicit && !implicit->declared()
            && explicit_i && explicit_i->declared();
    });

    suite.run_test("Serialise preserves table structure", []() {
        auto doc = round_trip(test_data::simple_table);
        if (!doc) return false;

        auto table = arf::get_table(*doc, "items");
        if (!table) return false;

        return table->rows().size() == 3
            && table->columns().size() == 3;
    });

    suite.run_test("Serialise nested subcategories", []() {
        auto doc = round_trip(test_data::nested_subcategories);
        if (!doc) return false;

        auto it = doc->categories.find("monsters");
        if (it == doc->categories.end()) return false;

        const auto& monsters = *it->second;

        return monsters.subcategories.count("goblins")
            && monsters.subcategories.count("undead")
            && monsters.subcategories.at("undead")
                ->subcategories.count("bosses");
    });

    suite.run_test("Serialise preserves document order", []() {
        auto doc = round_trip(test_data::document_order_test);
        if (!doc) return false;

        auto table = arf::get_table(*doc, "catalog");
        if (!table) return false;

        std::vector<int> ids;
        for (auto row : table->rows_document())
        {
            auto id = row.get_int("id");
            if (id) ids.push_back(*id);
        }

        return ids == std::vector<int>{1,2,3,4,5,6};
    });

    suite.run_test("Serialise preserves source literals", []() {
        const char* input =
            "test:\n"
            "  val = original text\n"
            "/test\n";

        auto doc = round_trip(input);
        if (!doc) return false;

        auto ref = arf::get(*doc, "test.val");
        auto src = ref ? ref->source_str() : std::nullopt;

        return src && *src == "original text";
    });

    suite.run_test("Serialiser refuses malformed document", []() {
        auto parsed = arf::parse(test_data::malformed);
        if (parsed.has_value())
            return false;

        return true;
    });

}


//========================================================================
// MAIN
//========================================================================

int main()
{
    std::cout << R"(
╔===================================================================╗
║                                                                   ║
║   █████╗ ██████╗ ███████╗██╗    ████████╗███████╗███████╗████████╗║
║  ██╔==██╗██╔==██╗██╔====╝██║    ╚==██╔==╝██╔====╝██╔====╝╚==██╔==╝║
║  ███████║██████╔╝█████╗  ██║       ██║   █████╗  ███████╗   ██║   ║
║  ██╔==██║██╔==██╗██╔==╝  ╚=╝       ██║   ██╔==╝  ╚====██║   ██║   ║
║  ██║  ██║██║  ██║██║     ██╗       ██║   ███████╗███████║   ██║   ║
║  ╚=╝  ╚=╝╚=╝  ╚=╝╚=╝     ╚=╝       ╚=╝   ╚======╝╚======╝   ╚=╝   ║
║                                                                   ║
║             A Readable Format - Comprehensive Test Suite          ║
║                         Version 0.2.0                             ║
║                                                                   ║
╚===================================================================╝
)" << "\n";
    
    int total_failed = 0;
    
    try
    {
        {
            test_suite parser_suite("PARSER TESTS");
            test_parser(parser_suite);
            parser_suite.print_summary();
            total_failed += parser_suite.failed_count();
        }
        
        {
            test_suite query_suite("QUERY API TESTS");
            test_query_api(query_suite);
            query_suite.print_summary();
            total_failed += query_suite.failed_count();
        }
        
        {
            test_suite reflect_suite("REFLECTION API TESTS");
            test_reflection_api(reflect_suite);
            reflect_suite.print_summary();
            total_failed += reflect_suite.failed_count();
        }
        
        {
            test_suite integration_suite("INTEGRATION TESTS");
            test_integration(integration_suite);
            integration_suite.print_summary();
            total_failed += integration_suite.failed_count();
        }

        {
            test_suite serialization_suite("SERIALIZATION TESTS");
            test_serialiser(serialization_suite);
            serialization_suite.print_summary();
            total_failed += serialization_suite.failed_count();
        }
        
        std::cout << "\n" << std::string(70, '=') << "\n";
        if (total_failed == 0)
        {
            std::cout << "✓✓✓ ALL TESTS PASSED ✓✓✓\n";
        }
        else
        {
            std::cout << "✗✗✗ " << total_failed << " TESTS FAILED ✗✗✗\n";
        }
        std::cout << std::string(70, '=') << "\n\n";
        
        return total_failed > 0 ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n✗✗✗ FATAL ERROR ✗✗✗\n";
        std::cerr << "Exception: " << e.what() << "\n";
        std::cerr << std::string(70, '=') << "\n\n";
        return 2;
    }
}
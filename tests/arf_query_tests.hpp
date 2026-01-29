#ifndef ARF_TESTS_QUERIES__
#define ARF_TESTS_QUERIES__


#pragma once

#include "arf_test_harness.hpp"

#include "../include/arf_query.hpp"
#include "../include/arf.hpp"

#include <iostream>
#include <limits>

namespace arf::tests
{
    using namespace arf;

    // -----------------------------------------------------------------
    // Basic path resolution
    // -----------------------------------------------------------------

    bool dot_path_correctly_split()
    {
        auto const p1 = details::split_dot_path("foo.bar.baz");
        EXPECT(p1.size() == 3, "Should find three items");
        EXPECT(p1[0] == "foo", "1st item should be foo");
        EXPECT(p1[1] == "bar", "2nd item should be bar");
        EXPECT(p1[2] == "baz", "3rd item should be baz");

        return true;
    }
    
    bool find_single_value()
    {
        auto ctx = load(R"(
            world:
                foo = 42
        )");
            
        auto q = query(ctx.document, "world.foo");

        EXPECT(!q.empty(), "There should be a match");
        EXPECT(q.locations().size() == 1, "There should be exactly one match");

        return true;
    }

    bool extract_single_value()
    {
        auto ctx = load(R"(
            world:
                foo = 42
                bar = 3.1415
                baz = foobar
                qux = true
        )");
                
        {
            auto foo = query(ctx.document, "world.foo");
            EXPECT(!foo.empty(), "There should be an integer match");
            auto v = foo.as_integer();
            EXPECT(v.has_value(), "Integer should have been found");
            EXPECT(*v == 42, "Incorrect integer value extracted");
        }

        {
            auto bar = query(ctx.document, "world.bar");
            EXPECT(!bar.empty(), "There should be a real match");
            auto v = bar.as_real();
            EXPECT(v.has_value(), "Real should have been found");
            EXPECT(*v - 3.1415 <= std::numeric_limits<double>::epsilon(), "Incorrect real value extracted");
        }

        {
            auto baz = query(ctx.document, "world.baz");
            EXPECT(!baz.empty(), "There should be a string match");
            auto v = baz.as_string();
            EXPECT(v.has_value(), "String should have been found");
            EXPECT(*v == "foobar", "Incorrect string value extracted");
        }

        {
            auto foo = query(ctx.document, "world.qux");
            EXPECT(!foo.empty(), "There should be a bool match");
            auto v = foo.as_bool();
            EXPECT(v.has_value(), "Bool should have been found");
            EXPECT(*v, "Incorrect bool value extracted");
        }

        return true;
    }

    bool extract_single_value_through_helpers()
    {
        auto ctx = load(R"(
            world:
                foo = 42
                bar = 3.1415
                baz = foobar
                qux = true
        )");
                
        {
            auto v = get_integer(ctx.document, "world.foo");
            EXPECT(v.has_value(), "Integer should have been found");
            EXPECT(*v == 42, "Incorrect integer value extracted");
        }

        {
            auto v = get_real(ctx.document, "world.bar");
            EXPECT(v.has_value(), "Real should have been found");
            EXPECT(*v - 3.1415 <= std::numeric_limits<double>::epsilon(), "Incorrect real value extracted");
        }

        {
            auto v = get_string(ctx.document, "world.baz");
            EXPECT(v.has_value(), "String should have been found");
            EXPECT(*v == "foobar", "Incorrect string value extracted");
        }

        {
            auto v = get_bool(ctx.document, "world.qux");
            EXPECT(v.has_value(), "Bool should have been found");
            EXPECT(*v, "Incorrect bool value extracted");
        }

        return true;
    }

    bool index_follows_array()
    {
        auto ctx = load(R"(
        top:
            arr:int[] = 42|13|1
        )");

        // Establishing ground truth:
        {
            auto keys = ctx.document.keys();
            EXPECT(keys.size() == 1, "There should be one key");
            auto k = ctx.document.key(keys.front().id());
            EXPECT(k.has_value(), "The key should have been extracted");
            EXPECT(k->name() == "arr", "The key should be named arr");
            EXPECT(k->is_array(), "The key should be an array");
            EXPECT(k->value().type == value_type::int_array, "The key should be an array of integers");
            auto const & arr = std::get<std::vector<typed_value>>(k->value().val);
            EXPECT(arr.size() == 3, "The array's arity should be 3");
            EXPECT(arr.at(1).type == value_type::integer && std::get<std::int64_t>(arr.at(1).val) == 13, "arr[1] != 13");
        }

        // Query behaviour
        {
            auto q = query(ctx.document, "top.arr.[1]");

            EXPECT(!q.empty(), "Query should resolve");
            EXPECT(q.locations().size() == 1, "Query should resolve without anbiguity");
            auto v = q.as_integer();
            EXPECT(v.has_value(), "Result should be integer");
            EXPECT(*v == 13, "top.arr.[1] should evaluate to 13");
        }

        return true;
    }

    // -----------------------------------------------------------------
    // Table row selection via predicate
    // -----------------------------------------------------------------

    bool query_table_row_by_string_id()
    {
        auto doc = load(R"(
            world:
                # race   poise
                  elves  friendly
                  orcs   hostile
        )");

        auto ctx =
            query(doc.document, "world")
                .where(eq("race", "orcs"))
                .project("poise");

        EXPECT(!ctx.locations().empty(), "Query should resolve");
        EXPECT(ctx.locations().size() == 1, "Should not be multiple matches");
        EXPECT(ctx.as_string().has_value(), "Result should be string type");
        EXPECT(ctx.as_string().value() == "hostile", "The result is not the expected string");

        return true;
    }

    // -----------------------------------------------------------------
    // Plurality preservation
    // -----------------------------------------------------------------

    bool query_plural_results()
    {
        auto ctx = load(R"(
            world:
                # race   poise
                  elves  friendly
                  orcs   hostile
                  orcs   drunk
        )");

        auto q =
            query(ctx.document, "world")
                .table(0)
                .rows()
                .where(eq("race", "orcs"))
                .project("poise");

        EXPECT(q.locations().size() == 2, "Should resolve to two matches");
        EXPECT(q.locations().front().kind == location_kind::terminal_value, "Plural query resolves to rows, not values");

        return true;
    }

    // -----------------------------------------------------------------
    // Ambiguity does not abort
    // -----------------------------------------------------------------

    bool query_reports_ambiguity()
    {
        auto ctx = load(R"(
            foo:
                bar = 1
                bar = 2
        )");

        auto res = query(ctx.document, "foo.bar");

        EXPECT(res.ambiguous(), "Should be multiple matches");
        EXPECT(!res.issues().empty(), "Should be a diagnostic report attached");

        return true;
    }

    bool ambiguious_query_will_not_extract_to_single()
    {
        auto ctx = load(R"(
            foo:
                bar = 1
                bar = 2
        )");

        auto q = query(ctx.document, "foo.bar");

        EXPECT(q.ambiguous(), "Should be multiple matches");
        auto res = q.as_integer();
        EXPECT(!res.has_value(), "Should not have a value");
        EXPECT(!q.issues().empty(), "Should be a diagnostic report attached");

        return true;
    }
    
    // -----------------------------------------------------------------
    // Table access
    // -----------------------------------------------------------------

    bool find_single_table()
    {
        auto ctx = load(R"(
            topcat:
                # name  id
                  foo   42
                  bar   13
        )");
            
        auto q = query(ctx.document, "topcat").table(0);

        EXPECT(!q.empty(), "The query should resolve");
        EXPECT(q.locations().size() == 1, "There should be exactly one match");
        EXPECT(q.locations().front().kind == location_kind::table_scope, "The match should be a table");

        return true;
    }

    bool find_multiple_tables()
    {
        auto ctx = load(R"(
            topcat:
                # name  id
                  foo   42
                  bar   13

                # qux  quux
                  aaa  baaa
                  bbb  cbbb
        )");
            
        auto q = query(ctx.document, "topcat").tables();

        EXPECT(!q.empty(), "The query should resolve");
        EXPECT(q.locations().size() == 2, "There should be two matches");
        for (auto const & i : q.locations())
            EXPECT(i.kind == location_kind::table_scope, "The matches should all be tables");

        return true;
    }

    bool enumerate_all_rows_in_table()
    {
        auto ctx = load(R"(
            top:
                # a  b
                  1  2
                  3  4
        )");

        auto q = query(ctx.document, "top").table(0).rows();

        EXPECT(!q.empty(), "Rows should resolve");
        EXPECT(q.locations().size() == 2, "Should enumerate both rows");

        for (auto const& loc : q.locations())
            EXPECT(loc.kind == location_kind::row_scope, "All matches should be rows");

        return true;
    }   

    bool enumerate_named_rows_in_table()
    {
        auto ctx = load(R"(
            top:
                # name  value
                  foo   1
                  foo   2
                  bar   3
                  qux   4
                  qux   5
        )");

        auto table = query(ctx.document, "top").table(0);
        EXPECT(!table.empty(), "table should resolve");
        
        auto q0 = query(ctx.document, "top").table(0).rows();
        EXPECT(!q0.empty(), "Rows should resolve");
        EXPECT(q0.locations().size() == 5, "Should enumerate all rows");
        
        for (auto const& loc : q0.locations())
            EXPECT(loc.kind == location_kind::row_scope, "All matches should be rows");

        auto q1 = query(ctx.document, "top").table(0).row("foo");
        EXPECT(!q1.empty(), "query for 'foo' rows should resolve");
        EXPECT(q1.locations().size() == 2, "there should be 2 'foo'");

        auto q2 = query(ctx.document, "top").table(0).row("bar");
        EXPECT(!q2.empty(), "query for 'bar' rows should resolve");
        EXPECT(q2.locations().size() == 1, "there should be 1 'bar'");

        auto q3 = query(ctx.document, "top").table(0).row("qux");
        EXPECT(!q3.empty(), "query for 'baz' rows should resolve");
        EXPECT(q3.locations().size() == 2, "there should be 2 'qux'");

        return true;
    }   

    doc_context script_two_tables()
    {
        return load(R"(
            world:
                # race   poise
                  elves  friendly

                # race  poise
                  orcs  hostile
        )");
    }

    bool select_table_by_dotpath()
    {
        auto ctx = script_two_tables();
        auto res =
            query(ctx.document, "world.#1")
                .where(eq("race", "orcs"))
                .project("poise");

        EXPECT(!res.empty(), "Query should resolve");
        auto v = res.as_string();
        EXPECT(v.has_value(), "result should be string type");
        EXPECT(v.value() == "hostile", "result is incorrect string");

        return true;
    }

    bool select_table_by_flowing_syntax()
    {
        auto ctx = script_two_tables();
        auto res =
            query(ctx.document, "world")
                .table(1)
                .where(eq("race", "orcs"))
                .project("poise");

        EXPECT(!res.empty(), "Query should resolve");
        auto v = res.as_string();
        EXPECT(v.has_value(), "result should be string type");
        EXPECT(v.value() == "hostile", "result is incorrect string");

        return true;
    }

    bool row_filter_is_composable()
    {
        auto ctx = load(R"(
            top:
                # name  value
                  foo   1
                  foo   2
                  bar   3
        )");

        auto q = query(ctx.document, "top")
                    .table(0)
                    .rows()
                    .row("foo");

        EXPECT(q.locations().size() == 2, "rows().row(name) must work");
        return true;
    }

    bool numeric_promotion()
    {
        auto ctx = load(R"(
            npc:
                # name   hp:int
                  npc1   12
                  npc2   11
                  npc3   10
                  npc4   9
                  npc5   8
        )");

        bool found_hp = false;
        for (auto c : ctx.document.columns())
            if (c.name() == "hp")
            {
                found_hp = true;
                EXPECT(c.type() == value_type::integer, "tested column should be of integer type");
            }

        EXPECT(found_hp, "there should be a column named 'hp'");

        auto q = query(ctx.document, "npc").table(0).rows();
        EXPECT(!q.empty(), "Query should resolve");

        auto q1 = query(ctx.document, "npc").table(0).where(lt("hp", 10));
        EXPECT(q1.locations().size() == 2,"integer < integer failed");

        auto q2 = query(ctx.document, "npc").table(0).where(lt("hp", 10.5));
        EXPECT(q2.locations().size() == 3, "integer < decimal failed");

        return true;
    }    

    bool projected_subset()
    {
        auto ctx = load(R"(
            npc:
                # name   hp:int   race
                  npc1   12       dwarf
                  npc2   11       elf
                  npc3   10       gnome
                  npc4   9        orc
                  npc5   8        human
        )");

        auto q = query(ctx.document, "npc").where(ge("hp", 10)).project("name", "race");

        EXPECT(!q.locations().empty(), "Query should have resolved");
        EXPECT(q.locations().size() == 6, "There should be six matches");

        std::string_view strs[] = {"npc1", "dwarf", 
                                   "npc2", "elf", 
                                   "npc3", "gnome"};
        for (int i = 0; i < 6; ++i)
            EXPECT(*q.locations()[i].value_ptr->source_literal == strs[i], "String mismatch");

        return true;
    }

    doc_context script_country_table()
    {
        return load(R"(
            world:
                # country   capital
                  Japan     Tokyo
                  Sweden    Stockholm
                  USA       Washington

                # foo       bar
                  one       null
                  two       null
        )");
    }

    bool flowing_explicit_table()
    {
        auto ctx = script_country_table();
        auto q = query(ctx.document, "world").table(0).row("Sweden").column("capital");
        EXPECT(!q.empty(), "Query should resolve");
        EXPECT(q.locations().size() == 1, "Should have one match");
        auto res = q.as_string();
        EXPECT(res.has_value(), "The result should be string type");
        EXPECT(res.value() == "Stockholm", "Capital should be Stockholm");
    
        return true;
    }

    bool flowing_explicit_table_commutative()
    {
        auto ctx = script_country_table();
        auto q = query(ctx.document, "world").table(0).column("capital").row("Sweden");
        EXPECT(!q.empty(), "Query should resolve");
        EXPECT(q.locations().size() == 1, "Should have one match");
        auto res = q.as_string();
        EXPECT(res.has_value(), "The result should be string type");
        EXPECT(res.value() == "Stockholm", "Capital should be Stockholm");
    
        return true;
    }

    bool flowing_implicit_table()
    {
        auto ctx = script_country_table();
        auto q = query(ctx.document, "world").row("Sweden").column("capital");
        EXPECT(!q.empty(), "Query should resolve");
        EXPECT(q.locations().size() == 1, "Should have one match");
        auto res = q.as_string();
        EXPECT(res.has_value(), "The result should be string type");
        EXPECT(res.value() == "Stockholm", "Capital should be Stockholm");
    
        return true;
    }

    bool flowing_implicit_table_commutative()
    {
        auto ctx = script_country_table();
        auto q = query(ctx.document, "world").column("capital").row("Sweden");
        EXPECT(!q.empty(), "Query should resolve");
        EXPECT(q.locations().size() == 1, "Should have one match");
        auto res = q.as_string();
        EXPECT(res.has_value(), "The result should be string type");
        EXPECT(res.value() == "Stockholm", "Capital should be Stockholm");
    
        return true;
    }

    bool dotpath_explicit_table()
    {
        auto ctx = script_country_table();
        auto q = query(ctx.document, "world.#0.-Sweden-.|capital|");
        EXPECT(q.locations().size() == 1, "Should have one match");
        auto res = q.as_string();
        EXPECT(res.has_value(), "The result should be string type");
        EXPECT(res.value() == "Stockholm", "Capital should be Stockholm");

        return true;
    }

    bool dotpath_implicit_table_introduction()
    {
        auto ctx = script_country_table();
        auto q = query(ctx.document, "world.-Japan-.|capital|");
        EXPECT(!q.empty(), "Query should resolve");
        EXPECT(q.locations().size() == 1, "Should have one match");
        auto res = q.as_string();
        EXPECT(res.has_value(), "The result should be string type");
        EXPECT(res.value() == "Tokyo", "Capital should be Tokyo");

        return true;
    }


    bool dotpath_explicit_table_commutative_column_first()
    {
        auto ctx = script_country_table();
        auto q = query(ctx.document, "world.#0.|capital|.-USA-");
        EXPECT(q.locations().size() == 1, "Should have one match");
        auto res = q.as_string();        
        EXPECT(res.has_value(), "The result should be string type");
        EXPECT(res.value() == "Washington", "Capital should be Washington");

        return true;
    }
    
    bool dotpath_implicit_table_commutative_column_first()
    {
        auto ctx = script_country_table();
        auto q = query(ctx.document, "world.|capital|.-USA-");
        EXPECT(q.locations().size() == 1, "Should have one match");
        auto res = q.as_string();
        EXPECT(res.has_value(), "The result should be string type");
        EXPECT(res.value() == "Washington", "Capital should be Washington");

        return true;
    }

    void run_query_tests()
    {
        SUBCAT("Foundations");
        RUN_TEST(dot_path_correctly_split);
        RUN_TEST(find_single_value);
        RUN_TEST(extract_single_value);
        RUN_TEST(extract_single_value_through_helpers);
        RUN_TEST(index_follows_array);
        SUBCAT("Extract plural data");
        RUN_TEST(query_plural_results);
        RUN_TEST(query_reports_ambiguity);
        RUN_TEST(ambiguious_query_will_not_extract_to_single);
        SUBCAT("Access tables");
        RUN_TEST(find_single_table);
        RUN_TEST(find_multiple_tables);
        RUN_TEST(select_table_by_dotpath);
        RUN_TEST(select_table_by_flowing_syntax);
        SUBCAT("Flowing API table rows");
        RUN_TEST(flowing_explicit_table);
        RUN_TEST(flowing_explicit_table_commutative);
        RUN_TEST(flowing_implicit_table);
        RUN_TEST(flowing_implicit_table_commutative);
        SUBCAT("Dotpath API table rows");
        RUN_TEST(dotpath_explicit_table);
        RUN_TEST(dotpath_implicit_table_introduction);
        RUN_TEST(dotpath_explicit_table_commutative_column_first);
        RUN_TEST(dotpath_implicit_table_commutative_column_first);
        SUBCAT("Access table rows");
        RUN_TEST(enumerate_all_rows_in_table);
        RUN_TEST(enumerate_named_rows_in_table);
        RUN_TEST(row_filter_is_composable);
        SUBCAT("Row access by predicate");
        RUN_TEST(numeric_promotion);
        SUBCAT("Row access by identifier");
        RUN_TEST(query_table_row_by_string_id);
        RUN_TEST(projected_subset);
    }
}


#endif

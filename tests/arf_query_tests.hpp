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
        auto const p1 = split_dot_path("foo.bar.baz");
        EXPECT(p1.size() == 3, "Should find three items");
        EXPECT(p1[0] == "foo", "1st item should be foo");
        EXPECT(p1[1] == "bar", "1st item should be bar");
        EXPECT(p1[2] == "baz", "1st item should be baz");

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

        // Testing query for the same data
        {
        // FINISH ME!
            auto q = query( ctx.document, "top.arr.1" );
            auto v = q.as_integer();
        }
        
        return false;
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
                //.where("race", "orcs")
                .select("poise");

        EXPECT(ctx.locations().size() == 1, "ambigious");
        EXPECT(ctx.as_string().has_value(), "no value");
        EXPECT(ctx.as_string().value() == "hostile", "wrong result");

        return true;
    }

    // -----------------------------------------------------------------
    // Plurality preservation
    // -----------------------------------------------------------------

    bool query_plural_results()
    {
        auto ctx = load(R"(
            world:
                # race poise
                  elves  friendly
                  orcs   hostile
                  orcs   drunk
        )");

        auto res =
            query(ctx.document, "world")
                //.where("race", "orcs")
                .select("poise");

        return false;                
        EXPECT(res.locations().size() == 2, "");

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
        //EXPECT(!res.issues().empty(), "Should be a diagnostic report attached");

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
    // Ordinal table access
    // -----------------------------------------------------------------

    bool query_ordinal_table()
    {
        auto ctx = load(R"(
            world:
                # race   poise
                  elves  friendly

                # race  poise
                  orcs  hostile
        )");

        auto res =
            query(ctx.document, "world")
                //.table(1)
                //.where("race", "orcs")
                .select("poise");

        return false;
        //EXPECT(res.as_string().value.value() == "hostile", "");

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
        // SUBCAT("Access tables");
        // //RUN_TEST(query_ordinal_table);
        // SUBCAT("Access by identifier");
        // //RUN_TEST(query_table_row_by_string_id);
    }
}


#endif

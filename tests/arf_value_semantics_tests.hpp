#ifndef ARF_TESTS_VALUE_SEMANTICS__
#define ARF_TESTS_VALUE_SEMANTICS__

#include "arf_test_harness.hpp"
#include "../include/arf_document.hpp"

namespace arf::tests
{

static bool test_key_type_inference()
{
    constexpr std::string_view src =
        "a = 42\n"
        "b = 3.14\n"
        "c = hello\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    auto root = doc->root();
    EXPECT(root.has_value());

    auto key_ids = root->keys();
    EXPECT(key_ids.size() == 3);

    auto k0 = doc->key(key_ids[0]);
    auto k1 = doc->key(key_ids[1]);
    auto k2 = doc->key(key_ids[2]);

    EXPECT(k0.has_value());
    EXPECT(k1.has_value());
    EXPECT(k2.has_value());

    EXPECT(k0->value().type == value_type::integer);
    EXPECT(k1->value().type == value_type::decimal);
    EXPECT(k2->value().type == value_type::string);

    return true;
}

static bool test_declared_key_type()
{
    constexpr std::string_view src =
        "x:float = 42\n";

    auto doc = load(src);
    EXPECT(doc.has_value());

    auto root = doc->root();
    EXPECT(root.has_value());

    auto key_ids = root->keys();
    EXPECT(key_ids.size() == 1);

    auto key = doc->key(key_ids.front());
    EXPECT(key.has_value());

    EXPECT(key->value().type == value_type::decimal);
    EXPECT(key->value().type_source == type_ascription::declared);

    return true;
}

static bool test_table_column_inference()
{
    constexpr std::string_view src =
        "# a  b\n"
        "  1  hello\n";

    auto doc = load(src);
    EXPECT(doc.has_value());
    EXPECT(doc->table_count() == 1);

    auto tbl = doc->table(table_id{0});
    EXPECT(tbl.has_value());

    auto cols = tbl->columns();
    EXPECT(cols[0].type == value_type::integer);
    EXPECT(cols[1].type == value_type::string);
    return true;
}

static bool test_declared_table_column()
{
    constexpr std::string_view src =
        "# a:int  b\n"
        "  3.14   hello\n";

    auto doc = load(src);
    auto tbl = doc->table(table_id{0});

    auto cols = tbl->columns();
    EXPECT(cols[0].type == value_type::integer);
    EXPECT(cols[0].type_source == type_ascription::declared);
    return true;
}

static bool test_malformed_array_survives()
{
    constexpr std::string_view src =
        "a = 1||2|||3\n";

    auto ctx = load(src);
    EXPECT(!ctx.has_errors());

    auto root = ctx.document.root();
    auto key  = root->keys()[0];

    auto arr = key.value().as_array<int>();
    EXPECT(arr.size() == 4);
    EXPECT(!arr[2].has_value()); // empty element
    return true;
}

//----------------------------------------------------------------------------

inline void run_value_semantics_tests()
{
    RUN_TEST(test_key_type_inference);
    RUN_TEST(test_declared_key_type);
    RUN_TEST(test_table_column_inference);
    RUN_TEST(test_declared_table_column);
    RUN_TEST(test_malformed_array_survives);
}

}

#endif
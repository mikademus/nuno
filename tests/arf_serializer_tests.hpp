#ifndef ARF_TESTS_SERIALIZER__
#define ARF_TESTS_SERIALIZER__

#include "arf_test_harness.hpp"
#include "../include/arf_serializer.hpp"
#include "../include/arf.hpp"

namespace arf::tests
{
using namespace arf;

static bool foo()
{
    constexpr std::string_view src =
        "a = 42\n"
        "# x  y\n"
        "  1  2\n"
        "  3  4\n";

    auto ctx = load(src, {});
    EXPECT(ctx.errors.empty(), "error emitted");

    std::cout << ctx.document.key_count() << std::endl;
    std::cout << ctx.document.table_count() << std::endl;

    std::ostringstream out;
    auto s = serializer(out);
    auto & r = s.root(ctx.document);

    std::cout << r.ordered_items.size() << std::endl;

    s.write(ctx.document);
    // out.flush();

    std::cout << out.str() << std::endl;

    return true;
}    

inline void run_seriealizer_tests()
{
    RUN_TEST(foo);
}

} // ns arf::tests

#endif
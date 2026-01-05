#ifndef ARF_TESTS_HARNESS__
#define ARF_TESTS_HARNESS__

#include <iostream>
#include <vector>
#include <string>

namespace arf::tests
{

    struct test_result
    {
        std::string name;
        bool passed;
    };

    extern std::vector<test_result> results;
    extern char const * last_error;

    #define EXPECT(cond, false_msg)                        \
        do                                                 \
        {                                                  \
            last_error = "";                               \
            if (!(cond)) {                                 \
                last_error = false_msg;                    \
                return false;                              \
            }                                              \
        } while (0)

    #define RUN_TEST(fn)                                   \
        do                                                 \
        {                                                  \
            bool ok = fn();                                \
            results.push_back({ #fn, ok });                \
            std::cout << (ok ? "[ ok ] " : "[FAIL] ")      \
                    << #fn;                                \
            if (!ok) std::cout << " FAILED: " << last_error;       \
            std::cout << "\n";                             \
        } while (0)

    #define SUBCAT(msg)                                                     \
        do                                                                  \
        {                                                                   \
            std::string str(msg);                                           \
            std::transform(str.begin(), str.end(), str.begin(), ::toupper); \
            std::cout << "-------" << str << "--------------\n";            \
        }                                                                   \
        while (0)
}

#endif
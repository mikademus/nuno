#include "arf_test_harness.hpp"
#include "arf_parser_tests.hpp"
#include "arf_materialiser_tests.hpp"
#include "arf_document_structure_tests.hpp"
#include "arf_reflection_tests.hpp"
//#include "arf_value_semantics_tests.hpp"
#include <cstring>

namespace arf::tests 
{
    std::vector<test_result> results;    
    char const * last_error = "";
}

bool first = true;

void run_tests( std::string suite_name, void(*pf_tests)() )
{
    if (!first)
        std::cout << '\n';
    else first = false;

    std::cout << suite_name << '\n';
    std::cout << std::string(suite_name.length(), '=') << '\n';
    pf_tests();
}

int main()
{
    using namespace arf::tests;

    run_tests("Parser pass", run_parser_tests);
    run_tests("Materialiser pass", run_materialiser_tests);
    run_tests("Document structure", run_document_structure_tests);
    run_tests("Reflection", run_reflection_tests);
    //run_value_semantics_tests();

    // unified reporting    
}
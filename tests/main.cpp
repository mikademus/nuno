#include "arf_test_harness.hpp"
#include "arf_parser_tests.hpp"
#include "arf_materialiser_tests.hpp"
#include "arf_document_structure_tests.hpp"
#include "arf_reflection_tests.hpp"
#include "arf_query_tests.hpp"
#include "arf_editor_tests.hpp"
#include "arf_serializer_tests.hpp"
#include "arf_integration_tests.hpp"

#include <cstring>
#include <iostream>

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

    #ifdef ARF_TESTS_PARSER__ 
        run_tests("Parser pass", run_parser_tests); 
    #endif

    #ifdef ARF_TESTS_MATERIALISER__ 
        run_tests("Materialiser pass", run_materialiser_tests);
    #endif

    #ifdef ARF_TESTS_DOCUMENT_STRUCTURE__ 
        run_tests("Document structure", run_document_structure_tests);
    #endif

    #ifdef ARF_TESTS_REFLECTION__ 
        run_tests("Reflection", run_reflection_tests);
    #endif

    #ifdef ARF_TESTS_QUERIES__ 
        run_tests("Queries", run_query_tests);
    #endif

    #ifdef ARF_TESTS_EDITOR__ 
        run_tests("Editor", run_editor_tests);
    #endif

    #ifdef ARF_TESTS_SERIALIZER__ 
        run_tests("Serialization", run_seriealizer_tests);
    #endif

    #ifdef ARF_TESTS_COMPREHENSIVE__ 
        run_tests("Integration", run_integration_tests);
    #endif
}
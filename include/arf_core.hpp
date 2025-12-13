// arf_core.hpp - A Readable Format (Arf!) - Core Data Structures
// Version 0.2.0

#ifndef ARF_CORE_HPP
#define ARF_CORE_HPP

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <memory>
#include <sstream>
#include <iomanip>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <cstring>

namespace arf 
{
    //========================================================================
    // CORE DATA STRUCTURES
    //========================================================================
    
    enum class value_type 
    {
        string,
        integer,
        decimal,
        boolean,
        date,
        string_array,
        int_array,
        float_array
    };
    
    using value = std::variant
                  <
                      std::string,
                      int64_t,
                      double,
                      bool,
                      std::vector<std::string>,
                      std::vector<int64_t>,
                      std::vector<double>
                  >;
    
    struct column 
    {
        std::string name;
        value_type type;
        
        bool operator==(const column& other) const
        {
            return name == other.name && type == other.type;
        }
    };
    
    using table_row = std::vector<value>;
    
    struct category 
    {
        std::string name;
        std::map<std::string, value> key_values;
        std::vector<column> table_columns;
        std::vector<table_row> table_rows;
        std::map<std::string, std::unique_ptr<category>> subcategories;
    };
    
    struct document 
    {
        std::map<std::string, std::unique_ptr<category>> categories;
    };
    
    struct parse_error 
    {
        size_t line_number;
        std::string message;
        std::string line_content;
        
        std::string to_string() const 
        {
            std::ostringstream oss;
            oss << "Line " << line_number << ": " << message;
            if (!line_content.empty())
                oss << "\n  > " << line_content;
            return oss.str();
        }
    };
    
    struct parse_context 
    {
        std::optional<document> doc;
        std::vector<parse_error> errors;
        
        bool has_value() const { return doc.has_value(); }
        bool has_errors() const { return !errors.empty(); }
    };
    
    //========================================================================
    // UTILITY FUNCTIONS
    //========================================================================
    
    namespace detail 
    {
        constexpr size_t MAX_LINES = 1'000'000;
        constexpr std::string_view ROOT_CATEGORY_NAME = "__root__";
        
        inline std::string_view trim_sv(std::string_view s) 
        {
            size_t start = s.find_first_not_of(" \t\r\n");
            if (start == std::string_view::npos) return {};
            size_t end = s.find_last_not_of(" \t\r\n");
            return s.substr(start, end - start + 1);
        }
        
        inline std::string to_lower(const std::string& s)
        {
            std::string result = s;
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            return result;
        }
        
        inline std::string type_to_string(value_type type)
        {
            switch (type)
            {
                case value_type::string: return "str";
                case value_type::integer: return "int";
                case value_type::decimal: return "float";
                case value_type::boolean: return "bool";
                case value_type::date: return "date";
                case value_type::string_array: return "str[]";
                case value_type::int_array: return "int[]";
                case value_type::float_array: return "float[]";
                default: return "str";
            }
        }
    }
    
} // namespace arf

#endif // ARF_CORE_HPP
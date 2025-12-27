// arf_core.hpp - A Readable Format (Arf!) - Core Data Structures
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_CORE_HPP
#define ARF_CORE_HPP

#include <string>
#include <string_view>
#include <vector>
//#include <map>
#include <variant>
#include <optional>
//#include <memory>
//#include <sstream>
#include <algorithm>
#include <concepts>

namespace arf 
{
//========================================================================
// Forward declarations and selected aliases
//========================================================================

    struct document;
    struct category;

//========================================================================
// IDs
//========================================================================

    inline constexpr size_t npos() { return static_cast<size_t>(-1); }

    template <typename Tag>
    struct id
    {
        size_t val;

        explicit id(size_t v = npos()) : val(v) {}
        operator size_t() const { return val; }
        id & operator= (size_t v) { val = v; return *this; }        
        auto operator<=>(id const &) const = default;
        id & operator++() { ++val; return *this; }
        id operator++(int) { id temp = *this; ++val; return temp; }
    };
    
    template <typename Tag>
    constexpr id<Tag> invalid_id()
    {
        return id<Tag>{ npos() };
    }

    struct category_tag;
    struct table_tag;
    struct table_row_tag;

    using category_id   = id<category_tag>;
    using table_id      = id<table_tag>;
    using table_row_id  = id<table_row_tag>;

//========================================================================
// Global aliases
//========================================================================

    using parse_event_target = std::variant<std::monostate, category_id, table_id, table_row_id>;


//========================================================================
// Values
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

    enum class type_ascription
    {
        tacit,    // implicit, not defined in source
        declared  // explicitly defined in source
    };

    enum class value_locus
    {
        key_value,  // declared via key = value
        table_cell  // declared inside a table row
    };

    using value = std::variant<
        std::string,
        int64_t,
        double,
        bool,
        std::vector<std::string>,
        std::vector<int64_t>,
        std::vector<double>
    >;

    struct typed_value
    {
        value           val;
        value_type      type;
        type_ascription type_source;
        value_locus     origin;
        std::optional<std::string> source_literal;
    };

//========================================================================
// Structure and parsing
//========================================================================
    
    struct source_location
    {
        size_t line {0};    // 1-based
    };

    enum class parse_event_kind
    {
        empty_line,
        comment,
        invalid,

        key_value,
        table_header,
        table_row,

        category_open,
        category_close
    };

    struct parse_event
    {
        parse_event_kind   kind;
        source_location    loc;
        std::string        text;        
        parse_event_target target; // Optional semantic attachment
    };    


//========================================================================
// Remaining data structures
//========================================================================

    struct category
    {
        category_id id;
        std::string name;
        category_id parent;    // npos for root
    };

    struct column
    {
        std::string     name;
        value_type      type;
        type_ascription type_source;
    };

    struct table_row
    {
        table_row_id id;
        category_id  owning_category;
        std::vector<typed_value> cells;
    };

    struct table
    {
        table_id              id;
        category_id           owning_category;
        std::vector<column>   columns;
        std::vector<table_row_id> rows;   // in authored order
    };

    struct document
    {
        // Primary spine
        std::vector<parse_event> events;

        // Entities
        std::vector<category>    categories;
        std::vector<table>       tables;
        std::vector<table_row>   rows;
    };

    struct parse_error
    {
        source_location loc;
        std::string     message;
        std::string     text;
    };
        
    struct parse_context
    {
        document                 doc;
        std::vector<parse_error> errors;

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
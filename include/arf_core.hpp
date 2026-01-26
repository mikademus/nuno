// arf_core.hpp - A Readable Format (Arf!) - Core Data Structures
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_CORE_HPP
#define ARF_CORE_HPP

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace arf 
{
//========================================================================
// Forward declarations and selected aliases
//========================================================================

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
    struct table_column_tag;
    struct key_tag;

    using category_id   = id<category_tag>;
    using table_id      = id<table_tag>;
    using table_row_id  = id<table_row_tag>;
    using column_id     = id<table_column_tag>;
    using key_id        = id<key_tag>;

//========================================================================
// Values
//
// Note: Avoid manually creating typed_value instances from values, for 
//       instance for comparisons. typed_value is intended to be generated 
//       during materialisation when creating the document and will be 
//       malformed unless all members are set correctly.
//========================================================================
    
    enum struct semantic_state : uint8_t
    {
        valid,
        invalid
    };

    enum class contamination_state : uint8_t
    {
        clean,
        contaminated
    };

    enum class value_type
    {
        unresolved,
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

    struct typed_value;

    using value = std::variant<
        std::monostate,
        std::string,
        int64_t,
        double,
        bool,
        std::vector<typed_value>
    >;

    struct typed_value
    {
        value               val;
        value_type          type;
        type_ascription     type_source;
        value_locus         origin;
        std::optional<std::string> source_literal;
        semantic_state      semantic      = semantic_state::valid;
        contamination_state contamination = contamination_state::clean;
    };

    inline bool is_valid(const typed_value &value) { return value.semantic == semantic_state::valid; }
    inline bool is_clean(const typed_value &value) { return value.contamination == contamination_state::clean; }
    inline bool is_numeric(value_type type) { return type == value_type::integer || type == value_type::decimal; }
    inline bool is_numeric(const typed_value &value) { return is_numeric(value.type); }
    inline bool is_array(value_type type) { return type == value_type::string_array || type == value_type::int_array || type == value_type::float_array; }
    inline bool is_array(const typed_value &value) { return is_array(value.type); }
    inline bool is_string(value_type type) { return type == value_type::string; }
    inline bool is_string(const typed_value &value) { return is_string(value.type); }
    inline bool is_boolean(value_type type) { return type == value_type::boolean; }
    inline bool is_boolean(const typed_value &value) { return is_boolean(value.type); }

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
        column_id       id;
        std::string     name;
        value_type      type;
        type_ascription type_source;
        std::optional<std::string> declared_type;
        semantic_state  semantic = semantic_state::valid;
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

//========================================================================
// Document generation context
//========================================================================

    struct source_location
    {
        size_t line {0};    // 1-based
        size_t column {0};  // 1-based
    };

    template <typename ERROR_KIND>
    struct error
    {
        ERROR_KIND         kind;
        source_location    loc;
        std::string        message;
    };

    template <typename T, typename ERROR_KIND>
    struct context
    {
        T document;
        std::vector<error<ERROR_KIND>> errors;

        T * operator->() { return &document; } 

        bool has_errors() const { return !errors.empty(); }
    };    
    
//========================================================================
// UTILITY FUNCTIONS
//========================================================================
    
    namespace detail 
    {
        constexpr size_t MAX_LINES = 1'000'000;
        constexpr std::string_view ROOT_CATEGORY_NAME = "__root__";
        
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

        inline std::optional<value_type>
        parse_declared_type(std::string_view sv)
        {
            auto s = to_lower(std::string(trim_sv(sv)));

            if (s == "int")     return value_type::integer;
            if (s == "float")   return value_type::decimal;
            if (s == "bool")    return value_type::boolean;
            if (s == "date")    return value_type::date;
            if (s == "str")     return value_type::string;
            if (s == "str[]")   return value_type::string_array;
            if (s == "int[]")   return value_type::int_array;
            if (s == "float[]") return value_type::float_array;

            return std::nullopt;
        }        

    // -----------------------------------------------------------------------
    // typed_value creation helpers
    // Note: no specialisations for arrays
    // -----------------------------------------------------------------------

        template<typename T>
        concept strictly_integral = std::integral<T> && !std::same_as<T, bool>;

        template<typename T, typename = void>
        struct vt_conv;

        template<strictly_integral T>
        struct vt_conv<T>
        {
            enum vtype { vtype = static_cast<int>(value_type::decimal) };
            typedef double stype;
        };

        template<std::floating_point T>
        struct vt_conv<T>
        {
            enum vtype { vtype = static_cast<int>(value_type::decimal) };
            typedef double stype;
        };

        template<std::same_as<bool> T>
        struct vt_conv<T>
        {
            enum vtype { vtype = static_cast<int>(value_type::boolean) };
            typedef bool stype;
        };

        template<typename T>
            requires std::convertible_to<T, std::string_view>
        struct vt_conv<T>
        {
            enum vtype { vtype = static_cast<int>(value_type::string) };
            typedef std::string stype;
        };

        template<typename T>
        inline typed_value make_typed_value( T value )
        {
            return typed_value{
                .val  = typename vt_conv<T>::stype(value),
                .type = static_cast<value_type>(vt_conv<T>::vtype),
                .type_source = type_ascription::tacit,
                .semantic = semantic_state::valid
            };
        }
    }
    
} // namespace arf

#endif // ARF_CORE_HPP
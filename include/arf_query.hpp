// arf_query.hpp - A Readable Format (Arf!) - Query Interface
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.
// 
// The query API provides a fluent interface for selecting and extracting
// values from Arf! documents. Queries operate on "value locations" rather
// than navigating object graphs, supporting:
//
// - Dot-path selection: query(doc, "world.config.seed")
// - Fluent refinement: query(doc, "items").table(0).rows()
// - Predicate filtering: .where(predicate)
// - Singular/plural extraction: .as_integer() or .integers()
//
// See query_interface.md for detailed usage patterns.

#ifndef ARF_QUERY_HPP
#define ARF_QUERY_HPP

#include "arf_document.hpp"
#include "arf_reflect.hpp"

#include <charconv>
#include <concepts>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace arf
{

    struct query_handle;

    template<typename T>
    struct query_result;

//======================================================================
// Public API:
// =====================================================================
// Entry points
// ------------
//
// Basic usage:
//   auto seed = get_integer(doc, "world.seed");
//   if (seed) { /* use *seed */ }
//
// Fluent queries:
//   auto items = query(doc, "inventory")
//                   .table(0)
//                   .rows()
//                   .where(predicate{"rarity", eq("legendary")})
//                   .strings();  // Get all matching names
//
// Path-like queries (see dotpath_query_syntax.md):
//   auto items = query(doc, "inventory.#0.|rarity|.-legendary-")
//                   .strings(); // Get all matching names
//
//======================================================================

    inline query_handle query(const document& doc) noexcept;
    inline query_handle query(const document& doc, std::string_view path) noexcept;

// Convenience getters (exact type match required)
// -------------------------------------------------
    inline query_result<int64_t>        get_integer(const document& doc, std::string_view path) noexcept;
    inline query_result<double>         get_real(const document& doc, std::string_view path) noexcept;
    inline query_result<std::string>    get_string(const document& doc, std::string_view path) noexcept;
    inline query_result<bool>           get_bool(const document& doc, std::string_view path) noexcept;

// Converting convenience getters (attempts string-to-type 
// conversion if requested type differes from stored type)
// ---------------------------------------------------------------------
    inline query_result<int64_t>        get_as_integer(const document& doc, std::string_view path) noexcept;
    inline query_result<double>         get_as_real(const document& doc, std::string_view path) noexcept;
    inline query_result<std::string>    get_as_string(const document& doc, std::string_view path) noexcept;

// =====================================================================
// Query issues
// =====================================================================

    enum class query_issue_kind
    {
        none,
        syntax_error,       // illegal syntax used in dotpath
        not_found,          // path does not resolve (structural navigation failed)
        invalid_index,      // OOB index / the index doesn't exist 
        empty_result,       // filtering/enumeration produced empty set
        ambiguous,          // multiple matches when singular expected
        type_mismatch,      // value is of another type than requested
        conversion_failed,  // attempt to convert value between types failed
        missing_literal,    // the source_literal member of the value is empty
        not_a_value,        // the queried address is not a value terminal
    };

    enum class diagnostic_kind
    {
        spelling_error,     // emitted when encountering possible mistakes in dotpath selectors (#, [], --, ||)
        row_not_found,      // named row not found in table(s)
        column_not_found,   // named column not found in table(s)
        invalid_index,      // non-numeric or otherwise malformed index following array, table, row or column selector
        index_out_of_bounds,// negative or too large ordinal index following array, table, row or column selector
        empty_scope,        // narrowing or enumeration requested on empty set
        ambiguous_selection,
    };

    struct query_issue
    {
        query_issue_kind kind { query_issue_kind::none };
        std::string     context;
        size_t          step_index { 0 };        
    };

    struct diagnostic
    {
        diagnostic_kind kind;
        std::string     symbol;       // "orcs", "hp", etc
        size_t          step_index;   // dotpath segment index
    };    

// =====================================================================
// Query location model
// =====================================================================

    enum class location_kind
    {
        category_scope,
        table_scope,
        row_scope,
        terminal_value
    };

    struct value_location
    {
        reflect::address           addr;
        location_kind              kind;
        const typed_value*         value_ptr { nullptr };
    };

// =====================================================================
// to_string
// =====================================================================

    inline std::string_view to_string(query_issue_kind kind)
    {
        switch (kind)
        {
            using enum query_issue_kind;
            case none: return "none";
            case syntax_error: return "syntax error";
            case empty_result: return "empty result";
            case not_found: return "not found";
            case invalid_index: return "invalid index";
            case ambiguous: return "ambiguous";
            case type_mismatch: return "type mismatch";
            case conversion_failed: return "conversion failed";
            case missing_literal: return "missing literal";
            case not_a_value: return "not a value";
        }
        return "unknown";
    }

    inline std::string_view to_string(diagnostic_kind kind)
    {
        switch (kind)
        {
            using enum diagnostic_kind;
            case row_not_found: return "row not found";
            case column_not_found: return "column not found";
            case diagnostic_kind::invalid_index: return "invalid index";
            case index_out_of_bounds: return "index out of bounds";
            case empty_scope: return "empty scope";
            case ambiguous_selection: return "ambiguous selection";
            case spelling_error: return "spelling error?";
        }
        return "unknown";
    }

    inline std::string_view to_string( location_kind loc )
    {
        switch (loc)
        {
            using enum location_kind;
            case category_scope: return "category scope";
            case table_scope: return "table scope";
            case row_scope: return "row scope";
            case terminal_value: return "terminal value";
        }
        return "unknown";
    }
    
//======================================================================
// Query predicates [ for .where(pred) ]
// =====================================================================
//
// Predicates for row filtering
//
// Predicates filter rows in table contexts based on column values.
//
// Construction:
//   Use the factory functions:
//      eq("column_name", value)
//      eq(column_index, value)
//
// Supported operators:
//   eq, ne, lt, le, gt, ge
//
// Value must match column type (no implicit conversion).
//
// Example:
//   query(doc, "items")
//       .table(0)
//       .where(eq("rarity", "legendary"))
//
//----------------------------------------------------------------------
    enum class predicate_op
    {
        eq, ne, lt, le, gt, ge
    };

    struct column_ref
    {
        std::variant<size_t, std::string> ref;
    };

    struct predicate
    {
        column_ref   column;
        predicate_op op;
        typed_value  rhs;
    };

    template<typename T>
    concept column_ref_type = std::is_integral_v<T> || std::convertible_to<T, std::string_view>;

    template<column_ref_type Col, typename T>
    predicate eq(Col col, T val) { return {col, predicate_op::eq, detail::make_typed_value(val)}; }
    template<column_ref_type Col, typename T>
    predicate ne(Col col, T val) { return {col, predicate_op::ne, detail::make_typed_value(val)}; }
    template<column_ref_type Col, typename T>
    predicate lt(Col col, T val) { return {col, predicate_op::lt, detail::make_typed_value(val)}; }
    template<column_ref_type Col, typename T>
    predicate le(Col col, T val) { return {col, predicate_op::le, detail::make_typed_value(val)}; }
    template<column_ref_type Col, typename T>
    predicate gt(Col col, T val) { return {col, predicate_op::gt, detail::make_typed_value(val)}; }
    template<column_ref_type Col, typename T>
    predicate ge(Col col, T val) { return {col, predicate_op::ge, detail::make_typed_value(val)}; }

//======================================================================
// Query handle
// =====================================================================
// 
// Represents a progressive query over a document. Each method narrows
// or transforms the working set of value locations and returns *this
// to allow chaining.
//
// NOTE that each selection method MUTATES the working set in place and
// returns *this for chaining.
//
//     IMPORTANT: Query handles are MUTABLE and STATEFUL.
//     Calling a method modifies the handle. To branch queries, copy
//     the handle BEFORE calling methods:
//
//     auto base = query(doc, "world").table(0);
//     
//     auto copy1 = base;  // Copy before branching
//     auto foos = copy1.rows("foo");
//     
//     auto copy2 = base;  // Copy again for second branch
//     auto bars = copy2.rows("bar");
//
// Handles are lightweight (pointer + vectors) and cheap to copy.
// They reference the document but do not own it - the document must
// outlive all query handles.
//
//
// Selection:
// ------------------------------------------------------------
// - use the select() method to process dotpath segments, f.i. "top.sub.my_key".
// - use the child() method to select a filter that can't be expressed in the
//   dotpath select() syntax (f.i. items containing dots).
//
//
// Row filtering (row narrowing by predicated filtering):
// ------------------------------------------------------------
// where(predicate)
//
// where() scope expansion rules
//  If the current working set contains:
//    - row_scope → filter rows
//    - table_scope → equivalent to rows().where(...)
//    - category_scope → expand to all tables under the category, then rows(), then apply where
//  If no rows exist after expansion, the result is empty (not an error).
//
//
// Projection from tables (shape transformation, not filtering)
// ------------------------------------------------------------
// project(n1, n2, n3 ...)
//
// project() declares which columns should be extracted from
// resolved row sets. It does NOT affect query resolution,
// ambiguity, or cardinality.
//
// Intended usage:
//   query(...).table(0).rows().where(...).project("hp", "mp");
//
//-----------------------------------------------------------------------

    class query_handle
    {
        template <typename T> friend struct query_result;        

    public:
        struct axis_selection
        {
            std::optional<std::string_view> row;
            std::optional<std::string_view> column;

            void reset() noexcept { row.reset(); column.reset(); }
            bool empty() const noexcept { return !row && !column; }
            bool complete() const noexcept { return row.has_value() && column.has_value(); }
        };

        // --------------------------------------------------------------
        // 1. Construction
        // --------------------------------------------------------------
        explicit query_handle(const document& doc) noexcept : doc_(&doc) {}

        // --------------------------------------------------------------
        // 2. Path-based selection
        // --------------------------------------------------------------
        query_handle& select(std::string_view path);
        query_handle& child(std::string_view name);

        // --------------------------------------------------------------
        // 3. Scope selectors (narrowing)
        // --------------------------------------------------------------
        query_handle& tables();
        query_handle& table(size_t ordinal);

        // --------------------------------------------------------------
        // 4. Table axis selectors
        // --------------------------------------------------------------
        query_handle& rows();
        query_handle& row(size_t ordinal);
        query_handle& row(std::string_view name);

        query_handle& columns();
        query_handle& column(size_t ordinal);
        query_handle& column(std::string_view name);

        // --------------------------------------------------------------
        // 5. Array element selector
        // --------------------------------------------------------------
        query_handle& index(size_t ordinal);

        // --------------------------------------------------------------
        // 6. Filtering & projection
        // --------------------------------------------------------------
        query_handle& where(predicate pred);

        template <std::convertible_to<std::string_view>... Names>
        query_handle& project(Names&&... names)
        {
            std::array<std::string_view, sizeof...(Names)> cols {
                std::string_view(std::forward<Names>(names))...
            };
            return project_impl(cols);
        };        

        // --------------------------------------------------------------
        // 7. Query status
        // --------------------------------------------------------------
        bool empty() const noexcept { return locations_.empty(); }
        bool ambiguous() const noexcept { return locations_.size() > 1; }

        // --------------------------------------------------------------
        // 8. Introspection
        // --------------------------------------------------------------
        const std::vector<value_location>& locations() const noexcept { return locations_; }
        const std::vector<query_issue>& issues() const noexcept { return issues_; }
        const std::vector<diagnostic>& diagnostics() const noexcept { return diagnostics_; }

        // --------------------------------------------------------------
        // 9. Extraction
        // --------------------------------------------------------------
        query_result<int64_t>     as_integer(bool convert = false) const noexcept;
        query_result<double>      as_real(bool convert = false) const noexcept;
        query_result<bool>        as_bool() const noexcept; // conversion disallowed
        query_result<std::string> as_string(bool convert = false) const noexcept;

        // TODO: arrays

    private:
        const document*                   doc_ { nullptr };
        std::vector<value_location>       locations_;
        mutable std::vector<query_issue>  issues_;
        mutable std::vector<diagnostic>   diagnostics_;
        axis_selection                    pending_axis_;

        void flush_pending_axis_();
        bool all_locations_are(location_kind scope) const noexcept;

        // Note: may add ambiguity diagnostic to issues_ (mutable)
        template<value_type vt>
        typed_value const *
        common_extraction_checks(query_issue_kind* err) const noexcept;

        template<typename T>
        bool extract_convert(T & out, query_issue_kind* err) const noexcept;

        template<value_type Vt, typename T>
        query_result<T>
        scalar_extract(bool convert) const noexcept;

        query_handle& project_impl(std::span<const std::string_view> column_names);
        
        void report_issue(query_issue_kind kind, std::string_view context, size_t line = 0) const noexcept;
        void report_if_empty(query_issue_kind kind, std::string_view context, size_t line = 0) const noexcept;
    };

//======================================================================
// query_result<T> - Result container with error information
// =====================================================================
//
// (Minimal replacement for C++23 std::expected).
//
// Combines std::optional<T> interface with error reporting.
// Queries can fail for several reasons:
//   - not_found: Path doesn't resolve
//   - ambiguous: Multiple matches when singular expected
//   - type_mismatch: Value exists but wrong type
//   - conversion_failed: Conversion attempted but failed
//
// Usage:
//   auto res = query(doc, "world.seed").as_integer();
//   if (res) {
//       use(*res);
//   } else {
//       handle(res.error());
//   }
//
//----------------------------------------------------------------------
    template <typename T>//, typename E = query_issue_kind>
    struct query_result
    {
        std::optional<T> storage;
        query_issue_kind error_value {query_issue_kind::none};

        query_result() = default;

        query_result(std::nullopt_t) {}

        query_result& operator=(std::nullopt_t)
        {
            storage = std::nullopt;
            return *this;
        }

        query_result(T val)
            : storage(std::move(val))
        {}

        query_result(query_issue_kind failure)
        {
            error_value = failure;
        }

        // Static helpers to clarify intent
        static auto success(T val) -> query_result { return query_result(std::move(val)); }
        
        static auto failure(query_issue_kind err) -> query_result
        {
            query_result res;
            res.error_value = std::move(err);
            return res;
        }

        // --- std::optional interface ---

        bool has_value() const { return storage.has_value(); }
        explicit operator bool() const { return storage.has_value(); }

        T& operator*() { return *storage; }
        const T& operator*() const { return *storage; }

        T* operator->() { return &*storage; }
        const T* operator->() const { return &*storage; }

        T& value()
        {
            if (!storage.has_value())
                throw std::bad_optional_access();

            return *storage;
        }

        template <typename U>
        T value_or(U&& default_value) const
        {
            return storage.has_value() ? *storage : static_cast<T>(std::forward<U>(default_value));
        }

        // --- Error access (expected-like) ---

        query_issue_kind& error() { return error_value; }
        const query_issue_kind& error() const { return error_value; }
    };    

// =====================================================================
// DETAILS
// =====================================================================

    namespace details
    {
// =====================================================================
// Dot-path splitting
// =====================================================================

        inline std::vector<std::string_view>
        split_dot_path(std::string_view path)
        {
            std::vector<std::string_view> out;

            std::size_t pos = 0;
            while (pos < path.size())
            {
                auto next = path.find('.', pos);
                out.push_back(
                    next == std::string_view::npos
                        ? path.substr(pos)
                        : path.substr(pos, next - pos)
                );

                if (next == std::string_view::npos)
                    break;

                pos = next + 1;
            }

            return out;
        }

        // Check if token is a row selector (-name-)
        inline bool is_row_selector(std::string_view token)
        {
            return token.size() >= 2 && token.front() == '-' && token.back() == '-';
        }

        // Check if token is a column selector (|name|)
        inline bool is_column_selector(std::string_view token)
        {
            return token.size() >= 2 && token.front() == '|' && token.back() == '|';
        }

        // Check if token is an array index ([n])
        inline bool is_array_index(std::string_view token)
        {
            return token.size() >= 3 && token.front() == '[' && token.back() == ']';
        }

        // Extract name from row selector
        inline std::string_view extract_row_name(std::string_view token)
        {
            return token.substr(1, token.size() - 2);
        }

        // Extract name from column selector
        inline std::string_view extract_column_name(std::string_view token)
        {
            return token.substr(1, token.size() - 2);
        }

        // Extract index from array selector
        inline std::optional<size_t> extract_array_index(std::string_view token)
        {
            auto inner = token.substr(1, token.size() - 2);
            size_t index = 0;
            auto [ptr, ec] = std::from_chars(
                inner.data(),
                inner.data() + inner.size(),
                index
            );
            
            if (ec == std::errc{} && ptr == inner.data() + inner.size())
                return index;
            
            return std::nullopt;
        }

// =====================================================================
// Core resolver
// =====================================================================

        using working_set = std::vector<value_location>;

        inline std::vector<value_location>
        enumerate_table_children(
            const document& doc,
            const value_location& parent,
            std::string_view token)
        {
            std::vector<value_location> out;

            reflect::inspect_context ctx{ &doc };
            auto insp = reflect::inspect(ctx, parent.addr);

            // If token is a row selector, extract the name
            std::string_view row_name;
            bool filter_by_name = false;
            
            if (is_row_selector(token))
            {
                row_name = extract_row_name(token);
                filter_by_name = true;
            }

            for (auto const& child : insp.structural_children(ctx))
            {
                if (child.kind != reflect::structural_child::kind::row)
                    continue;

                auto child_addr = insp.extend_address(child);
                
                // If filtering by name, check it
                if (filter_by_name)
                {
                    reflect::inspect_context row_ctx{ &doc };
                    auto row_insp = reflect::inspect(row_ctx, child_addr);
                    
                    if (auto row_view = std::get_if<document::table_row_view>(&row_insp.item))
                    {
                        if (row_view->name() != row_name)
                            continue;  // Skip non-matching rows
                    }
                }

                out.push_back({
                    child_addr,
                    location_kind::row_scope,
                    nullptr
                });
            }

            return out;
        }

        inline std::vector<value_location>
        enumerate_row_children(
            const document& doc,
            const value_location& parent,
            std::string_view token)
        {
            std::vector<value_location> out;

            reflect::inspect_context ctx{ &doc };
            auto insp = reflect::inspect(ctx, parent.addr);

            // Extract column name if token is a column selector
            std::string_view col_name = is_column_selector(token) 
                ? extract_column_name(token) 
                : token;

            for (auto const& child : insp.structural_children(ctx))
            {
                if (child.kind != reflect::structural_child::kind::column)
                    continue;

                if (child.name != col_name)
                    continue;

                auto child_addr = insp.extend_address(child);
                auto child_insp = reflect::inspect(ctx, child_addr);

                if (child_insp.value)
                {
                    out.push_back({
                        child_addr,
                        location_kind::terminal_value,
                        child_insp.value
                    });
                }
            }

            return out;
        }

        // Category → Tables → Rows → Cells
        inline std::vector<value_location>
        expand_category_to_cells(
            const document& doc,
            const value_location& parent,
            std::string_view col_token)
        {
            std::vector<value_location> out;
            
            reflect::inspect_context ctx{ &doc };
            auto insp = reflect::inspect(ctx, parent.addr);
            
            for (const auto& child : insp.structural_children(ctx))
            {
                if (child.kind != reflect::structural_child::kind::table)
                    continue;
                    
                value_location table_loc{
                    insp.extend_address(child),
                    location_kind::table_scope,
                    nullptr
                };
                
                auto rows = enumerate_table_children(doc, table_loc, "");
                for (auto& row : rows)
                {
                    auto cells = enumerate_row_children(doc, row, col_token);
                    out.insert(out.end(), cells.begin(), cells.end());
                }
            }
            
            return out;
        }
        
        // Table → Rows → Cells
        inline std::vector<value_location>
        expand_table_to_cells(
            const document& doc,
            const value_location& parent,
            std::string_view col_token)
        {
            std::vector<value_location> out;
            
            auto rows = enumerate_table_children(doc, parent, "");
            for (auto& row : rows)
            {
                auto cells = enumerate_row_children(doc, row, col_token);
                out.insert(out.end(), cells.begin(), cells.end());
            }
            
            return out;
        }

    // --------------------------------------------------------------
    // Core resolver: Enumerators
    // --------------------------------------------------------------

        inline std::vector<value_location>
        enumerate_category_children(
            const document& doc,
            const value_location& parent,
            std::string_view token)
        {
            std::vector<value_location> out;

            reflect::inspect_context ctx{ &doc };
            auto insp = reflect::inspect(ctx, parent.addr);

            // ============================================================
            // Handle table ordinal syntax: #0, #1, #2, etc.
            // Bare # (no ordinal) selects ALL tables in scope.
            // ============================================================
            if (token.starts_with('#'))
            {
                // Bare "#" → all tables
                if (token.size() == 1)
                {
                    for (const auto& child : insp.structural_children(ctx))
                    {
                        if (child.kind == reflect::structural_child::kind::table)
                        {
                            out.push_back({
                                insp.extend_address(child),
                                location_kind::table_scope,
                                nullptr
                            });
                        }
                    }
                    return out;
                }

                // "#n" → specific table by ordinal
                auto ordinal_str = token.substr(1);
                size_t target_ordinal = 0;
                
                auto [ptr, ec] = std::from_chars(
                    ordinal_str.data(),
                    ordinal_str.data() + ordinal_str.size(),
                    target_ordinal
                );
                
                // Valid ordinal parsed - find matching table
                if (ec == std::errc{} && ptr == ordinal_str.data() + ordinal_str.size())
                {
                    for (const auto& child : insp.structural_children(ctx))
                    {
                        using st = reflect::structural_child;
                        
                        if (child.kind == st::kind::table && child.ordinal == target_ordinal)
                        {
                            auto child_addr = insp.extend_address(child);
                            out.push_back({
                                child_addr,
                                location_kind::table_scope,
                                nullptr
                            });
                            return out;  // Found the table, we're done
                        }
                    }
                }
                
                // If we get here, ordinal was invalid or table not found
                return out;  // Return empty
            }

            // ============================================================
            // Normal name-based matching
            // ============================================================
            for (const auto& child : insp.structural_children(ctx))
            {
                using st = reflect::structural_child;
                
                if (child.name != token)
                    continue;

                auto child_addr = insp.extend_address(child);
                auto child_insp = reflect::inspect(ctx, child_addr);

                // Keys: extract value manually
                if (child.kind == st::kind::key)
                {
                    // Try to get key_view first
                    if (auto key_view = std::get_if<document::key_view>(&child_insp.item))
                    {
                        out.push_back({
                            child_addr,
                            location_kind::terminal_value,
                            &key_view->value()
                        });
                    }
                    // For array keys, reflection stores the value directly
                    else if (auto value_ptr = std::get_if<const typed_value*>(&child_insp.item))
                    {
                        out.push_back({
                            child_addr,
                            location_kind::terminal_value,
                            *value_ptr
                        });
                    }
                    else
                    {
                        assert(false && "only keys and arrays allowed");
                    }                  
                }
                // Tables (by name - rare but supported)
                else if (child.kind == st::kind::table)
                {
                    out.push_back({
                        child_addr,
                        location_kind::table_scope,
                        nullptr
                    });
                }
                // Subcategories
                else if (child.kind == st::kind::sub_category || 
                        child.kind == st::kind::top_category)
                {
                    out.push_back({
                        child_addr,
                        location_kind::category_scope,
                        nullptr
                    });
                }
            }

            return out;
        }

        inline std::vector<value_location>
        enumerate_value_children(
            const document& doc,
            const value_location& parent,
            std::string_view token)
        {
            std::vector<value_location> out;

            if (!parent.value_ptr)
                return out;

            if (!is_array(parent.value_ptr->type))
                return out;

            // Extract index from [n] syntax
            auto index_opt = is_array_index(token) 
                ? extract_array_index(token) 
                : std::nullopt;
            
            if (!index_opt)
                return out;  // Invalid index syntax

            size_t index = *index_opt;

            reflect::inspect_context ctx{ &doc };
            auto insp = reflect::inspect(ctx, parent.addr);

            for (auto const& child : insp.structural_children(ctx))
            {
                if (child.kind != reflect::structural_child::kind::index)
                    continue;

                if (child.ordinal != index)
                    continue;

                auto child_addr = insp.extend_address(child);
                auto child_insp = reflect::inspect(ctx, child_addr);

                if (child_insp.value)
                {
                    out.push_back({
                        child_addr,
                        location_kind::terminal_value,
                        child_insp.value
                    });
                }
            }

            return out;
        }

        inline std::vector<value_location>
        enumerate_table_to_cells(
            const document& doc,
            const value_location& parent,
            std::string_view col_token)
        {
            std::vector<value_location> out;

            // Step 1: Get all rows (reuse enumerate_table_children)
            auto all_rows = enumerate_table_children(doc, parent, "");
            
            // Step 2: Extract column from each row (reuse enumerate_row_children)
            for (auto& row_loc : all_rows)
            {
                auto cells = enumerate_row_children(doc, row_loc, col_token);
                out.insert(out.end(), cells.begin(), cells.end());
            }

            return out;
        }       

        inline std::vector<value_location>
        enumerate_category_to_cells(
            const document& doc,
            const value_location& parent,
            std::string_view col_token)
        {
            std::vector<value_location> out;

            reflect::inspect_context ctx{ &doc };
            auto insp = reflect::inspect(ctx, parent.addr);

            for (const auto& child : insp.structural_children(ctx))
            {
                if (child.kind != reflect::structural_child::kind::table)
                    continue;

                value_location table_loc{
                    insp.extend_address(child),
                    location_kind::table_scope,
                    nullptr
                };

                auto cells = enumerate_table_to_cells(doc, table_loc, col_token);
                out.insert(out.end(), cells.begin(), cells.end());
            }

            return out;
        }        

        inline std::vector<value_location>
        collapse_row_to_cell(
            const document& doc,
            const value_location& row_loc,
            std::string_view column)
        {
            return enumerate_row_children(
                doc,
                row_loc,
                "|" + std::string(column) + "|"
            );
        }

    // --------------------------------------------------------------
    // Core resolver: Dispatch
    // --------------------------------------------------------------

        inline std::vector<value_location>
        enumerate_matching_children(
            const document& doc,
            const value_location& loc,
            std::string_view token)
        {
            // Array index selector: [n]
            // ============================================================
            if (is_array_index(token))
            {
                if (loc.kind == location_kind::terminal_value)
                    return enumerate_value_children(doc, loc, token);
                
                // Error: array index on non-value
                return {};
            }

            // Normal dispatch for other tokens
            // ============================================================
            switch (loc.kind)
            {
                case location_kind::category_scope:
                    return enumerate_category_children(doc, loc, token);

                case location_kind::table_scope:
                    return enumerate_table_children(doc, loc, token);

                case location_kind::row_scope:
                    return enumerate_row_children(doc, loc, token);

                case location_kind::terminal_value:
                    return enumerate_value_children(doc, loc, token);
            }

            return {};
        }

    // --------------------------------------------------------------
    // Core resolver loop
    // --------------------------------------------------------------

        inline std::vector<value_location>
        resolve_axis_selections(
            const document& doc,
            const std::vector<value_location>& input,
            query_handle::axis_selection& axis)
        {
            std::vector<value_location> out;

            for (const auto& loc : input)
            {
                // --------------------------------------------------
                // Category scope
                // --------------------------------------------------
                if (loc.kind == location_kind::category_scope)
                {
                    reflect::inspect_context ctx{ &doc };
                    auto insp = reflect::inspect(ctx, loc.addr);

                    if (axis.row && axis.column)
                    {
                        // Both row and column specified
                        for (const auto& child : insp.structural_children(ctx))
                        {
                            if (child.kind != reflect::structural_child::kind::table)
                                continue;

                            value_location table_loc{
                                insp.extend_address(child),
                                location_kind::table_scope,
                                nullptr
                            };

                            // Find rows matching the row selector
                            auto rows = enumerate_table_children(
                                doc,
                                table_loc,
                                "-" + std::string(*axis.row) + "-"
                            );

                            // Extract the column from each matching row
                            for (auto& row : rows)
                            {
                                auto cells = enumerate_row_children(
                                    doc, 
                                    row, 
                                    "|" + std::string(*axis.column) + "|"
                                );
                                out.insert(out.end(), cells.begin(), cells.end());
                            }
                        }
                    }
                    else if (axis.column)
                    {
                        // Column only - extract from all rows
                        auto cells = enumerate_category_to_cells(
                            doc, 
                            loc, 
                            "|" + std::string(*axis.column) + "|"
                        );
                        out.insert(out.end(), cells.begin(), cells.end());
                    }
                    else if (axis.row)
                    {
                        // Row only - return matching rows
                        for (const auto& child : insp.structural_children(ctx))
                        {
                            if (child.kind != reflect::structural_child::kind::table)
                                continue;

                            value_location table_loc{
                                insp.extend_address(child),
                                location_kind::table_scope,
                                nullptr
                            };

                            auto rows = enumerate_table_children(
                                doc,
                                table_loc,
                                "-" + std::string(*axis.row) + "-"
                            );
                            out.insert(out.end(), rows.begin(), rows.end());
                        }
                    }
                }

                // --------------------------------------------------
                // Table scope
                // --------------------------------------------------
                else if (loc.kind == location_kind::table_scope)
                {
                    if (axis.row && axis.column)
                    {
                        // Find matching rows, extract column
                        auto rows = enumerate_table_children(
                            doc, 
                            loc, 
                            "-" + std::string(*axis.row) + "-"
                        );

                        for (auto& row : rows)
                        {
                            auto cells = enumerate_row_children(
                                doc, 
                                row, 
                                "|" + std::string(*axis.column) + "|"
                            );
                            out.insert(out.end(), cells.begin(), cells.end());
                        }
                    }
                    else if (axis.row)
                    {
                        auto rows = enumerate_table_children(
                            doc, 
                            loc, 
                            "-" + std::string(*axis.row) + "-"
                        );
                        out.insert(out.end(), rows.begin(), rows.end());
                    }
                    else if (axis.column)
                    {
                        auto cells = enumerate_table_to_cells(
                            doc, 
                            loc, 
                            "|" + std::string(*axis.column) + "|"
                        );
                        out.insert(out.end(), cells.begin(), cells.end());
                    }
                }

                // --------------------------------------------------
                // Row scope
                // --------------------------------------------------
                else if (loc.kind == location_kind::row_scope)
                {
                    if (axis.column)
                    {
                        auto cells = enumerate_row_children(
                            doc, 
                            loc, 
                            "|" + std::string(*axis.column) + "|"
                        );
                        out.insert(out.end(), cells.begin(), cells.end());
                    }
                }
            }

            return out;
        }

        inline working_set
        resolve_step(
            const document& doc,
            const working_set& current,
            std::string_view token,
            query_handle::axis_selection& axis,
            std::vector<diagnostic>& diagnostics_,
            size_t step_index)
        {
            if (is_array_index(token))
            {
                auto inner = token.substr(1, token.size() - 2);
                size_t dummy;
                auto [ptr, ec] = std::from_chars(inner.data(), inner.data() + inner.size(), dummy);
                if (ec != std::errc{})
                {
                    diagnostics_.push_back({
                        diagnostic_kind::invalid_index,
                        std::string(token),
                        step_index
                    });
                }
            }
            // Malformed row/column selectors
            else if (token.size() == 2 && (token == "--" || token == "||"))
            {
                diagnostics_.push_back({
                    diagnostic_kind::spelling_error,
                    std::string(token),
                    step_index
                });
            }

            // Axis tokens: do not enumerate yet
            if (is_row_selector(token))
            {
                axis.row = extract_row_name(token);
                return current;
            }

            if (is_column_selector(token))
            {
                axis.column = extract_column_name(token);
                return current;
            }

            // Structural tokens: normal enumeration
            working_set next;
            for (const auto& loc : current)
            {
                auto matches = enumerate_matching_children(doc, loc, token);
                next.insert(next.end(), matches.begin(), matches.end());
            }

            return next;
        }
        
        inline working_set
        resolve_dot_path(
            const document& doc,
            std::span<const std::string_view> segments,
            query_handle::axis_selection& axis,
            std::vector<query_issue>& issues_,
            std::vector<diagnostic>& diagnostics_)
        {
            working_set current;
            //query_handle::axis_selection axis;

            // Seed: root category scope
            current.push_back({
                reflect::root(),
                location_kind::category_scope,
                nullptr
            });

            size_t i = 0;
            for (auto seg : segments)
            {
                if (seg.empty())
                {
                    issues_.push_back({
                        query_issue_kind::syntax_error,
                        std::string("empty dotpath segment"),
                        i
                    });    
                    diagnostics_.push_back({
                        diagnostic_kind::empty_scope,
                        std::string("empty dotpath segment"),
                        i
                    });                    
                    assert(false && "decide what to do: abort, continue or continue empty");
                    continue;
                }

                // Axis selectors do NOT resolve immediately
                if (details::is_row_selector(seg))
                {
                    axis.row = details::extract_row_name(seg);
                    continue;
                }

                if (details::is_column_selector(seg))
                {
                    axis.column = details::extract_column_name(seg);
                    continue;
                }

                // If we are about to structurally descend,
                // first resolve any pending axis selections
                if (axis.row || axis.column)
                {
                    current = resolve_axis_selections(doc, current, axis);
                    axis.reset();  // ← Just reset, don't check any flags

                    if (current.empty())
                        return current;
                }

                // Normal structural step
                current = resolve_step(doc, current, seg, axis, diagnostics_, i);
                if (current.empty())
                    return current;

                ++i;
            }

            // Final axis resolution at end of path
            if (axis.row || axis.column)
            {
                current = resolve_axis_selections(doc, current, axis);
                axis.reset();
            }

            return current;
        }

        std::optional<size_t>
        resolve_column_index(
            const document::table_view& table,
            const column_ref& ref)
        {
            if (auto idx = std::get_if<size_t>(&ref.ref))
                return *idx;

            const auto& name = std::get<std::string>(ref.ref);

            const auto& header = table.columns();
            for (size_t i = 0; i < header.size(); ++i)
                if (auto const & col = table.doc->column(header[i]); 
                    col.has_value() && col->name() == name)
                        return i;

            return std::nullopt;
        }
    } // ns details

// =====================================================================
// IMPLEMENTATIONS
// =====================================================================

    void query_handle::report_issue(query_issue_kind kind, std::string_view context, size_t line) const noexcept
    {
        issues_.push_back({ kind, std::string(context), line });
    }
    void query_handle::report_if_empty(query_issue_kind kind, std::string_view context, size_t line) const noexcept
    {
        if (locations_.empty())
            report_issue(kind, context, line);
    }

    query_handle& query_handle::select(std::string_view path)
    {
        auto segments = details::split_dot_path(path);

        locations_.clear();
        issues_.clear();

        locations_ = details::resolve_dot_path(
            *doc_,
            std::span<const std::string_view>(segments.data(), segments.size()),
            pending_axis_,
            issues_,
            diagnostics_
        );

        report_if_empty(query_issue_kind::not_found,
                        std::string(path),
                        segments.empty() ? 0 : segments.size() - 1);
        
        if (locations_.size() > 1)
            report_issue(query_issue_kind::ambiguous,
                         std::string(path),
                         segments.size() - 1);

        return *this;
    }

    query_handle& query_handle::child(std::string_view name)
    {
        std::vector<value_location> next;
        issues_.clear();

        for (const auto& loc : locations_)
        {
            reflect::inspect_context ctx{ doc_ };
            auto insp = reflect::inspect(ctx, loc.addr);

            for (const auto& child : insp.structural_children(ctx))
            {
                if (child.name != name)
                    continue;

                auto child_addr = insp.extend_address(child);
                auto child_insp = reflect::inspect(ctx, child_addr);

                location_kind kind = location_kind::category_scope;
                const typed_value* value_ptr = nullptr;

                using st = reflect::structural_child;
                switch (child.kind)
                {
                    case st::kind::key:
                        kind = location_kind::terminal_value;
                        if (auto key_view = std::get_if<document::key_view>(&child_insp.item))
                            value_ptr = &key_view->value();
                        else if (auto val_ptr = std::get_if<const typed_value*>(&child_insp.item))
                            value_ptr = *val_ptr;
                        break;

                    case st::kind::table:
                        kind = location_kind::table_scope;
                        break;

                    case st::kind::sub_category:
                    case st::kind::top_category:
                        kind = location_kind::category_scope;
                        break;

                    default:
                        continue;  // Skip non-nameable children (rows, columns, indices)
                }

                next.push_back({ child_addr, kind, value_ptr });
            }
        }

        locations_ = std::move(next);

        report_if_empty(query_issue_kind::not_found, 
                        "child(\"" + std::string(name) + "\") - not found");

        return *this;
    }

    query_handle& query_handle::tables()
    {
        std::vector<value_location> next;
        issues_.clear();

        for (const auto& loc : locations_)
        {
            if (loc.kind != location_kind::category_scope)
                continue;

            reflect::inspect_context ctx{ doc_ };
            auto insp = reflect::inspect(ctx, loc.addr);

            for (const auto& child : insp.structural_children(ctx))
            {
                if (child.kind != reflect::structural_child::kind::table)
                    continue;

                auto child_addr = insp.extend_address(child);

                next.push_back({
                    child_addr,
                    location_kind::table_scope,
                    nullptr
                });
            }
        }

        locations_ = std::move(next);

        report_if_empty(query_issue_kind::empty_result,
                        "tables() - no tables in current scope");

        return *this;
    }
    
    query_handle& query_handle::table(size_t ordinal)
    {
        issues_.clear();

        // First enumerate tables in the current scope
        tables();

        if (locations_.empty())
            return *this;

        if (ordinal >= locations_.size())
        {
            locations_.clear();

            report_issue(query_issue_kind::invalid_index,
                         "table(" + std::to_string(ordinal) + ") - only " + 
                         std::to_string(locations_.size()) + " tables available");

            return *this;
        }

        value_location selected = locations_[ordinal];
        locations_.clear();
        locations_.push_back(selected);

        return *this;
    }

    query_handle& query_handle::rows()
    {
        std::vector<value_location> next;
        issues_.clear();

        for (const auto& loc : locations_)
        {
            if (loc.kind != location_kind::table_scope)
                continue;

            reflect::inspect_context ctx{ doc_ };
            auto insp = reflect::inspect(ctx, loc.addr);

            for (const auto& child : insp.structural_children(ctx))
            {
                if (child.kind != reflect::structural_child::kind::row)
                    continue;

                auto child_addr = insp.extend_address(child);

                next.push_back({
                    child_addr,
                    location_kind::row_scope,
                    nullptr
                });
            }
        }

        locations_ = std::move(next);

        flush_pending_axis_();

        report_if_empty(query_issue_kind::empty_result,
                        "rows() - current scope has no rows");

        return *this;
    }

    query_handle& query_handle::row(size_t ordinal)
    {
        issues_.clear();

        rows();

        if (locations_.empty())
            return *this;

        if (ordinal >= locations_.size())
        {
            locations_.clear();

            report_issue(query_issue_kind::invalid_index,
                         "row(" + std::to_string(ordinal) + ") - index out of range, only " + 
                         std::to_string(locations_.size()) + " rows available");

            return *this;
        }

        auto chosen = locations_[ordinal];
        locations_.clear();
        locations_.push_back(chosen);

        flush_pending_axis_();

        return *this;
    }

    bool query_handle::all_locations_are(location_kind scope) const noexcept
    {
        for (const auto& loc : locations_)
            if (loc.kind != scope)
                return false;
        return true;
    }

    query_handle& query_handle::row(std::string_view name)
    {
        issues_.clear();

        // If already in row scope, filter existing rows (progressive filtering)
        if (all_locations_are(location_kind::row_scope))
        {
            std::vector<value_location> filtered;
            
            for (const auto& loc : locations_)
            {
                reflect::inspect_context ctx{ doc_ };
                auto insp = reflect::inspect(ctx, loc.addr);
                
                if (auto row_view = std::get_if<document::table_row_view>(&insp.item))
                {
                    if (row_view->name() == name)
                        filtered.push_back(loc);
                }
            }
            
            locations_ = std::move(filtered);
            
            report_if_empty(query_issue_kind::empty_result,
                            "row(\"" + std::string(name) + "\") - no matching rows in current set");
            
            return *this;
        }

        // Set pending axis
        pending_axis_.row = name;

        // Resolve immediately if:
        // 1. Column is also pending (cell selection)
        // 2. We're in table/category scope (row selection without column)
        bool should_resolve = pending_axis_.column.has_value() ||
                            !locations_.empty() && 
                            (locations_.front().kind == location_kind::table_scope ||
                            locations_.front().kind == location_kind::category_scope);

        if (should_resolve)
        {
            auto before_size = locations_.size();
            locations_ = details::resolve_axis_selections(*doc_, locations_, pending_axis_);
            pending_axis_.reset();
            
            if (locations_.empty() && before_size > 0)
            {
                diagnostics_.push_back({
                    diagnostic_kind::row_not_found,
                    std::string(name),
                    0
                });
            }
        }

        report_if_empty(query_issue_kind::empty_result,
                        "row(\"" + std::string(name) + "\") - row not found in table");

        return *this;
    }

    query_handle& query_handle::columns()
    {
        std::vector<value_location> next;
        issues_.clear();

        for (const auto& loc : locations_)
        {
            // row → cells
            if (loc.kind == location_kind::row_scope)
            {
                reflect::inspect_context ctx{ doc_ };
                auto insp = reflect::inspect(ctx, loc.addr);

                for (const auto& child : insp.structural_children(ctx))
                {
                    if (child.kind != reflect::structural_child::kind::column)
                        continue;

                    auto child_addr = insp.extend_address(child);
                    auto child_insp = reflect::inspect(ctx, child_addr);

                    if (child_insp.value)
                    {
                        next.push_back({
                            child_addr,
                            location_kind::terminal_value,
                            child_insp.value
                        });
                    }
                }
            }

            // table → rows → cells
            else if (loc.kind == location_kind::table_scope)
            {
                auto cells = details::enumerate_table_to_cells(*doc_, loc, "");
                next.insert(next.end(), cells.begin(), cells.end());
            }

            // category → tables → rows → cells
            else if (loc.kind == location_kind::category_scope)
            {
                auto cells = details::enumerate_category_to_cells(*doc_, loc, "");
                next.insert(next.end(), cells.begin(), cells.end());
            }
        }

        locations_ = std::move(next);

        flush_pending_axis_();

        report_if_empty(query_issue_kind::empty_result,
                        "columns() - no columns in current scope");

        return *this;
    }

    query_handle& query_handle::column(size_t index)
    {
        std::vector<value_location> next;
        issues_.clear();

        // Ensure column context exists (commutative)
        columns();
        bool reported = false;

        if (index >= locations_.size())
        {
            report_issue(query_issue_kind::invalid_index,
                         "column(" + std::to_string(index) + ") - index out of range, only " + 
                         std::to_string(locations_.size()) + " columns available");
            reported = true;
        }    

        else for (const auto& loc : locations_)
        {
            if (loc.kind != location_kind::terminal_value || !loc.value_ptr)
                continue;

            reflect::inspect_context ctx{ doc_ };
            auto insp = reflect::inspect(ctx, loc.addr);

            for (const auto& child : insp.structural_children(ctx))
            {
                if (child.kind == reflect::structural_child::kind::column &&
                    child.ordinal == index)
                {
                    auto child_addr = insp.extend_address(child);
                    auto child_insp = reflect::inspect(ctx, child_addr);

                    if (child_insp.value)
                    {
                        next.push_back({
                            child_addr,
                            location_kind::terminal_value,
                            child_insp.value
                        });
                    }
                }
            }
        }

        locations_ = std::move(next);

        flush_pending_axis_();

        if (locations_.empty() && !reported)
            report_issue(query_issue_kind::invalid_index,
                         "column(" + std::to_string(index) + ") - invalid column index");            

        return *this;
    }

    query_handle& query_handle::column(std::string_view name)
    {
        issues_.clear();

        // Set pending axis
        pending_axis_.column = name;

        // Resolve if row is also pending
        if (pending_axis_.row)
        {
            auto before_size = locations_.size();
            locations_ = details::resolve_axis_selections(*doc_, locations_, pending_axis_);
            pending_axis_.reset();
            
            // Diagnostic: named column didn't match anything
            if (locations_.empty() && before_size > 0)
            {
                diagnostics_.push_back({
                    diagnostic_kind::column_not_found,
                    std::string(name),
                    0
                });
            }
        }

        // Or if we're already in a terminal value context (from a prior column call)
        else if (!locations_.empty() && locations_.front().kind == location_kind::terminal_value)
        {
            // Already resolved - do nothing
            pending_axis_.reset();
        }

        report_if_empty(query_issue_kind::not_found,
                        std::string("column(\"") + std::string(name) + "\") - column not found");

        return *this;
    }

    query_handle& query_handle::index(size_t n)
    {
        std::vector<value_location> next;
        issues_.clear();

        for (const auto& loc : locations_)
        {
            if (loc.kind != location_kind::terminal_value || !loc.value_ptr)
                continue;

            if (!is_array(loc.value_ptr->type))
                continue;

            reflect::inspect_context ctx{ doc_ };
            auto insp = reflect::inspect(ctx, loc.addr);

            for (const auto& child : insp.structural_children(ctx))
            {
                if (child.kind == reflect::structural_child::kind::index &&
                    child.ordinal == n)
                {
                    auto child_addr = insp.extend_address(child);
                    auto child_insp = reflect::inspect(ctx, child_addr);

                    if (child_insp.value)
                    {
                        next.push_back({
                            child_addr,
                            location_kind::terminal_value,
                            child_insp.value
                        });
                    }
                }
            }

            if (next.empty() && !locations_.empty())
            {
                diagnostics_.push_back({
                    diagnostic_kind::index_out_of_bounds,
                    "index(" + std::to_string(n) + ") - array index out of bounds or not an array",
                    0
                });
            }            
        }

        locations_ = std::move(next);

        flush_pending_axis_();

        report_if_empty(query_issue_kind::invalid_index,
                        "index(" + std::to_string(n) + ") - array index out of bounds or not an array");

        return *this;
    }

    bool evaluate_predicate(const typed_value& lhs, const predicate& pred)
    {
        const typed_value& rhs = pred.rhs;

        // Semantic sanity
        if (!is_valid(lhs) || !is_valid(rhs))
            return false;

        // Arrays are explicitly unsupported
        if (is_array(lhs) || is_array(rhs))
            return false;

        // --------
        // Numeric
        // --------
        if (is_numeric(lhs) && is_numeric(rhs))
        {
            const double l =
                lhs.type == value_type::integer
                    ? static_cast<double>(std::get<int64_t>(lhs.val))
                    : std::get<double>(lhs.val);

            const double r =
                rhs.type == value_type::integer
                    ? static_cast<double>(std::get<int64_t>(rhs.val))
                    : std::get<double>(rhs.val);

            switch (pred.op)
            {
                case predicate_op::eq: return l == r;
                case predicate_op::ne: return l != r;
                case predicate_op::lt: return l <  r;
                case predicate_op::le: return l <= r;
                case predicate_op::gt: return l >  r;
                case predicate_op::ge: return l >= r;
            }

            return false;
        }

        // --------
        // String
        // --------
        if (is_string(lhs) && is_string(rhs))
        {
            const auto& l = std::get<std::string>(lhs.val);
            const auto& r = std::get<std::string>(rhs.val);

            switch (pred.op)
            {
                case predicate_op::eq: return l == r;
                case predicate_op::ne: return l != r;
                case predicate_op::lt: return l <  r;
                case predicate_op::le: return l <= r;
                case predicate_op::gt: return l >  r;
                case predicate_op::ge: return l >= r;
            }

            return false;
        }

        // --------
        // Boolean
        // --------
        if (is_boolean(lhs) && is_boolean(rhs))
        {
            const bool l = std::get<bool>(lhs.val);
            const bool r = std::get<bool>(rhs.val);

            switch (pred.op)
            {
                case predicate_op::eq: return l == r;
                case predicate_op::ne: return l != r;
                default:
                    // Ordering comparisons on booleans are meaningless
                    return false;
            }
        }

        // --------
        // Everything else
        // --------
        return false;
    }

    query_handle& query_handle::where(predicate pred)
    {
        std::vector<value_location> next;
        issues_.clear();

    // Shorthand: where() on table == rows().where()
    // -----------------------------------------------
        // Expand any table-scope locations to rows
        bool has_rows = false;
        bool has_tables = false;
        bool has_cats = false;

        for (auto const& loc : locations_)
        {
            if (loc.kind == location_kind::row_scope)
                has_rows = true;
            else if (loc.kind == location_kind::table_scope)
                has_tables = true;
            else if (loc.kind == location_kind::category_scope)
                has_cats = true;
        }

        // Expand only as needed, preserving narrowing semantics
        if (!has_rows)
        {
            if (has_cats)
                tables();

            if (has_tables || has_cats)
                rows();
        }

    // Main filter:
    // -----------------------------------------------
        for (const auto& loc : locations_)
        {
            if (loc.kind != location_kind::row_scope)
                continue;

            reflect::inspect_context ctx{ doc_ };
            auto insp = reflect::inspect(ctx, loc.addr);

            if (!insp.ok())
                continue;

            auto row_view = std::get_if<document::table_row_view>(&insp.item);
            if (!row_view)
                continue;

            auto idx = details::resolve_column_index(row_view->table(), pred.column);
            if (!idx)
            {
                report_issue(query_issue_kind::invalid_index, "where()");
                continue;
            }

            // Notice that the predicate here is a copy
            // and changes will currently not be propagated. 
            // pred.column.ref = *idx; // canonicalise

            const auto & cells = row_view->node->cells;
            if (*idx >= cells.size())
                continue;
            
            const typed_value & cell = cells.at(*idx);
            if (cell.type == value_type::unresolved)
                continue;

            if (evaluate_predicate(cell, pred))
                next.push_back(loc);
        }

        locations_ = std::move(next);

        if (locations_.empty())
        {
            report_issue(query_issue_kind::empty_result,
                         "where() - predicate eliminated all rows");
            
            diagnostics_.push_back({
                diagnostic_kind::empty_scope,
                "predicate eliminated all rows",
                0
            });
        }

        return *this;
    }

    query_handle& query_handle::project_impl(
        std::span<const std::string_view> column_names)
    {
        std::vector<value_location> next;
        issues_.clear();

        for (const auto& loc : locations_)
        {
            if (loc.kind != location_kind::row_scope)
                continue;

            reflect::inspect_context ctx{ doc_ };
            auto insp = reflect::inspect(ctx, loc.addr);

            if (!insp.ok())
                continue;

            auto row_view =
                std::get_if<document::table_row_view>(&insp.item);

            if (!row_view)
                continue;

            for (auto name : column_names)
            {
                auto idx =
                    details::resolve_column_index(row_view->table(), {std::string(name)});

                if (!idx)
                {
                    report_issue(query_issue_kind::invalid_index, 
                                 std::string("project(\"") + std::string(name) + "\")");
                    continue;
                }

                const auto& cells = row_view->node->cells;
                if (*idx >= cells.size())
                    continue;

                const typed_value& cell = cells[*idx];

                if (cell.type == value_type::unresolved)
                    continue;

                auto child = reflect::structural_child{
                    .kind    = reflect::structural_child::kind::column,
                    .name    = name,
                    .ordinal = *idx
                };

                next.push_back({
                    insp.extend_address(child),
                    location_kind::terminal_value,
                    &cell
                });
            }
        }

        locations_ = std::move(next);

        report_if_empty(query_issue_kind::empty_result,
                        "project() - no columns matched projection");

        return *this;
    }

    void query_handle::flush_pending_axis_()
    {
        if (!pending_axis_.row && !pending_axis_.column)
            return;

        locations_ =
            details::resolve_axis_selections(*doc_, locations_, pending_axis_);

        pending_axis_.reset();
    }

    template<value_type vt>
    typed_value const *
    query_handle::common_extraction_checks(query_issue_kind* err) const noexcept
    {
        *err = query_issue_kind::none;

        if (locations_.empty())
        {
            *err = query_issue_kind::empty_result;
            report_issue(*err, "<extraction>");
            return nullptr;
        }

        if (locations_.size() > 1)
        {
            *err = query_issue_kind::ambiguous;
            report_issue(*err, "<extraction>");             
            return nullptr;
        }

        auto* v = locations_.front().value_ptr;

        if (!v || v->type != vt)
        {
            *err = query_issue_kind::type_mismatch;
            report_issue(*err, "<extraction>");             
            return nullptr;
        }

        return v;
    }

    template<typename T>
    bool query_handle::extract_convert(T & out, query_issue_kind* err) const noexcept
    {
        if (auto const * vp = locations_.front().value_ptr; vp != nullptr)
        {
            if (auto const & sl = vp->source_literal; sl.has_value())
            {
                if (auto res = std::from_chars(sl->data(), sl->data() + sl->size(), out); 
                    res.ec == std::errc{})
                    return true;
                else
                    *err = query_issue_kind::conversion_failed;
            }
            else 
                *err = query_issue_kind::missing_literal;
        }
        else
            *err = query_issue_kind::not_a_value;
        return false;
    }
    template<>
    bool query_handle::extract_convert<std::string>(std::string & out, query_issue_kind* err) const noexcept
    {
        if (auto vp = locations_.front().value_ptr; vp != nullptr)
        {
            if (auto const & sl = vp->source_literal; sl.has_value())
            {
                out = *sl;
                return true;
            }
            else *err = query_issue_kind::missing_literal;
        }
        else
            *err = query_issue_kind::not_a_value;

        return false;
    }

    template<value_type Vt, typename T>
    query_result<T>
    query_handle::scalar_extract(bool convert) const noexcept
    {
        // Flush any pending axis selections before extraction
        const_cast<query_handle*>(this)->flush_pending_axis_();

        query_issue_kind err;

        if (auto v = common_extraction_checks<Vt>(&err); v != nullptr)
            return std::get<T>(v->val);

        if (convert)
        {
            T v{};
            if (extract_convert(v, &err))
                return v;
        }

        return {err};
    }

    query_result<int64_t> query_handle::as_integer(bool convert) const noexcept
    {
        return scalar_extract<value_type::integer, int64_t>(convert);
    }

    query_result<double> query_handle::as_real(bool convert) const noexcept
    {
        return scalar_extract<value_type::decimal, double>(convert);
    }

    query_result<bool> query_handle::as_bool() const noexcept
    {
        // Flush any pending axis selections before extraction
        const_cast<query_handle*>(this)->flush_pending_axis_();

        query_issue_kind err;
        if (auto v = common_extraction_checks<value_type::boolean>(&err); v != nullptr)
            return std::get<bool>(v->val);
        return {err};
    }

    query_result<std::string> query_handle::as_string(bool convert) const noexcept
    {
        return scalar_extract<value_type::string, std::string>(convert);
    }

// =====================================================================
// Entry points
// =====================================================================

    inline query_handle query(const document& doc) noexcept
    {
        return query_handle{ doc };
    }

    inline query_handle query(const document& doc, std::string_view path) noexcept
    {
        query_handle q{ doc };
        q.select(path);
        return q;
    }

    inline query_result<int64_t> get_integer(const document& doc, std::string_view path) noexcept
    {
        return query(doc, path).as_integer();
    }

    inline query_result<double> get_real(const document& doc, std::string_view path) noexcept
    {
        return query(doc, path).as_real();
    }

    inline query_result<std::string> get_string(const document& doc, std::string_view path) noexcept
    {
        return query(doc, path).as_string();
    }

    inline query_result<bool> get_bool(const document& doc, std::string_view path) noexcept
    {
        return query(doc, path).as_bool();
    }

    inline query_result<int64_t> get_as_integer(const document& doc, std::string_view path) noexcept
    {
        return query(doc, path).as_integer(true);
    }

    inline query_result<double> get_as_real(const document& doc, std::string_view path) noexcept
    {
        return query(doc, path).as_real(true);
    }

    inline query_result<std::string> get_as_string(const document& doc, std::string_view path) noexcept
    {
        return query(doc, path).as_string(true);
    }

} // namespace arf

#endif // ARF_QUERY_HPP

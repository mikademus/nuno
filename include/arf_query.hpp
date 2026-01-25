// arf_query.hpp - A Readable Format (Arf!) - Query Interface
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

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

// =====================================================================
// Public API:
// =====================================================================

    inline query_handle query(const document& doc) noexcept;
    inline query_handle query(const document& doc, std::string_view path) noexcept;

// Convenience getters
// -------------------    
    inline query_result<int64_t>        get_integer(const document& doc, std::string_view path) noexcept;
    inline query_result<double>         get_real(const document& doc, std::string_view path) noexcept;
    inline query_result<std::string>    get_string(const document& doc, std::string_view path) noexcept;
    inline query_result<bool>           get_bool(const document& doc, std::string_view path) noexcept;

// Converting convenience getters
// -------------------    
    inline query_result<int64_t>        get_as_integer(const document& doc, std::string_view path) noexcept;
    inline query_result<double>         get_as_real(const document& doc, std::string_view path) noexcept;
    inline query_result<std::string>    get_as_string(const document& doc, std::string_view path) noexcept;

// =====================================================================
// Query issues
// =====================================================================

    enum class query_issue_kind
    {
        none,
        empty,              // enumeration returned empty result set
        not_found,          // queried item not found
        invalid_index,      // OOB index / the index doesn't exist 
        ambiguous,          // multiple matches
        type_mismatch,      // value is of another type than requested
        conversion_failed,  // attempt to convert value between types failed
        no_literal,         // the source_literal member of the value is empty
        not_a_value,        // the queried address is not a value terminal
    };

    struct query_issue
    {
        query_issue_kind kind { query_issue_kind::none };
        std::string     context;
        std::size_t     step_index { 0 };        
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
// Query predicates [ for .where(pred) ]
// =====================================================================

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

// =====================================================================
// Core resolver
// =====================================================================

        using working_set = std::vector<value_location>;

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

            // Structural enumeration is allowed even on partial inspection
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
                // Tables
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
        enumerate_table_children(
            const document& doc,
            const value_location& parent,
            std::string_view /*token*/)
        {
std::cout << "    -- trace: enumerate_table_children()\n";
            std::vector<value_location> out;

            reflect::inspect_context ctx{ &doc };
            auto insp = reflect::inspect(ctx, parent.addr);

            for (auto const& child : insp.structural_children(ctx))
            {
                if (child.kind != reflect::structural_child::kind::row)
                    continue;

                auto child_addr = insp.extend_address(child);

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
std::cout << "    -- trace: enumerate_row_children(" << token << ")\n";
            std::vector<value_location> out;

            reflect::inspect_context ctx{ &doc };
            auto insp = reflect::inspect(ctx, parent.addr);

            for (auto const& child : insp.structural_children(ctx))
            {
                if (child.kind != reflect::structural_child::kind::column)
                    continue;

                if (child.name != token)
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
        enumerate_value_children(
            const document& doc,
            const value_location& parent,
            std::string_view token)
        {
std::cout << "    -- trace: enumerate_value_children(" << token << ")\n";
            std::vector<value_location> out;

            if (!parent.value_ptr)
                return out;

            if (!reflect::is_array(parent.value_ptr->type))
                return out;

            // index token must be numeric
            std::size_t index = 0;
            if (auto [p, ec] = std::from_chars(
                    token.data(),
                    token.data() + token.size(),
                    index
                );
                ec != std::errc{} || p != token.data() + token.size())
            {
                return out;
            }

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

    // --------------------------------------------------------------
    // Core resolver: Dispatch
    // --------------------------------------------------------------

        inline std::vector<value_location>
        enumerate_matching_children(
            const document& doc,
            const value_location& loc,
            std::string_view token)
        {
            switch (loc.kind)
            {
                case location_kind::category_scope:
                {   
//std::cout << "    -- trace: location_kind::category_scope\n"; 
                    return enumerate_category_children(doc, loc, token);
                }

                case location_kind::table_scope:
                {   
std::cout << "    -- trace: location_kind::table_scope\n"; 
                    return enumerate_table_children(doc, loc, token);
                }

                case location_kind::row_scope:
                {   
std::cout << "    -- trace: location_kind::row_scope\n"; 
                    return enumerate_row_children(doc, loc, token);
                }

                case location_kind::terminal_value:
                {   
std::cout << "    -- trace: location_kind::terminal_value\n"; 
                    return enumerate_value_children(doc, loc, token);
                }
            }

            return {};
        }

    // --------------------------------------------------------------
    // Core resolver loop
    // --------------------------------------------------------------

        inline working_set
        resolve_step(
            const document& doc,
            const working_set& current,
            std::string_view token)
        {
            working_set next;

            if (current.empty())
                return next;

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
            std::span<const std::string_view> segments)
        {
            working_set current;

            // Seed: root category scope
            current.push_back({
                reflect::root(),
                location_kind::category_scope,
                nullptr
            });

            for (auto seg : segments)
            {
                current = resolve_step(doc, current, seg);
                if (current.empty())
                    break;
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
// query_result (optional-with-error)
// ---------------------------------------------------------------------
// minimal replacement for std::expected
// =====================================================================

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
// Query handle
// =====================================================================

    class query_handle
    {
    public:
        explicit query_handle(const document& doc) noexcept : doc_(&doc) {}

        // --------------------------------------------------------------
        // Selectors / narrowing operations
        // --------------------------------------------------------------

        query_handle& select(std::string_view path);

        query_handle& tables();
        query_handle& table(size_t ordinal);

        query_handle& rows();
        query_handle& row(size_t ordinal);
        query_handle& row(std::string_view name);

        query_handle& where(predicate pred);

        // ------------------------------------------------------------
        // projection (shape transformation, not filtering)
        // ------------------------------------------------------------
        //
        // project() declares which columns should be extracted from
        // resolved row sets. It does NOT affect query resolution,
        // ambiguity, or cardinality.
        //
        // Intended usage:
        //   query(...).table(0).rows().where(...).project("hp", "mp")
        //
        // Until implemented, this is a no-op placeholder.
        //
        template <std::convertible_to<std::string_view>... Names>
        query_handle& project(Names&&... names)
        {
            std::array<std::string_view, sizeof...(Names)> cols {
                std::string_view(std::forward<Names>(names))...
            };
            return project_impl(cols);
        };        


        // --------------------------------------------------------------
        // Status of the query (of the result)
        // --------------------------------------------------------------

        bool empty() const noexcept { return locations_.empty(); }
        bool ambiguous() const noexcept { return locations_.size() > 1; }

        // --------------------------------------------------------------
        // Information about working set (about the result)
        // --------------------------------------------------------------

        const std::vector<value_location>& locations() const noexcept { return locations_; }
        const std::vector<query_issue>& issues() const noexcept { return issues_; }

        // --------------------------------------------------------------
        // Scalar extraction
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
    };

// =====================================================================
// IMPLEMENTATIONS
// =====================================================================

    query_handle& query_handle::select(std::string_view path)
    {
        auto segments = details::split_dot_path(path);

        locations_.clear();
        issues_.clear();

        locations_ = details::resolve_dot_path(
            *doc_,
            std::span<const std::string_view>(segments.data(), segments.size())
        );

        if (locations_.empty())
        {
            issues_.push_back({
                query_issue_kind::empty,
                std::string(path),
                segments.empty() ? 0 : segments.size() - 1
            });
        }
        
        else if (locations_.size() > 1)
        {
            issues_.push_back({
                query_issue_kind::ambiguous,
                std::string(path),
                segments.size() - 1
            });
        }

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

        if (locations_.empty())
        {
            issues_.push_back({
                query_issue_kind::empty,
                "tables()",
                0
            });
        }

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
            issues_.push_back({
                query_issue_kind::invalid_index,
                "table(" + std::to_string(ordinal) + ")",
                0
            });
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

        if (locations_.empty())
        {
            issues_.push_back({
                query_issue_kind::empty,
                "rows()",
                0
            });
        }

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
            issues_.push_back({
                query_issue_kind::invalid_index,
                "row(" + std::to_string(ordinal) + ")",
                0
            });
            return *this;
        }

        auto chosen = locations_[ordinal];
        locations_.clear();
        locations_.push_back(chosen);

        return *this;
    }

    query_handle& query_handle::row(std::string_view name)
    {
        std::vector<value_location> next;
        issues_.clear();

        // If we're still at table level, expand first
        bool has_table = false;
        for (auto const& loc : locations_)
            if (loc.kind == location_kind::table_scope)
                has_table = true;

        if (has_table)
            rows();   // expands table → rows

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

            if (row_view->name() == name)
                next.push_back(loc);
        }

        locations_ = std::move(next);

        if (locations_.empty())
        {
            issues_.push_back({
                query_issue_kind::not_found,
                std::string("row(\"") + std::string(name) + "\")",
                0
            });
        }
        else if (locations_.size() > 1)
        {
            issues_.push_back({
                query_issue_kind::ambiguous,
                std::string("row(\"") + std::string(name) + "\")",
                0
            });
        }

        return *this;
    }

    inline bool
    evaluate_predicate(
        const typed_value& cell,
        const predicate&   pred)
    {
        // Reject unresolved or contaminated values early
        if (cell.type == value_type::unresolved)
            return false;

        const typed_value& rhs = pred.rhs;

        if (rhs.type == value_type::unresolved)
            return false;

        // Types must match exactly (for now)
        if (cell.type != rhs.type)
            return false;

        switch (cell.type)
        {
            case value_type::integer:
            {
                const auto lhs = std::get<int64_t>(cell.val);
                const auto r   = std::get<int64_t>(rhs.val);

                switch (pred.op)
                {
                    case predicate_op::eq: return lhs == r;
                    case predicate_op::ne: return lhs != r;
                    case predicate_op::lt: return lhs <  r;
                    case predicate_op::le: return lhs <= r;
                    case predicate_op::gt: return lhs >  r;
                    case predicate_op::ge: return lhs >= r;
                }
                break;
            }

            case value_type::decimal:
            {
                const auto lhs = std::get<double>(cell.val);
                const auto r   = std::get<double>(rhs.val);

                switch (pred.op)
                {
                    case predicate_op::eq: return lhs == r;
                    case predicate_op::ne: return lhs != r;
                    case predicate_op::lt: return lhs <  r;
                    case predicate_op::le: return lhs <= r;
                    case predicate_op::gt: return lhs >  r;
                    case predicate_op::ge: return lhs >= r;
                }
                break;
            }

            case value_type::string:
            {
                const auto& lhs = std::get<std::string>(cell.val);
                const auto& r   = std::get<std::string>(rhs.val);

                switch (pred.op)
                {
                    case predicate_op::eq: return lhs == r;
                    case predicate_op::ne: return lhs != r;
                    case predicate_op::lt: return lhs <  r;
                    case predicate_op::le: return lhs <= r;
                    case predicate_op::gt: return lhs >  r;
                    case predicate_op::ge: return lhs >= r;
                }
                break;
            }

            case value_type::boolean:
            {
                const auto lhs = std::get<bool>(cell.val);
                const auto r   = std::get<bool>(rhs.val);

                switch (pred.op)
                {
                    case predicate_op::eq: return lhs == r;
                    case predicate_op::ne: return lhs != r;
                    default:
                        return false; // ordering on bool is nonsense
                }
            }

            // Explicitly unsupported for predicates (for now)
            case value_type::string_array:
            case value_type::int_array:
            case value_type::float_array:
            case value_type::date:
            default:
                return false;
        }

        return false;
    }

    query_handle& query_handle::where(predicate pred)
    {
        std::vector<value_location> next;
        issues_.clear();

    // Shorthand: where() on table == rows().where()
    // -----------------------------------------------
        // Expand any table-scope locations to rows
        bool has_tables = false;
        for (auto const& loc : locations_)
            if (loc.kind == location_kind::table_scope)
                has_tables = true;
        if (has_tables)
            rows();  // Expands all tables to rows

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
                issues_.push_back({
                    query_issue_kind::invalid_index,
                    "where()",
                    0
                });
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
            {
                next.push_back(loc);
            }
        }

        locations_ = std::move(next);

        if (locations_.empty())
        {
            issues_.push_back({
                query_issue_kind::not_found,
                "where()",
                0
            });
        }

        return *this;
    }

    query_handle& query_handle::project_impl(std::span<const std::string_view> column_names)
    {
        // Placeholder.
        // Future implementation will:
        //  - Validate column existence
        //  - Record projection metadata
        //  - Influence plural extraction helpers
        //
        // Deliberately does nothing for now.
        return *this;
    }

    template<value_type vt>
    typed_value const *
    query_handle::common_extraction_checks(query_issue_kind* err) const noexcept
    {
        *err = query_issue_kind::none;

        if (locations_.empty())
        {
            *err = query_issue_kind::not_found;
            issues_.push_back({
                query_issue_kind::not_found,
                "<extraction>",
                0
            });                
            return nullptr;
        }

        if (locations_.size() > 1)
        {
            *err = query_issue_kind::ambiguous;
            issues_.push_back({
                query_issue_kind::ambiguous,
                "<extraction>",
                0
            });                
            return nullptr;
        }

        auto* v = locations_.front().value_ptr;

        if (!v || v->type != vt)
        {
            *err = query_issue_kind::type_mismatch;
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
                *err = query_issue_kind::no_literal;
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
            else *err = query_issue_kind::no_literal;
        }
        else
            *err = query_issue_kind::not_a_value;

        return false;
    }

    template<value_type Vt, typename T>
    query_result<T>
    query_handle::scalar_extract(bool convert) const noexcept
    {
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

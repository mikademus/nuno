// arf_query.hpp - A Readable Format (Arf!) - Query Interface
// Version 0.3.0
//
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_QUERY_HPP
#define ARF_QUERY_HPP

#pragma once

#include "arf_document.hpp"
#include "arf_reflect.hpp"

#include <string_view>
#include <vector>
#include <optional>
#include <variant>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace arf
{
    struct document;
    class query_handle;
    template<value_type> struct result_entry;

// ---------------------------------------------------------------------
// Primary entry points and convenience getters
// ---------------------------------------------------------------------

    query_handle query(const document& doc) noexcept;
    query_handle query(const document& doc, std::string_view dot_path) noexcept;

// Quick-access friendly getters:

    result_entry<value_type::string>
    get_string(const document& doc, std::string_view path) noexcept;

    result_entry<value_type::integer>
    get_integer(const document& doc, std::string_view path) noexcept;

    result_entry<value_type::decimal>
    get_real(const document& doc, std::string_view path) noexcept;


// ---------------------------------------------------------------------
// Query diagnostics
// ---------------------------------------------------------------------

    enum class query_issue_kind
    {
        none,
        not_found,
        ambiguous,
        type_mismatch,
        invalid_predicate,
        structural_violation,
        no_literal,
    };

    struct query_issue
    {
        query_issue_kind kind { query_issue_kind::none };
        std::string_view message {};
    };

// ---------------------------------------------------------------------
// Predicate model (value-side only; execution is contextual)
// ---------------------------------------------------------------------

    struct predicate
    {
        enum class kind
        {
            eq,
            ne,
            lt,
            le,
            gt,
            ge
        };

        std::string_view column {};
        kind             op     {};
        std::string_view value  {};
    };

    inline predicate is(std::string_view value) noexcept
    {
        return { {}, predicate::kind::eq, value };
    }

    inline predicate ne(std::string_view value) noexcept
    {
        return { {}, predicate::kind::ne, value };
    }

    inline predicate lt(std::string_view value) noexcept
    {
        return { {}, predicate::kind::lt, value };
    }

    inline predicate le(std::string_view value) noexcept
    {
        return { {}, predicate::kind::le, value };
    }

    inline predicate gt(std::string_view value) noexcept
    {
        return { {}, predicate::kind::gt, value };
    }

    inline predicate ge(std::string_view value) noexcept
    {
        return { {}, predicate::kind::ge, value };
    }

// =====================================================================
// Query subsystem
// =====================================================================

//namespace arf::query
//{
    namespace details
    {
        template <arf::value_type T> struct value_type_to_view_type;
        template<> struct value_type_to_view_type<arf::value_type::boolean>       { typedef bool type; };
        template<> struct value_type_to_view_type<arf::value_type::integer>       { typedef int64_t type; };
        template<> struct value_type_to_view_type<arf::value_type::decimal>       { typedef double type; };
        template<> struct value_type_to_view_type<arf::value_type::string>        { typedef std::string_view type; };
        template<> struct value_type_to_view_type<arf::value_type::int_array>     { typedef std::span<std::int64_t> type; };
        template<> struct value_type_to_view_type<arf::value_type::float_array>   { typedef std::span<double> type; };
        template<> struct value_type_to_view_type<arf::value_type::string_array>  { typedef std::span<std::string> type; };

        template <arf::value_type T> struct value_type_to_stored_type;
        template<> struct value_type_to_stored_type<arf::value_type::boolean>       { typedef bool type; };
        template<> struct value_type_to_stored_type<arf::value_type::integer>       { typedef int64_t type; };
        template<> struct value_type_to_stored_type<arf::value_type::decimal>       { typedef double type; };
        template<> struct value_type_to_stored_type<arf::value_type::string>        { typedef std::string type; };
        template<> struct value_type_to_stored_type<arf::value_type::int_array>     { typedef std::vector<std::int64_t> type; };
        template<> struct value_type_to_stored_type<arf::value_type::float_array>   { typedef std::vector<double> type; };
        template<> struct value_type_to_stored_type<arf::value_type::string_array>  { typedef std::vector<std::string> type; };

        std::vector<std::string_view> split_dot_path(std::string_view dot_path)
        {
            std::vector<std::string_view> ret;

            std::size_t pos = 0;

            while (pos < dot_path.size())
            {
                const auto next = dot_path.find('.', pos);
                const auto token =
                    (next == std::string_view::npos)
                        ? dot_path.substr(pos)
                        : dot_path.substr(pos, next - pos);
                ret.push_back(token);

                if (next == std::string_view::npos)
                    break;

                pos = next + 1;
            }

            return ret;
        }
    }

    template<arf::value_type T>
    struct result_entry
    {
        typedef typename details::value_type_to_view_type<T>::type type;

        std::optional<type>             value;
        std::vector<query_issue_kind>   issues;        
    };

    struct result_list
    {
        std::vector<const typed_value *>                    values;
        std::vector<std::pair<int, query_issue_kind>>       issues; // issues are indexed to values

        bool homogeneous() const noexcept;
        bool heterogeneous() const noexcept;
    };

/* legal query narrowing rules
     A step of type B can only come from states {A}

     B      <-  {A}
     ------ -- ----------------------
     index      {value}
     value      {key, column, row}
     key        {category}
     category   {category}
     table      {category}
     row        {table, column}
     column     {table, row}
*/    

    struct query_node 
    { 
        enum struct semantic_kind
        {
            root,
            category,
            table,
            row,
            column,
            key,
            value,
        };

        reflect::address addr; 
        semantic_kind kind;
    };

    inline std::string_view to_string( query_node::semantic_kind kind)
    {
        switch (kind)
        {
            using enum query_node::semantic_kind;
            case root: return "root";
            case category: return "category";
            case table: return "table";
            case row: return "row";
            case column: return "column";
            case key: return "key";
            case value: return "value";
            default: return "unknown";
        }
    }

    namespace details
    {
        using working_set = std::vector<query_node>;
        using sk = query_node::semantic_kind;        

        static const std::unordered_map<sk, std::unordered_set<sk>> legal_transitions =
        {
            { sk::root,     { sk::category, sk::key, sk::table } },
            { sk::category, { sk::category, sk::key, sk::table } },
            { sk::table,    { sk::row, sk::column } },
            { sk::row,      { sk::column } },
            { sk::column,   { sk::row, sk::value } },
            { sk::key,      { sk::value } },
            { sk::value,    { /* specifiers only */ } },
        };

        inline bool is_legal_transition(sk from, sk to)
        {
            auto it = legal_transitions.find(from);
            return it != legal_transitions.end() && it->second.contains(to);
        }

        void enumerate_named_children( const document& doc, const query_node& node, std::string_view token, working_set& out );
        void enumerate_table_children(const document&, const query_node&, std::string_view token, working_set& out );
        void enumerate_row_children(const document&, const query_node&, std::string_view token, working_set& out );
        void enumerate_column_children(const document&, const query_node&, std::string_view token, working_set& out );
        void enumerate_key_value(const document&, const query_node&, std::string_view token, working_set& out );
        void enumerate_value_specifier(const document&, const query_node&, std::string_view token, working_set& out );

        working_set resolve_step( const document& doc, const working_set& in, std::string_view token);
        working_set resolve_dot_path( const document& doc, std::span<const std::string_view> segments);
    } // ns details


// ---------------------------------------------------------------------
// query_handle
// ---------------------------------------------------------------------
// Lazy query handle.
// Owns a working set of reflection addresses.
// Selectors refine immediately; materialisation is view-based.
// ---------------------------------------------------------------------

    class query_handle
    {
    public:
        using working_set = details::working_set;
        using semantic_kind = query_node::semantic_kind;


        explicit query_handle(const document& doc) noexcept
            : doc_(&doc)
        {
            // initial working set is the document root
            addrs_.push_back({reflect::root(), semantic_kind::root});
        }

        // -----------------------------------------------------------------
        // Selection
        // -----------------------------------------------------------------

        // Dot-path selector (split internally)
        query_handle& select(std::string_view dot_path)
        {
            auto segments = details::split_dot_path(dot_path);

            addrs_ = details::resolve_dot_path(
                *doc_,
                std::span<const std::string_view>(segments.data(), segments.size())
            );

            if (addrs_.empty())
            {
                issues_.push_back({
                    query_issue_kind::not_found,
                    "dot-path resolution yielded no values"
                });
            }

            return *this;
        }

        // -----------------------------------------------------------------
        // Introspection
        // -----------------------------------------------------------------

        bool empty() const noexcept
        {
            return addrs_.empty();
        }

        bool ambiguous() const noexcept
        {
            return addrs_.size() > 1;
        }

        const std::vector<query_issue>& issues() const noexcept
        {
            return issues_;
        }

        const working_set& addresses() const noexcept
        {
            return addrs_;
        }

        // -----------------------------------------------------------------
        // Materialisation
        // -----------------------------------------------------------------

        result_list values() const
        {
            result_list out;
            // std::vector<const typed_value *> out;

            reflect::inspect_context ctx {doc_};
            for (const auto& addr : addrs_)
            {
                if (auto v = reflect::inspect(ctx, addr.addr); v.ok())
                    out.values.push_back(v.value);
            }

            return out;
        }

        std::vector<reflect::inspected> inspected() const
        {
            std::vector<reflect::inspected> out;

            reflect::inspect_context ctx {doc_};
            for (const auto& addr : addrs_)
                out.push_back(reflect::inspect(ctx, addr.addr));

            return out;
        }

        // -----------------------------------------------------------------
        // Singular extraction
        // -----------------------------------------------------------------

        result_entry<value_type::integer>
        as_integer() const noexcept { return extract<value_type::integer>(); }

        result_entry<value_type::decimal>
        as_real() const noexcept { return extract<value_type::decimal>(); }

        result_entry<value_type::boolean>
        as_bool() const noexcept { return extract<value_type::boolean>(); }

        result_entry<value_type::string> 
        as_string() const noexcept;

        // -----------------------------------------------------------------
        // Plural extraction
        // -----------------------------------------------------------------

        //result_list booleans() const noexcept;
        //result_list integers() const noexcept;
        //result_list reals() const noexcept;
        //result_list strings() const noexcept;        

    private:
        const document*               doc_   { nullptr };
        working_set                   addrs_ {};
        std::vector<query_issue>      issues_;

        template<value_type T>
        result_entry<T> singular_extraction_guard() const noexcept;

        template<value_type VType>
        result_entry<VType> 
        extract( reflect::inspected * insp = nullptr ) const noexcept;
    };

// ---------------------------------------------------------------------
// Ergonomic getters (human API)
// ---------------------------------------------------------------------

    inline result_entry<value_type::string>
    get_string(const document& doc, std::string_view path) noexcept
    {
        return query(doc, path).as_string();
    }

    inline result_entry<value_type::integer>
    get_integer(const document& doc, std::string_view path) noexcept
    {
        return query(doc, path).as_integer();
    }

    inline result_entry<value_type::decimal>
    get_real(const document& doc, std::string_view path) noexcept
    {
        return query(doc, path).as_real();
    }

// ---------------------------------------------------------------------
// IMPLEMENTATION
// ---------------------------------------------------------------------

    template<value_type T>
    inline result_entry<T> query_handle::singular_extraction_guard() const noexcept
    {
        result_entry<T> res;

        if (addrs_.empty())
            res.issues.push_back(query_issue_kind::not_found);

        else if (addrs_.size() != 1)
            res.issues.push_back(query_issue_kind::ambiguous);

        return res;
    }

    template<value_type VType>
    inline result_entry<VType> 
    query_handle::extract( reflect::inspected * insp ) const noexcept
    {
        using SType = typename details::value_type_to_stored_type<VType>::type;

        result_entry<VType> ret 
            = singular_extraction_guard<VType>();

        if (!ret.issues.empty()) return ret;

        reflect::inspect_context ctx {doc_};
        auto res = reflect::inspect(ctx, addrs_.front().addr);

        if (res.value->type == VType)
            ret.value = std::get<SType>(res.value->val); 
        else
            ret.issues.push_back(query_issue_kind::type_mismatch);

        if (insp != nullptr) 
            *insp = std::move(res);

        return ret;
    }

    inline result_entry<value_type::string> 
    query_handle::as_string() const noexcept
    {
        reflect::inspected insp;

        auto ret = extract<value_type::string>(&insp);

        if (!ret.value.has_value() && insp.value != nullptr)
        {
            if (insp.value->source_literal.has_value())
                ret.value = {*insp.value->source_literal};
            else
                ret.issues.push_back(query_issue_kind::no_literal);
        }
        return ret;
    }    

    namespace details
    {
        inline void enumerate_named_children(
            const document& doc,
            const query_node& node,
            std::string_view token,
            working_set& out)
        {
std::cout << "      enumerate_named_children()\n";

            reflect::inspect_context ctx{ &doc };
            auto insp = reflect::inspect(ctx, node.addr);
/*
            if (!insp.ok())
            {
std::cout << "         enumerate_named: inspection.ok() == false; return {}\n";
std::cout << "insp.fully_inspected() = " << insp.fully_inspected() << std::endl;
std::cout << "insp.first_error_step() = " << insp.first_error_step() << std::endl;
std::cout << "insp.value (ptr) = " << insp.value << std::endl;
if (insp.value)
{
    std::cout << "insp.value->type = " << detail::type_to_string(insp.value->type) << std::endl;
    std::cout << "insp.steps_insp = " << insp.steps_inspected << std::endl;
}
                return;
            }
*/
            for (const auto& child : insp.structural_children(ctx))
            {
std::cout << "             child: " << child.name << std::endl;

                if (child.name != token)
                    continue;

                // auto child_addr = insp.extend_address(child);
                auto child_addr = insp.extend_address(child);
                auto child_insp = reflect::inspect(ctx, child_addr);

                using st_ch = reflect::structural_child;

                if (child_insp.value != nullptr)
                    out.push_back({ child_addr, sk::key });

                else if (child.kind == st_ch::kind::table)
                    out.push_back({ child_addr, sk::table });

                else
                    out.push_back({ child_addr, sk::category });
            }
        }

        inline void enumerate_key_value(
            const document& doc,
            const query_node& node,
            std::string_view /*token*/,
            working_set& out)
        {
std::cout << "enumerate_key_value\n";
            reflect::inspect_context ctx { &doc };
            auto insp = reflect::inspect(ctx, node.addr);

            if (!insp.ok())
                return;

            if (insp.value == nullptr)
                return;

            // Key has exactly one value; token is ignored here
            out.push_back({
                node.addr,
                sk::value
            });
        }

        inline working_set resolve_step(
            const document& doc,
            const working_set& in,
            std::string_view token)
        {
            working_set out;
            working_set candidates;

std::cout << "resolving step " << token << std::endl;

            for (const query_node& node : in)
            {
                candidates.clear();

                switch (node.kind)
                {
                    case sk::root:
                    case sk::category:
std::cout << "  step kind is category\n";
                        enumerate_named_children(doc, node, token, candidates);
                        break;

                    case sk::table:
std::cout << "  step kind is table\n";
                        //enumerate_table_children(doc, node, token, candidates);
                        break;

                    case sk::row:
std::cout << "  step kind is row\n";
                        // enumerate_row_children(doc, node, token, candidates);
                        break;

                    case sk::column:
std::cout << "  step kind is column\n";
                        // enumerate_column_children(doc, node, token, candidates);
                        break;

                    case sk::key:
std::cout << "  step kind is key\n";
                        // enumerate_key_value(doc, node, token, candidates);
                        break;

                    case sk::value:
std::cout << "  step kind is value\n";
                        // enumerate_value_specifier(doc, node, token, candidates);
                        break;
                }

                //auto const& allowed = legal_transitions.at(node.kind);

                for (auto& cand : candidates)
                {
                    //if (allowed.contains(cand.kind))
                    {
                        std::cout << "Adding candidate for \"" << token << "\" of type " << to_string(cand.kind) << std::endl;
                        out.push_back(std::move(cand));
                    }
                }
            }

            return out;
        }


        inline working_set resolve_dot_path(
            const document& doc,
            std::span<const std::string_view> segments)
        {
std::cout << "resolve_dot_path(";
for (auto seg : segments)
    std::cout << seg << " -> ";
std::cout << ")\n";

            working_set current;

            // seed with root
            current.push_back({reflect::root(), sk::root});

            // for (std::string_view segment : segments)
            for (size_t seg_idx = 0; seg_idx < segments.size(); ++seg_idx)
            {
                auto & segment = segments[seg_idx];

std::cout << "  resolve_dot_path 2: segment = " << segment << std::endl;

                current = resolve_step(doc, current, segment);

                for (auto &c : current)
                    std::cout << "    - cand: " << to_string(c.kind) << std::endl;

                if (current.empty())
                {
std::cout << "  resolve_dot_path 3: current is empty, breaking at " << segment 
          << " (idx " << seg_idx << ", num steps = " << segments.size() << ")" << std::endl;
                    // break;
                    return {};
                }
            }

            // --- implicit key → value promotion ---
            working_set promoted;

            for (const auto& node : current)
            {
                if (node.kind == sk::key)
                {
                    reflect::inspect_context ctx { &doc };
                    auto insp = reflect::inspect(ctx, node.addr);

                    if (insp.ok() && insp.value != nullptr)
                    {
                        promoted.push_back({ node.addr, sk::value });
                    }
                }
                else
                {
                    promoted.push_back(node);
                }
            }

            return promoted;
            // return current;
        }        

    } // ns details

    inline query_handle query(const document& doc) noexcept
    {
        return query_handle { doc };
    }

    inline query_handle query(const document& doc, std::string_view path) noexcept
    {
        query_handle q { doc };
        q.select(path);
        return q;
    }    
} // namespace arf

#endif // ARF_QUERY_HPP

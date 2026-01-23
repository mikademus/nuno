#ifndef ARF_QUERY_HPP
#define ARF_QUERY_HPP

#pragma once

#include "arf_document.hpp"
#include "arf_reflect.hpp"

#include <string_view>
#include <vector>
#include <optional>
#include <span>

namespace arf
{
// =====================================================================
// Query issues
// =====================================================================

    enum class query_issue_kind
    {
        none,
        not_found,
        ambiguous,
        type_mismatch,
        no_literal,
    };

    struct query_issue
    {
        query_issue_kind kind { query_issue_kind::none };
        std::string     context;        // e.g. "world.races.orcs"
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

    namespace details
    {
        using working_set = std::vector<value_location>;

        // --------------------------------------------------------------
        // Enumerators
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
                    if (auto key_view = std::get_if<document::key_view>(&child_insp.item))
                    {
                        out.push_back({
                            child_addr,
                            location_kind::terminal_value,
                            &key_view->value()
                        });
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
            const document&,
            const value_location&,
            std::string_view)
        {
            // Placeholder: rows / columns later
            return {};
        }

        inline std::vector<value_location>
        enumerate_row_children(
            const document&,
            const value_location&,
            std::string_view)
        {
            return {};
        }

        inline std::vector<value_location>
        enumerate_value_children(
            const document&,
            const value_location&,
            std::string_view)
        {
            // Arrays / predicates later
            return {};
        }

        // --------------------------------------------------------------
        // Dispatch
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
        // Resolver loop
        // --------------------------------------------------------------

        inline working_set
        resolve_step(
            const document& doc,
            const working_set& current,
            std::string_view token)
        {
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
    }

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
        explicit query_handle(const document& doc) noexcept
            : doc_(&doc)
        {}

        query_handle& select(std::string_view path)
        {
            auto segments = split_dot_path(path);

            locations_.clear();
            issues_.clear();

            locations_ = details::resolve_dot_path(
                *doc_,
                std::span<const std::string_view>(segments.data(), segments.size())
            );

            if (locations_.empty())
            {
                issues_.push_back({
                    query_issue_kind::not_found,
                    std::string(path),
                    segments.empty() ? 0 : segments.size() - 1
                });
            }

            return *this;
        }

        bool empty() const noexcept
        {
            return locations_.empty();
        }

        bool ambiguous() const noexcept
        {
            return locations_.size() > 1;
        }

        const std::vector<value_location>& locations() const noexcept
        {
            return locations_;
        }

        const std::vector<query_issue>& issues() const noexcept
        {
            return issues_;
        }

        // --------------------------------------------------------------
        // Scalar extraction
        // --------------------------------------------------------------

        query_result<int64_t> as_integer() const noexcept
        {
            query_issue_kind err;
            if (auto v = common_extraction_checks<value_type::integer>(&err); v != nullptr)
                return std::get<int64_t>(v->val);
            return {err};
        }

        query_result<double> as_real() const noexcept
        {
            query_issue_kind err;
            if (auto v = common_extraction_checks<value_type::decimal>(&err); v != nullptr)
                return std::get<double>(v->val);
            return {err};
        }

        query_result<bool> as_bool() const noexcept
        {
            query_issue_kind err;
            if (auto v = common_extraction_checks<value_type::boolean>(&err); v != nullptr)
                return std::get<bool>(v->val);
            return {err};
        }

        query_result<std::string> as_string() const noexcept
        {
            query_issue_kind err;
            if (auto v = common_extraction_checks<value_type::string>(&err); v != nullptr)
                return std::get<std::string>(v->val);
            return {err};
        }

        // TODO: arrays

    private:
        const document*                   doc_ { nullptr };
        std::vector<value_location>       locations_;
        std::vector<query_issue>          issues_;

        template<value_type vt>
        typed_value const *
        common_extraction_checks(query_issue_kind* err) const noexcept
        {
            *err = query_issue_kind::none;

            if (locations_.empty())
            {
                *err = query_issue_kind::not_found;
                return nullptr;
            }

            if (locations_.size() > 1)
            {
                *err = query_issue_kind::ambiguous;
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

    };

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


} // namespace arf

#endif // ARF_QUERY_HPP

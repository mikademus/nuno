// arf_materialise.hpp - A Readable Format (Arf!) - Semantic analysis and materialiser
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

// Mandatory memory items:
// * Save invalid semantic nodes. They must be accessible for reflection and tooling.
// * Columns with invalid types must be marked as such. They will collapse to strings and this overwrites the type. That the type was invalid must be preserved.
// * Cells with invalid type resolutions must be marked as such.
// * Currently rows of invalid arity are rejected. This data must be preserved.

#ifndef ARF_MATERIALISE_HPP
#define ARF_MATERIALISE_HPP

#include "arf_core.hpp"
#include "arf_parser.hpp"
#include "arf_document.hpp"
#include <unordered_map>


namespace arf
{
    struct materialiser_options
    {
        size_t max_category_depth = 64;
    };    

    enum class semantic_error_kind
    {
        unknown_type,
        type_mismatch,
        declared_type_mismatch,
        invalid_literal,
        invalid_declared_type,
        column_arity_mismatch,
        duplicate_key,
        invalid_category_close,
        depth_exceeded,
    // warnings
        date_unsupported,
    };

    using material_context = context<document, semantic_error_kind>;

    // Facade
    material_context materialise( const parse_context& ctx, materialiser_options opts = {});    

//========================================================================
// materialiser
//========================================================================

    struct materialiser
    {
        materialiser(const parse_context& ctx,
                     materialiser_options opts);

        material_context run();

    private:
        // Immutable input
        const parse_context& ctx_;
        const cst_document&  cst_;

        // Output
        material_context     out_;
        document&            doc_;

        // State
        materialiser_options     opts_;
        std::vector<category_id> stack_;
        std::optional<table_id>  active_table_;
        size_t                   key_index_     {0};


        // Helpers
        void handle_category_open(const parse_event& ev);
        void handle_category_close(const parse_event& ev);
        void handle_table_header(const parse_event& ev);
        void handle_table_row(const parse_event& ev);        
        void handle_key(parse_event const& ev);        
    };


//========================================================================
// Implementation
//========================================================================

namespace 
{
    inline bool value_matches_type(value_type v, value_type expected)
    {
        if (expected == value_type::unresolved)
            return true;

        return v == expected;
    } 

    std::optional<bool> is_bool(std::string_view s)
    {
        static std::unordered_map<std::string_view, bool> booleans = 
        {
            {"true", true}, {"false", false},
            {"yes",  true}, {"no",    false},
            {"on",   true}, {"off",   false},
        };

        if (auto it = booleans.find(s); it != booleans.end())
            return it->second;
        return std::nullopt;
    };

    inline std::optional<value_type> parse_declared_type(std::string_view s, material_context & out)
    {
        static std::unordered_map<std::string_view, value_type> valid_types = 
        {
            {"str", value_type::string},
            {"int", value_type::integer},
            {"float", value_type::decimal},
            {"bool", value_type::boolean},
            {"date", value_type::date},
            {"str[]", value_type::string_array},
            {"int[]", value_type::int_array},
            {"float[]", value_type::float_array},
        };

        if (auto it = valid_types.find(s); it != valid_types.end())
        {
            if (it->second == value_type::date)             
            {
                out.errors.push_back({
                    semantic_error_kind::date_unsupported,
                    {0},
                    "the 'date' data type is currently not validated; treating as string"
                });                
                return value_type::string;
            }

            return it->second;
        }
        return std::nullopt;
    }
    
    inline std::optional<typed_value> infer_scalar_value(std::string_view s)
    {
        typed_value tv;
        tv.source_literal = std::string(s);
        tv.origin = value_locus::key_value;

        if (auto b = is_bool(s); b.has_value())
        {
            tv.type = value_type::boolean;
            tv.val  = b.value();
            return tv;
        }

        char* end = nullptr;
        long i = std::strtol(s.data(), &end, 10);
        if (end == s.data() + s.size())
        {
            tv.type = value_type::integer;
            tv.val  = static_cast<int64_t>(i);
            return tv;
        }

        char* fend = nullptr;
        double d = std::strtod(s.data(), &fend);
        if (fend == s.data() + s.size())
        {
            tv.type = value_type::decimal;
            tv.val  = d;
            return tv;
        }

        tv.type = value_type::string;
        tv.val  = std::string(s);
        return tv;
    }    

    inline std::optional<value> try_convert(std::string_view s, value_type t)
    {
        switch (t)
        {
            case value_type::string:
                return std::string(s);

            case value_type::integer:
            {
                char* end = nullptr;
                long v = std::strtol(s.data(), &end, 10);
                if (end != s.data() + s.size())
                    return std::nullopt;
                return static_cast<int64_t>(v);
            }

            case value_type::decimal:
            {
                char* end = nullptr;
                double v = std::strtod(s.data(), &end);
                if (end != s.data() + s.size())
                    return std::nullopt;
                return v;
            }

            case value_type::boolean:
                if (auto b = is_bool(s); b.has_value())
                    return b.value();
                return std::nullopt;

            case value_type::string_array:
            case value_type::int_array:
            case value_type::float_array:
                // Arrays are handled elsewhere; cell literal stays intact
                return std::nullopt;

            default:
                return std::nullopt;
        }
    }

    inline typed_value coerce_cell(
        std::string_view literal,
        value_type column_type,
        source_location loc,
        material_context & ctx
    )
    {
        typed_value tv;
        tv.origin = value_locus::table_cell;
        tv.source_literal = std::string(literal);

        // Unresolved or string → accept verbatim
        if (column_type == value_type::unresolved ||
            column_type == value_type::string)
        {
            tv.type = value_type::string;
            tv.val  = std::string(literal);
            return tv;
        }

        // Attempt strict conversion
        auto converted = try_convert(literal, column_type); // existing utility
        if (!converted)
        {
            ctx.errors.push_back({
                semantic_error_kind::type_mismatch,
                loc,
                "cell value does not match column type"
            });

            // Degrade to string
            tv.type     = value_type::string;
            tv.val      = std::string(literal);
            tv.semantic = semantic_state::invalid;
            return tv;
        }

        tv.type = column_type;
        tv.val  = *converted;
        return tv;
    }

    inline typed_value coerce_key_value(
        std::string_view literal,
        value_type target,
        source_location loc,
        material_context& out
    )
    {
        auto inferred = infer_scalar_value(literal);

        // Failed inference
        if (!inferred)
        {
            out.errors.push_back({
                semantic_error_kind::invalid_literal,
                loc,
                "invalid key literal"
            });

            return {
                std::string(literal),
                value_type::string,
                type_ascription::tacit,
                value_locus::key_value
            };
        }

        auto tv = *inferred;

        // No declared type or exact match
        if (target == value_type::unresolved || tv.type == target)
            return tv;

        // Try coercion (value-only)
        if (auto v = try_convert(literal, target))
        {
            return {
                std::move(*v),
                target,
                type_ascription::declared,
                value_locus::key_value
            };
        }

        // Declared vs actual mismatch
        out.errors.push_back({
            semantic_error_kind::declared_type_mismatch,
            loc,
            "key value does not match declared type"
        });

        // Collapse to string, preserve data
        return {
            std::string(literal),
            value_type::string,
            type_ascription::tacit,
            value_locus::key_value
        };
    }

} // anon ns

    inline materialiser::materialiser(const parse_context& ctx,
                                      materialiser_options opts)
        : ctx_(ctx)
        , cst_(ctx.document)
        , out_{}
        , doc_(out_.document)
        , opts_(opts)
        , active_table_(std::nullopt)
    {
        category_id root = doc_.create_root();
        stack_.push_back(root);
    }

    inline material_context materialiser::run()
    {
        for (const auto& ev : cst_.events)
        {
            switch (ev.kind)
            {
                case parse_event_kind::category_open:
                    handle_category_open(ev);
                    break;

                case parse_event_kind::category_close:
                    handle_category_close(ev);
                    break;

                case parse_event_kind::table_header:
                    handle_table_header(ev);
                    break;

                case parse_event_kind::table_row:
                    handle_table_row(ev);
                    break;

                case parse_event_kind::key_value:
                    handle_key(ev);
                    break;

                default:
                    break;
            }
        }

        return std::move(out_);
    }

    inline void materialiser::handle_category_open(const parse_event& ev)
    {
        if (stack_.size() >= opts_.max_category_depth)
        {
            out_.errors.push_back({
                semantic_error_kind::depth_exceeded,
                ev.loc,
                "maximum category depth exceeded"
            });
            return;
        }

        active_table_.reset();        
        auto cid = std::get<category_id>(ev.target);
        const auto& cst_cat = cst_.categories[cid.val];

        category_id parent = stack_.back();

        doc_.create_category(cid, cst_cat.name, parent);
        stack_.push_back(cid);
    }

    inline void materialiser::handle_category_close(const parse_event& ev)
    {
        // Named close: unresolved name
        if (std::holds_alternative<unresolved_name>(ev.target))
        {
            auto name = std::get<unresolved_name>(ev.target);

            if (name.empty())
            {
                out_.errors.push_back({
                    semantic_error_kind::invalid_category_close,
                    ev.loc,
                    "empty category name in close"
                });
                return;
            }

            auto it = std::find_if(
                stack_.rbegin(),
                stack_.rend(),
                [&](category_id cid)
                {
                    return doc_.categories_[cid.val].name == name;
                }
            );

            if (it == stack_.rend() || (*it).val == stack_.front().val)
            {
                out_.errors.push_back({
                    semantic_error_kind::invalid_category_close,
                    ev.loc,
                    "attempt to close category that is not open"
                });
                return;
            }

            while (stack_.back() != *it)
                stack_.pop_back();

            stack_.pop_back();
            active_table_.reset();
            return;
        }

        // Implicit close: category_id
        if (std::holds_alternative<category_id>(ev.target))
        {
            if (stack_.size() <= 1)
            {
                out_.errors.push_back({
                    semantic_error_kind::invalid_category_close,
                    ev.loc,
                    "attempt to close root category"
                });
                return;
            }

            auto closing = std::get<category_id>(ev.target);

            if (stack_.back() != closing)
            {
                out_.errors.push_back({
                    semantic_error_kind::invalid_category_close,
                    ev.loc,
                    "category close does not match open scope"
                });
                return;
            }

            stack_.pop_back();
            active_table_.reset();
        }
    }

    inline void materialiser::handle_table_header(const parse_event& ev)
    {
        auto tid = std::get<table_id>(ev.target);
        const auto& cst_tbl = cst_.tables[tid.val];

        document::table_node tbl;
        tbl.id      = tid;
        tbl.owner   = stack_.back();
        tbl.columns = cst_tbl.columns;
        tbl.rows.clear();

        // Resolve declared column types BEFORE moving
        for (auto& col : tbl.columns)
        {
            if (col.type_source == type_ascription::declared)
            {
                auto const & s = col.declared_type.value();
                auto vt = parse_declared_type(s, out_);
                if (!vt)
                {
                    out_.errors.push_back({
                        semantic_error_kind::invalid_declared_type,
                        ev.loc,
                        "unknown declared column type"
                    });

                    col.type = value_type::string; // collapse
                    col.semantic = semantic_state::invalid;
                }
                else
                {
                    col.type = *vt;
                    if (s == "date")
                        col.semantic = semantic_state::invalid;
                }
            }
            else
            {
                col.type = value_type::unresolved;
            }
        }

        // Store the table
        doc_.tables_.push_back(std::move(tbl));

        // Attach table to owning category
        auto cat_id = stack_.back();
        doc_.categories_[cat_id.val].tables.push_back(tid);

        active_table_ = tid;
    }


    inline void materialiser::handle_table_row(const parse_event& ev)
    {
        if (!active_table_)
            return; // syntactically valid, semantically inert

        auto rid = std::get<table_row_id>(ev.target);
        const auto& cst_row = cst_.rows[rid.val];

        auto& tbl = doc_.tables_[active_table_->val];

        if (cst_row.cells.size() != tbl.columns.size())
        {
            out_.errors.push_back({
                semantic_error_kind::column_arity_mismatch,
                ev.loc,
                "table row arity does not match header"
            });
            return;
        }

        document::row_node row;
        row.id    = rid;
        row.table = tbl.id;
        row.owner = stack_.back();
        row.cells.reserve(tbl.columns.size());

        for (size_t i = 0; i < tbl.columns.size(); ++i)
        {
            auto& col = tbl.columns[i];

            std::string_view literal =
                (i < cst_row.cells.size())
                    ? std::string_view(cst_row.cells[i].source_literal.value())
                    : std::string_view{};

            auto tv = coerce_cell(literal, col.type, ev.loc, out_);
            row.cells.push_back(std::move(tv));
        }

        doc_.rows_.push_back(std::move(row));
        tbl.rows.push_back(rid);
    }

    inline void materialiser::handle_key(parse_event const&)
    {
        const cst_key& cst = cst_.keys.at(key_index_++);

        document::key_node k;
        k.name        = cst.name;
        k.owner       = cst.owner;
        k.type_source = cst.declared_type
                            ? type_ascription::declared
                            : type_ascription::tacit;

        value_type target = value_type::unresolved;

        // 1. Declared type (if any)
        if (cst.declared_type)
        {
            auto t = parse_declared_type(*cst.declared_type, out_);
            if (!t)
            {
                out_.errors.push_back({
                    semantic_error_kind::invalid_declared_type,
                    cst.loc,
                    "unknown declared key type"
                });

                // Collapse declared type
                target        = value_type::string;
                k.type_source = type_ascription::tacit;
                k.semantic    = semantic_state::invalid;
            }
            else
            {
                target = *t;
            }
        }

        // 2. Coerce value (never drops the key)
        auto tv = coerce_key_value(
            cst.literal,
            target,
            cst.loc,
            out_
        );

        if (tv.semantic == semantic_state::invalid)
            k.semantic = semantic_state::invalid;

        // 3. Finalise type
        k.type  = tv.type;
        k.value = std::move(tv);

        key_id id{ doc_.keys_.size() };
        doc_.keys_.push_back(std::move(k));
        doc_.categories_[cst.owner.val].keys.push_back(id);
    }

    inline material_context materialise(const parse_context& ctx, materialiser_options opts)
    {
        materialiser m(ctx, opts);
        return m.run();
    }
}

#endif
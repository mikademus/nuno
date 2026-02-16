// arf_materialise.hpp - A Readable Format (Arf!) - Semantic analysis and materialiser
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

// Mandatory memory items:
// * Save invalid semantic nodes. They must be accessible for reflection and tooling.
// * Columns with invalid types must be marked as such. They will collapse to strings and this overwrites the type. That the type was invalid must be preserved.
// * Cells with invalid type resolutions must be marked as such.
// * Currently rows of invalid arity are rejected. This data must be preserved.
// * There may be name conflicts when children to rejected categories are attached the next valid ancestor. This must be handled.

#ifndef ARF_MATERIALISE_HPP
#define ARF_MATERIALISE_HPP

#include "arf_core.hpp"
#include "arf_parser.hpp"
#include "arf_document.hpp"
#include <array>
#include <memory>
#include <ranges>
#include <unordered_map>

#include <iostream>

namespace arf
{
    #define DBG_EMIT std::cout << "[M] "

    //#define TRACE_CONTAM(where, x) \
    //    DBG_EMIT << where << ": semantic=" << int(x.semantic) \
    //            << " contam=" << int(x.contamination) << "\n";
                    
    struct materialiser_options
    {
        bool own_parser_data {true}; // Document will assume ownership of the parser data. Without it the serialiser will not be able to output the original format.
        size_t max_category_depth {64};
    //--Debug options
        bool echo_lines  {false}; // prints each CST parser event to be handled
        bool echo_errors {false}; // prints each logged error 
    };    

    enum class semantic_error_kind
    {
        unknown_type,
        type_mismatch,
        declared_type_mismatch,
        invalid_literal,
        invalid_declared_type,
        invalid_array_element, 
        column_arity_mismatch,
        duplicate_key,
        invalid_subcategory,
        invalid_category_open,
        invalid_category_close,
        depth_exceeded,
    // warnings
        date_unsupported,
        __LAST
    };

    constexpr std::array<std::string_view, static_cast<size_t>(semantic_error_kind::__LAST)> 
    semantic_error_string =
    {
        "unknown type",
        "type mismatch",
        "declared type mismatch",
        "invalid literal",
        "invalid declared type",
        "invalid array element,",
        "column arity mismatch",
        "duplicate key",
        "invalid subcategory",
        "invalid category open",
        "invalid category close",
        "depth exceeded",
        "date unsupported",
    };

    using material_context = context<document, semantic_error_kind>;

    // Facade
    material_context materialise( const parse_context& ctx, materialiser_options opts = {});    
    material_context materialise(parse_context&& ctx, materialiser_options opts = {});

//========================================================================
// materialiser
//========================================================================

    struct materialiser
    {
        materialiser(const parse_context& ctx, materialiser_options opts);  // Non-owning

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

        // Materialisation can break the direct 
        // correspondence between CST events and
        // node IDs; so they must be mapped. 
        // nullopt means: resolve dynamically to current stack top
        std::vector<std::optional<category_id>> cst_to_doc_category_;


        // Helpers
        void insert_source_item(document::source_id id);
        document::table_node * find_table(table_id tid);
        document::category_node * find_category(category_id cid);

        void handle_category_open(const parse_event& ev, size_t parse_idx );
        void handle_category_close(const parse_event& ev, size_t parse_idx);
        void handle_table_header(const parse_event& ev, size_t parse_idx);
        void handle_table_row(const parse_event& ev, size_t parse_idx);        
        void handle_key(parse_event const& ev, size_t parse_idx);
        void handle_comment(parse_event const& ev, size_t parse_idx);
        void handle_paragraph(parse_event const& ev, size_t parse_idx);

        void log_err( semantic_error_kind what, std::string_view msg, source_location loc )
        {
            out_.errors.push_back({ what, loc, std::string(msg) });
            if (opts_.echo_errors)
                DBG_EMIT << "Error #" << std::to_string(static_cast<int>(what)) 
                        << ": " << semantic_error_string[static_cast<size_t>(what)] 
                        << ". Message: " << msg << "\n";
        }

        bool row_is_valid(document::row_node const& r);
        bool table_is_valid(document::table_node const& t, document const& doc);
    };

//========================================================================
// Implementation
//========================================================================

namespace 
{
    using error_sink = decltype(material_context::errors);

    inline void split_pipe( std::string_view s, std::vector<std::string_view>& out )
    {
        size_t pos = 0;
        while (true)
        {
            size_t next = s.find('|', pos);
            if (next == std::string_view::npos)
            {
                out.push_back(s.substr(pos));
                break;
            }
            out.push_back(s.substr(pos, next - pos));
            pos = next + 1;
        }
    }

    std::optional<bool> is_bool(std::string_view s)
    {
        static std::unordered_map<std::string_view, bool> booleans = 
        {
            {"true", true}, {"false", false},
            // {"yes",  true}, {"no",    false},
            // {"on",   true}, {"off",   false},
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
            {"float", value_type::floating_point},
            {"bool", value_type::boolean},
            {"date", value_type::date},
            {"str[]", value_type::string_array},
            {"int[]", value_type::integer_array},
            {"float[]", value_type::floating_point_array},
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
        tv.origin = value_locus::key_value;
        tv.creation = creation_state::authored;

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
            tv.type = value_type::floating_point;
            tv.val  = d;
            return tv;
        }

        tv.type = value_type::string;
        tv.val  = std::string(s);
        return tv;
    }    

    inline std::optional<value> try_convert(std::string_view s, value_type t, source_location loc, error_sink & err)
    {
        auto log_err = [&err, loc, s, t]()
        {
            std::string msg("could not convert ");
            msg += s;
            msg += " to ";
            msg += type_to_string(t);

            err.push_back({
                semantic_error_kind::type_mismatch,
                loc,
                msg
            });
        };

        switch (t)
        {
            case value_type::string:
                return std::string(s);

            case value_type::integer:
            {
                char* end = nullptr;
                long v = std::strtol(s.data(), &end, 10);
                if (end != s.data() + s.size())
                {
                    log_err();
                    return std::nullopt;
                }
                return static_cast<int64_t>(v);
            }

            case value_type::floating_point:
            {
                char* end = nullptr;
                double v = std::strtod(s.data(), &end);
                if (end != s.data() + s.size())
                {
                    log_err();
                    return std::nullopt;
                }
                return v;
            }

            case value_type::boolean:
                if (auto b = is_bool(s); b.has_value())
                    return b.value();
                log_err();
                return std::nullopt;

            case value_type::string_array:
            case value_type::integer_array:
            case value_type::floating_point_array:
                // Arrays are handled elsewhere; cell literal stays intact
                return std::nullopt;

            default:
                return std::nullopt;
        }
    }

    inline value_type array_type_of(value_type scalar)
    {
        switch (scalar)
        {
            case value_type::string:  return value_type::string_array;
            case value_type::integer: return value_type::integer_array;
            case value_type::floating_point: return value_type::floating_point_array;
            default:                  return value_type::unresolved;
        }
    }

    inline bool array_has_invalid_elements(const typed_value& tv)
    {
        if (!std::holds_alternative<std::vector<typed_value>>(tv.val))
            return false;

        auto const& elems = std::get<std::vector<typed_value>>(tv.val);
        for (auto const& e : elems)
            if (e.semantic == semantic_state::invalid)
                return true;

        return false;
    }

    inline typed_value coerce_array(
        std::string_view literal,
        value_type declared_type,
        value_locus origin,
        source_location loc,
        material_context& ctx
    )
    {
        typed_value tv;
        tv.type        = declared_type;
        tv.type_source = type_ascription::declared;
        tv.origin      = origin;
        tv.semantic    = semantic_state::valid;
        tv.creation    = creation_state::authored;

        tv.val = std::vector<typed_value>{};
        auto& values = std::get<std::vector<typed_value>>(tv.val);

        const bool want_int   = declared_type == value_type::integer_array;
        const bool want_float = declared_type == value_type::floating_point_array;
        const bool want_str   = declared_type == value_type::string_array;

        bool array_contaminated = false;

        size_t pos = 0;
        while (pos < literal.size())
        {
            const size_t next = literal.find('|', pos);
            const size_t len =
                (next == std::string_view::npos)
                    ? literal.size() - pos
                    : next - pos;

            std::string_view part = literal.substr(pos, len);

            typed_value elem;
            elem.origin = value_locus::array_element;

            // Empty element: preserved, missing but not invalid
            if (part.empty())
            {
                elem.type          = value_type::unresolved;
                elem.val           = std::monostate{};
                elem.creation      = creation_state::authored;
                elem.type_source   = type_ascription::tacit;
                elem.semantic      = semantic_state::valid;
                elem.contamination = contamination_state::clean;
            }
            else if (want_int)
            {
                if (auto v = try_convert(part, value_type::integer, loc, ctx.errors); v.has_value())
                {
                    elem.val           = std::get<int64_t>(*v);
                    elem.type          = value_type::integer;
                    elem.type_source   = type_ascription::declared;
                    elem.contamination = contamination_state::clean;
                }
                else
                {
                    elem.val           = std::string(part);
                    elem.type          = value_type::string;
                    elem.type_source   = type_ascription::tacit;
                    elem.semantic      = semantic_state::invalid;
                    elem.contamination = contamination_state::clean;
                    array_contaminated = true;
                }
            }
            else if (want_float)
            {
                if (auto v = try_convert(part, value_type::floating_point, loc, ctx.errors); v.has_value())
                {
                    elem.val           = std::get<double>(*v);
                    elem.type          = value_type::floating_point;
                    elem.type_source   = type_ascription::declared;
                    elem.contamination = contamination_state::clean;
                }
                else
                {
                    elem.val           = std::string(part);
                    elem.type          = value_type::string;
                    elem.type_source   = type_ascription::tacit;
                    elem.semantic      = semantic_state::invalid;
                    elem.contamination = contamination_state::clean;
                    array_contaminated = true;
                }
            }
            else if (want_str)
            {
                elem.val           = std::string(part);
                elem.type          = value_type::string;
                elem.type_source   = type_ascription::declared;
                elem.contamination = contamination_state::clean;
            }
            else
            {
                // Defensive: unresolved array type
                elem.val           = std::string(part);
                elem.type          = value_type::string;
                elem.type_source   = type_ascription::tacit;
                elem.semantic      = semantic_state::invalid;
                elem.contamination = contamination_state::clean;
                array_contaminated = true;
            }

            elem.creation = creation_state::authored;
            values.push_back(std::move(elem));

            if (next == std::string_view::npos)
                break;

            pos = next + 1;
        }

        // Handle trailing delimiter: "1|2|" → final empty element
        if (!literal.empty() && literal.back() == '|')
        {
            typed_value elem;
            elem.origin        = value_locus::array_element;
            elem.type          = value_type::unresolved;
            elem.val           = std::monostate{};
            elem.type_source   = type_ascription::tacit;
            elem.semantic      = semantic_state::valid;
            elem.contamination = contamination_state::clean;
            elem.creation      = creation_state::authored;

            values.push_back(std::move(elem));
        }

        if (array_contaminated)
        {
            tv.contamination = contamination_state::contaminated;

            ctx.errors.push_back({
                semantic_error_kind::invalid_array_element,
                loc,
                "one or more array elements are invalid"
            });

        }
        
        return tv;
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
        tv.creation = creation_state::authored;

        // Unresolved or string → accept verbatim
        if (column_type == value_type::unresolved ||
            column_type == value_type::string)
        {
            tv.type = value_type::string;
            tv.val  = std::string(literal);
            return tv;
        }

        // Attempt strict conversion
        auto converted = try_convert(literal, column_type, loc, ctx.errors);
        if (!converted)
        {
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

            typed_value tv;
            tv.val         = std::string(literal);
            tv.type        = value_type::string;
            tv.type_source = type_ascription::tacit;
            tv.origin      = value_locus::key_value;
            tv.semantic    = semantic_state::invalid;
            tv.creation    = creation_state::authored;
            return tv;
        }

        auto tv = *inferred;

        // No declared type or exact match
        if (target == value_type::unresolved || tv.type == target)
            return tv;

        // Try coercion (value-only)
        if (auto v = try_convert(literal, target, loc, out.errors))
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
        {
            typed_value tv;
            tv.val         = std::string(literal);
            tv.type        = value_type::string;
            tv.type_source = type_ascription::tacit;
            tv.origin      = value_locus::key_value;
            tv.semantic    = semantic_state::invalid;
            tv.creation    = creation_state::authored;
            return tv;
        }
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
        cst_to_doc_category_.resize(cst_.categories.size());
        category_id root = doc_.create_root();
        stack_.push_back(root);
    }

    inline material_context materialiser::run()
    {
        for (size_t i = 0; i < cst_.events.size(); ++i)
        {
            const auto& ev = cst_.events[i];

            switch (ev.kind)
            {
                case parse_event_kind::category_open:
                    if (opts_.echo_lines) DBG_EMIT << "event " << i << ": category_open = " << ev.text << "\"" << std::endl;
                    handle_category_open(ev, i);
                    break;

                case parse_event_kind::category_close:
                    if (opts_.echo_lines) DBG_EMIT << "event " << i << ": category_close = " << ev.text << "\"" << std::endl;
                    handle_category_close(ev, i);
                    break;

                case parse_event_kind::table_header:
                    if (opts_.echo_lines) DBG_EMIT << "event " << i << ": table_header = " << ev.text << "\"" << std::endl;
                    handle_table_header(ev, i);
                    break;

                case parse_event_kind::table_row:
                    if (opts_.echo_lines) DBG_EMIT << "event " << i << ": table_row = " << ev.text << "\"" << std::endl;
                    handle_table_row(ev, i);
                    break;

                case parse_event_kind::key_value:
                    if (opts_.echo_lines) DBG_EMIT << "event " << i << ": key_value = " << ev.text << "\"" << std::endl;
                    handle_key(ev, i);
                    break;

                case parse_event_kind::comment:
                    if (opts_.echo_lines) DBG_EMIT << "event " << i << ": comment \"" << ev.text << "\"" << std::endl;
                    handle_comment(ev, i);
                    break;

                case parse_event_kind::paragraph:
                    if (opts_.echo_lines) DBG_EMIT << "event " << i << ": paragraph \"" << ev.text << "\"" << std::endl;
                    handle_paragraph(ev, i);
                    break;

                default:
                    if (opts_.echo_lines) DBG_EMIT << "event " << i << " unknown, skipped = " << ev.text << "\"" << std::endl;
                    break;
            }
        }

        if (opts_.own_parser_data)
        {
            // Transfer ownership via move
            out_.document.source_context_ = 
                // This works, which is a bit surprising.
                std::make_unique<parse_context>(std::move(ctx_));
                // I had expected the const-ness of ctx_ to require:
                //std::make_unique<parse_context>(std::move(const_cast<parse_context&>(ctx_)));
        }
                
        // Register contamination sources
        for (auto const& key : doc_.keys_)
            if (key.contamination == contamination_state::contaminated)
                doc_.mark_key_contaminated(key.id);

        for (auto const& row : doc_.rows_)
            if (row.contamination == contamination_state::contaminated)
                doc_.mark_row_contaminated(row.id);

        if (!doc_.categories_.empty())  doc_.next_category_id_  = doc_.categories_.back().id + 1;
        if (!doc_.columns_.empty())     doc_.next_column_id_    = doc_.columns_.back()._id() + 1;
        if (!doc_.comments_.empty())    doc_.next_comment_id_   = doc_.comments_.back().id + 1;
        if (!doc_.keys_.empty())        doc_.next_key_id_       = doc_.keys_.back().id + 1;
        if (!doc_.paragraphs_.empty())  doc_.next_paragraph_id_ = doc_.paragraphs_.back().id + 1;
        if (!doc_.rows_.empty())        doc_.next_row_id_       = doc_.rows_.back().id + 1;
        if (!doc_.tables_.empty())      doc_.next_table_id_     = doc_.tables_.back().id + 1;

        return std::move(out_);
    }

    inline void materialiser::handle_category_open(const parse_event& ev, size_t parse_idx)
    {
        auto cid = std::get<category_id>(ev.target);

        auto sv = std::string_view(ev.text);
        bool is_subcat = sv.starts_with(":");
        bool is_topcat = sv.ends_with(":");

        // Default: unresolved (invalid category)
        cst_to_doc_category_[cid.val] = std::nullopt;

        auto fail = [&](semantic_error_kind k, const char* msg)
        {
            log_err(k, msg, ev.loc);
            active_table_.reset();
        };

        if (is_subcat && is_topcat)
            return fail(semantic_error_kind::invalid_category_open,
                        "category can't be both top-level and subcategory");

        if (is_subcat && stack_.size() <= 1)
            return fail(semantic_error_kind::invalid_subcategory,
                        "subcategory must not be declared in the root");

        if (opts_.max_category_depth != 0 && stack_.size() - 1 >= opts_.max_category_depth)
            return fail(semantic_error_kind::depth_exceeded,
                        "maximum category depth exceeded");

        // Valid category from here on

        if (is_topcat)
        {
            while (stack_.size() > 1)
                stack_.pop_back();
        }

        active_table_.reset();

        const auto& cst_cat = cst_.categories[cid.val];
        category_id parent  = stack_.back();

        category_id doc_id = doc_.create_category(cid, cst_cat.name, parent);
        if (opts_.echo_lines) DBG_EMIT << "created category id: " << cid.val << ", name: " << cst_cat.name << std::endl;

        auto it = std::ranges::find_if(doc_.categories_, [doc_id](document::category_node const &cat){return cat.id.val == doc_id.val;});
        assert (it != doc_.categories_.end());

        it->source_event_index_open = parse_idx;        
        it->creation = creation_state::authored;
        it->semantic = semantic_state::valid;

        cst_to_doc_category_[cid.val] = doc_id;
        insert_source_item(doc_id);
        stack_.push_back(doc_id);
    }

    inline void materialiser::handle_category_close(const parse_event& ev, size_t parse_idx)
    {
        // Named close: unresolved name
        if (std::holds_alternative<unresolved_name>(ev.target))
        {
            auto name = std::get<unresolved_name>(ev.target);

            if (stack_.size() <= 1)
            {
                log_err(semantic_error_kind::invalid_category_close,
                    "attempt to close category that is not open",
                    ev.loc );
                return;
            }

            if (name.empty())
            {
                log_err( semantic_error_kind::invalid_category_close,
                         "empty category name in close",
                         ev.loc);
                return;
            }

            auto it = std::find_if(
                stack_.rbegin(),
                stack_.rend(),
                [&](category_id cid)
                {
                    decltype(document::categories_)::iterator it;
                    it = std::ranges::find_if(doc_.categories_, [cid](document::category_node const &cat){return cat.id.val == cid.val;});
                    if (it != doc_.categories_.end())
                        return it->name == name;
                    return false;
                }
            );

            if (it == stack_.rend() || (*it).val == stack_.front().val)
            {
                log_err(semantic_error_kind::invalid_category_close,
                        "attempt to close category that is not open",
                        ev.loc);
                return;
            }

            auto cat_it = std::ranges::find_if(doc_.categories_, [cid = *it](document::category_node const &cat){return cat.id.val == cid.val;});
            assert (cat_it != doc_.categories_.end());
            cat_it->source_event_index_close = parse_idx;

            while (stack_.back() != *it)
                stack_.pop_back();

            stack_.pop_back();
            active_table_.reset();

            document::category_close_marker marker{*it, document::category_close_form::named};
            insert_source_item(marker);

            return;
        }

        // Implicit close: category_id
        if (std::holds_alternative<category_id>(ev.target))
        {
            if (stack_.size() <= 1)
            {
                log_err( semantic_error_kind::invalid_category_close,
                         "attempt to close root category",
                         ev.loc);
                return;
            }

            auto closing = std::get<category_id>(ev.target);

            if (stack_.back() != closing)
            {
                log_err( semantic_error_kind::invalid_category_close,
                         "category close does not match open scope",
                         ev.loc );
                return;
            }

            auto cat_it = std::ranges::find_if(doc_.categories_, [closing](document::category_node const &cat){return cat.id.val == closing.val;});
            assert (cat_it != doc_.categories_.end());
            cat_it->source_event_index_close = parse_idx;

            stack_.pop_back();
            active_table_.reset();

            document::category_close_marker marker{closing, document::category_close_form::shorthand};
            insert_source_item(marker);            
        }
    }

    inline void materialiser::handle_table_header(const parse_event& ev, size_t parse_idx)
    {
        auto tid = std::get<table_id>(ev.target);
        const auto& cst_tbl = cst_.tables[tid.val];

        document::table_node tbl;
        tbl.id       = tid;
        tbl.creation = creation_state::authored;
        tbl.owner    = stack_.back();
        tbl.source_event_index = parse_idx;
        tbl.rows.clear();

        for (const auto& cst_col : cst_tbl.columns)
        {
            document::column_node col_;
            col_.col      = cst_col;
            col_.table    = tid;
            col_.creation = creation_state::authored;
            col_.owner    = tbl.owner;
            col_.source_event_index = parse_idx;

            column & col = col_.col;

            // Resolve declared column types early
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
                    tbl.contamination = contamination_state::contaminated;
                }
                else
                {
                    col.type = *vt;
                    if (s == "date")
                    {
                        col.semantic = semantic_state::invalid;
                        tbl.contamination = contamination_state::contaminated;
                    }
                }

                if (tbl.contamination == contamination_state::contaminated)
                    doc_.propagate_contamination_up_category_chain(col_.owner);
            }
            else
            {
                col.type = value_type::unresolved;
            }

            doc_.columns_.push_back(col_);
            tbl.columns.push_back(col_.col.id);
        }

        // Store the table
        doc_.tables_.push_back(std::move(tbl));

        // Attach table to owning category
        category_id cat_id = stack_.back();

        auto it = doc_.find_node_by_id(doc_.categories_, cat_id);
        assert (it != doc_.categories_.end());
        it->tables.push_back(tid);
        insert_source_item(tid);

        active_table_ = tid;
    }


    inline void materialiser::handle_table_row(const parse_event& ev, size_t parse_idx)
    {
        if (!active_table_)
            return; // syntactically valid, semantically inert

        auto rid = std::get<row_id>(ev.target);
        const auto& cst_row = cst_.rows[rid.val];

        auto it = doc_.find_node_by_id(doc_.tables_, *active_table_);
        assert(it != doc_.tables_.end());
        auto & tbl = *it;

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
        row.id                 = rid;
        row.table              = tbl.id;
        row.creation           = creation_state::authored;
        row.owner              = stack_.back();
        row.semantic           = semantic_state::valid;
        row.contamination      = contamination_state::clean;        
        row.source_event_index = parse_idx;
        row.cells.reserve(tbl.columns.size());

        // Column invalidity contaminates the row
        for (auto const& col_id : tbl.columns)
        {
            auto it = doc_.find_node_by_id(doc_.columns_, col_id);
            assert (it != doc_.columns_.end());
            if (it->col.semantic == semantic_state::invalid)
            {
                row.contamination = contamination_state::contaminated;
                break;
            }
        }

        for (size_t i = 0; i < tbl.columns.size(); ++i)
        {
            auto it = doc_.find_node_by_id(doc_.columns_, tbl.columns[i]);
            assert (it != doc_.columns_.end());
            auto & col = it->col;

            std::string literal =
                (i < cst_row.cells.size())
                    ? cst_row.cells[i].value_to_string()//std::string_view(cst_row.cells[i].source_literal.value())
                    : "";

            typed_value tv;
            tv.origin = value_locus::table_cell;

            if (col.type == value_type::string_array ||
                col.type == value_type::integer_array ||
                col.type == value_type::floating_point_array)
            {
                tv = coerce_array(literal, col.type, value_locus::table_cell, ev.loc, out_);
            }
            else
            {
                tv = coerce_cell(literal, col.type, ev.loc, out_);
            }

            row.cells.push_back(std::move(tv));
        }

        // Cell or array invalidity contaminates the row
        for (auto const& c : row.cells)
        {
            if (c.semantic == semantic_state::invalid)
            {
                row.contamination = contamination_state::contaminated;
                break;
            }

            if (array_has_invalid_elements(c))
            {
                row.contamination = contamination_state::contaminated;
                break;
            }
        }

        doc_.rows_.push_back(std::move(row));
        tbl.rows.push_back(rid);
        insert_source_item(rid);
    }

    inline void materialiser::handle_key(parse_event const& ev, size_t parse_idx)
    {
        auto kid = std::get<key_id>(ev.target);
        const cst_key& cst = cst_.keys.at(kid.val);

        document::key_node k;
        k.id    = kid;
        k.name  = cst.name;
        k.creation = creation_state::authored;
        k.owner = stack_.back(); 
        k.source_event_index = parse_idx;

        k.type_source = cst.declared_type
                            ? type_ascription::declared
                            : type_ascription::tacit;

        value_type target = value_type::unresolved;

        if (cst.declared_type)
        {
            auto t = parse_declared_type(*cst.declared_type, out_);
            if (!t)
            {
                log_err(
                    semantic_error_kind::invalid_declared_type,
                    "unknown declared key type",
                    cst.loc
                );

                target        = value_type::string;
                k.type_source = type_ascription::tacit;
                k.semantic    = semantic_state::invalid;
            }
            else
            {
                target = *t;
            }
        }

        typed_value tv;

        const bool target_is_array =
            target == value_type::string_array ||
            target == value_type::integer_array ||
            target == value_type::floating_point_array;

        if (target_is_array)
        {
            tv = coerce_array(
                cst.literal, target,
                value_locus::key_value,
                cst.loc, out_
            );
        }
        else
        {
            tv = coerce_key_value(
                cst.literal, target,
                cst.loc, out_
            );
        }

        if (tv.semantic == semantic_state::invalid)
            k.semantic = semantic_state::invalid;

        if (tv.contamination == contamination_state::contaminated)
            k.contamination = contamination_state::contaminated;
        else if (target_is_array)
        {
            for (auto& elem : std::get<std::vector<typed_value>>(tv.val))
            {
                elem.creation = creation_state::authored;

                if (elem.semantic == semantic_state::invalid ||
                    elem.contamination == contamination_state::contaminated)
                {
                    k.contamination = contamination_state::contaminated;
                    break;
                }
            }
        }

        k.type  = tv.type;
        k.value = std::move(tv);

        key_id id{ doc_.keys_.size() };
        doc_.keys_.emplace_back(std::move(k));
        auto& key = doc_.keys_[id.val];
        
        auto it = std::ranges::find_if(doc_.categories_, [&](auto const & c){return c.id.val == key.owner.val;});
        assert (it != doc_.categories_.end() && "Category doesn't exist");
        auto & cat = *it;

        cat.keys.emplace_back(id);
        insert_source_item(id);
    }

    document::table_node * materialiser::find_table(table_id tid)
    {
        for (auto & t : doc_.tables_)
            if (t.id == tid)
                return &t;
        return nullptr;
    }
    document::category_node * materialiser::find_category(category_id cid)
    {
        for (auto & c : doc_.categories_)
            if (c.id == cid)
                return &c;
        return nullptr;
    }

    void materialiser::insert_source_item(document::source_id id)  // Takes the variant directly
    {
        if (active_table_)
        {
            if (auto ptr = find_table(*active_table_))
                ptr->ordered_items.push_back(document::source_item_ref{id});
        }
        else
        {
            if (auto ptr = find_category(stack_.back()))
                ptr->ordered_items.push_back(document::source_item_ref{id});
        }
    }

    inline void materialiser::handle_comment(const parse_event& ev, size_t parse_idx)
    {
        comment_id cid = doc_.create_comment_id();
        
        document::comment_node cn;
        cn.id       = cid;
        cn.text     = ev.text;
        cn.owner    = stack_.back();
        cn.creation = creation_state::authored;
        cn.source_event_index = parse_idx;
        
        doc_.comments_.push_back(std::move(cn));
        
        if (opts_.echo_lines)
            DBG_EMIT << "Created comment_id{" << cid.val << "} with text: \"" << ev.text << "\"\n";
        
        insert_source_item(cid);
    }

    void materialiser::handle_paragraph(const parse_event& ev, size_t parse_idx)
    {
        paragraph_id pid = doc_.create_paragraph_id();
        
        document::paragraph_node pn;
        pn.id       = pid;
        pn.text     = ev.text;
        pn.owner    = stack_.back();
        pn.creation = creation_state::authored;
        pn.source_event_index = parse_idx;
        
        doc_.paragraphs_.push_back(std::move(pn));
        
        if (opts_.echo_lines)
            DBG_EMIT << "Created paragraph_id{" << pid.val << "} with text: \"" << ev.text << "\"\n";
        
        insert_source_item(pid);
    }

    inline bool materialiser::row_is_valid(document::row_node const& r)
    {
        for (auto const& c : r.cells)
            if (c.semantic == semantic_state::invalid)
                return false;
        return true;
    }

    inline bool materialiser::table_is_valid(document::table_node const& t, document const& doc)
    {
        for (auto const& c : t.columns)
        {
            auto it = doc_.find_node_by_id(doc_.columns_, c);
            
            if (it == doc_.columns_.end())
                return false;

            if (it->col.semantic == semantic_state::invalid)
                return false;
        }

        for (auto rid : t.rows)
            if (!row_is_valid(doc.rows_[rid.val]))
                return false;

        return true;
    }    


    // Non-owning (keeps parse_context alive externally)
    material_context materialise(const parse_context& ctx, materialiser_options opts)
    {
        opts.own_parser_data = false;  // Force non-owning
        materialiser m(ctx, opts);
        return m.run();
    }

    // Owning (transfers parse_context into document)
    material_context materialise(parse_context&& ctx, materialiser_options opts)
    {
        opts.own_parser_data = true;  // Force owning
        materialiser m(std::move(ctx), opts);
        return m.run();
    }

#undef DBG_EMIT    

}

#endif
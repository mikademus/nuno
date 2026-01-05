// arf_materialise.hpp - A Readable Format (Arf!)
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_A_READABLE_FORMAT
#define ARF_A_READABLE_FORMAT

#include "arf_core.hpp"
#include "arf_parser.hpp"
#include "arf_materialise.hpp"
#include "arf_document.hpp"

namespace arf
{
//========================================================================
// Document creation errors
//========================================================================

    using any_error = std::variant<error<parse_error_kind>, error<semantic_error_kind>>;

    using doc_context = context<document, any_error>;
    doc_context load(std::string_view text, materialiser_options opt = {} );

    inline bool is_parse_error(any_error const &e) { return std::holds_alternative<error<parse_error_kind>>(e); }
    inline bool is_material_error(any_error const &e) { return std::holds_alternative<error<semantic_error_kind>>(e); }

    inline parse_error_kind get_parse_error(any_error const & e) { return std::get<error<parse_error_kind>>(e).kind; }
    inline semantic_error_kind get_material_error(any_error const & e) { return std::get<error<semantic_error_kind>>(e).kind; }


    inline doc_context load( std::string_view src, materialiser_options opt )
    {
        doc_context out{};

        auto parse_ctx = parse(src);
        auto mat_ctx   = materialise(parse_ctx, opt);

        out.document = std::move(mat_ctx.document);

        out.errors.reserve(parse_ctx.errors.size() + mat_ctx.errors.size());

        for (auto const & pe : parse_ctx.errors)
        {
            error<any_error> err;
            err.kind = pe;
            out.errors.push_back(err);
        }

        for (auto const & se : mat_ctx.errors)
        {
            error<any_error> err;
            err.kind = se;
            out.errors.push_back(err);
        }

        return out;
    }

}

#endif
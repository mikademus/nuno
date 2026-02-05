// arf_parser.hpp - A Readable Format (Arf!) - Parser raw text to CST document
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_PARSER_HPP
#define ARF_PARSER_HPP

#include "arf_core.hpp"
#include <assert.h>
#include <cstdlib>
#include <sstream>

namespace arf 
{
//========================================================================
// Structure and parsing
//========================================================================
    
    enum class parse_event_kind
    {
        comment,        // "// ..."
        paragraph,      // any non-grammar text (including empty lines)

        key_value,
        table_header,
        table_row,

        category_open,
        category_close
    };

    inline std::string_view to_string(parse_event_kind kind)
    {
        switch (kind)
        {
            using enum parse_event_kind;
            case comment: return "comment";
            case paragraph: return "paragraph";
            case key_value: return "key_value";
            case table_header: return "table_header";
            case table_row: return "table_row";
            case category_open: return "category_open";
            case category_close: return "category_close";
        }
        return "unknown";
    }

    using unresolved_name = std::string_view;

    using parse_event_target = 
        std::variant
        <
            std::monostate,
            unresolved_name,  
            category_id, 
            table_id, 
            table_row_id,
            key_id
        >;


    struct parse_event
    {
        parse_event_kind   kind;
        source_location    loc;
        std::string        text;        
        parse_event_target target; // Optional semantic attachment
    };
    
//========================================================================
// PARSER API
//========================================================================

    struct cst_key
    {
        category_id owner;
        std::string name;
        std::optional<std::string> declared_type; // raw text after ':'
        std::string literal;                      // RHS verbatim
        source_location loc;
    };

    struct cst_document
    {
        // Primary spine
        std::vector<parse_event> events;

        // Entities
        std::vector<category>    categories;
        std::vector<table>       tables;
        std::vector<table_row>   rows;
        std::vector<cst_key>     keys;
    };

    enum struct parse_error_kind
    {
        nothing,
    };

    using parse_context = context<cst_document, parse_error_kind>;

    parse_context parse(const std::string& input);
    parse_context parse(const std::string_view input);    
    
//========================================================================
// Implementation details
//========================================================================
    
    namespace
    {
        using namespace detail;

        struct parser_impl
        {
            parse_context ctx;

            // Columns are stored per-table so a global counter is needed
            column_id next_column_id {0};

            // Active context
            std::vector<category_id> category_stack;
            table_id active_table {invalid_id<table_tag>()};

            // Blobbing state for comments and paragraphs
            std::vector<std::string> pending_comment_lines;
            std::vector<std::string> pending_paragraph_lines;
            
            void flush_pending_comment();
            void flush_pending_paragraph();
            void flush_all_pending();

            void parse(std::string_view input);
            void add_error(const std::string& message);

            std::vector<std::string> split_lines(const std::string& input);
            std::vector<std::string> split_table_cells(std::string_view line);

            void parse_line(std::string_view line, size_t line_no);

            void create_root_category();

            void open_top_level_category(std::string_view name, parse_event& ev);
            void open_category(std::string_view name, parse_event& ev);
            void close_category(std::string_view, parse_event& ev);

            bool key_value(parse_event& ev);

            void start_table(std::string_view header, parse_event& ev);
            bool table_row(std::string_view text, parse_event& ev);
        };

//---------------------------------------------------------------------------        

        void parser_impl::parse(std::string_view input)
        {
            size_t line_no = 0;
            create_root_category();
            
            size_t start = 0;
            while (start < input.size())
            {
                size_t end = input.find('\n', start);
                
                // Extract line (either to newline or to end of input)
                std::string_view line = (end == std::string_view::npos) 
                    ? input.substr(start) 
                    : input.substr(start, end - start);
                
                // Trim \r for Windows line endings
                if (!line.empty() && line.back() == '\r')
                    line.remove_suffix(1);
                
                parse_line(line, ++line_no);
                
                // If no more newlines, we're done
                if (end == std::string_view::npos)
                    break;
                    
                start = end + 1;
            }
            
            // Flush any pending blobs at end of document
            flush_all_pending();            
        }

//---------------------------------------------------------------------------        

        void parser_impl::add_error(const std::string& message)
        {
            ctx.errors.push_back({
                parse_error_kind::nothing,
                { 0 }, // line tracking can be improved later
                message,
            });            
        }

//---------------------------------------------------------------------------        

        void parser_impl::create_root_category()
        {
            assert (ctx.document.categories.empty() && "Root must be the first category");

            category root;
            root.id     = 0;
            root.name   = "__root__";
            root.parent = invalid_id<category_tag>();

            ctx.document.categories.push_back(root);
            category_stack.push_back(root.id);
        }

//---------------------------------------------------------------------------        

        std::vector<std::string> parser_impl::split_lines(const std::string& input) 
        {
            std::vector<std::string> result;
            std::istringstream stream(input);
            std::string line;
            while (std::getline(stream, line))
                result.push_back(line);
            return result;
        }

//---------------------------------------------------------------------------        
            
        std::vector<std::string> parser_impl::split_table_cells(std::string_view line) 
        {
            std::vector<std::string> cells;
            std::string current;
            int spaces = 0;
            
            for (char c : line) 
            {
                if (c == ' ') 
                {
                    ++spaces;
                    if (spaces >= 2 && !current.empty()) 
                    {
                        cells.push_back(std::string(trim_sv(current)));
                        current.clear();
                        spaces = 0;
                    }
                } 
                else 
                {
                    if (spaces == 1) current += ' ';
                    spaces = 0;
                    current += c;
                }
            }
            
            if (!current.empty())
                cells.push_back(std::string(trim_sv(current)));
            
            // If we only got 1 cell and it contains '=', this is likely a key-value pair
            // that shouldn't be parsed as a table row at all
            if (cells.size() == 1 && cells[0].find('=') != std::string::npos)
            {
                cells.clear(); // Return empty to signal this isn't a valid table row
            }
            
            return cells;
        }

//---------------------------------------------------------------------------

        void parser_impl::flush_pending_comment()
        {
            if (pending_comment_lines.empty())
                return;

            parse_event ev;
            ev.kind = parse_event_kind::comment;
            ev.loc.line = 0;  // TODO: track first line number if needed
            
            // Join lines with newlines to create multi-line blob
            std::string blob;
            for (size_t i = 0; i < pending_comment_lines.size(); ++i)
            {
                blob += pending_comment_lines[i];
                if (i + 1 < pending_comment_lines.size())
                    blob += '\n';
            }
            ev.text = std::move(blob);
            
            ctx.document.events.push_back(ev);
            pending_comment_lines.clear();
        }

//---------------------------------------------------------------------------

        void parser_impl::flush_pending_paragraph()
        {
            if (pending_paragraph_lines.empty())
                return;

            parse_event ev;
            ev.kind = parse_event_kind::paragraph;
            ev.loc.line = 0;  // TODO: track first line number if needed
            
            // Join lines with newlines to create multi-line blob
            std::string blob;
            for (size_t i = 0; i < pending_paragraph_lines.size(); ++i)
            {
                blob += pending_paragraph_lines[i];
                if (i + 1 < pending_paragraph_lines.size())
                    blob += '\n';
            }

            ev.text = std::move(blob);
            
            ctx.document.events.push_back(ev);
            pending_paragraph_lines.clear();
        }

//---------------------------------------------------------------------------

        void parser_impl::flush_all_pending()
        {
            flush_pending_comment();
            flush_pending_paragraph();
        }        

//---------------------------------------------------------------------------        

    void parser_impl::parse_line(std::string_view line, size_t line_no)
    {
        std::string_view trimmed = trim_sv(line);

        // Empty lines become paragraphs
        if (trimmed.empty())
        {
            flush_pending_comment();
            pending_paragraph_lines.push_back(std::string(line));
            return;
        }

        // Comments: accumulate into blob
        if (trimmed.starts_with("//"))
        {
            flush_pending_paragraph();
            pending_comment_lines.push_back(std::string(line));
            return;
        }

        // All structural tokens flush pending blobs
        parse_event ev;
        ev.loc.line = line_no;
        ev.text     = line;

        // Category open (subcategory)
        if (trimmed.starts_with(":"))
        {
            flush_all_pending();
            open_category(trimmed.substr(1), ev);
            return;
        }

        // Category close
        if (trimmed.starts_with("/") && trimmed.size() > 1)
        {
            flush_all_pending();
            close_category(trimmed.substr(1), ev);
            return;
        }

        // Top-level category
        if (trimmed.ends_with(":"))
        {
            flush_all_pending();
            open_top_level_category(trimmed.substr(0, trimmed.size() - 1), ev);
            return;
        }

        // Table header
        if (trimmed.starts_with("#"))
        {
            flush_all_pending();
            start_table(trimmed.substr(1), ev);
            return;
        }

        // Key/value
        if (trimmed.find('=') != std::string_view::npos)
        {
            flush_all_pending();
            if (!key_value(ev))
            {
                // Malformed key - treat as paragraph
                pending_paragraph_lines.push_back(std::string(line));
            }
            return;
        }

        // Table row (if inside a table)
        if (active_table != invalid_id<table_tag>())
        {
            flush_all_pending();
            if (!table_row(trimmed, ev))
            {
                // Not a valid row - treat as paragraph
                pending_paragraph_lines.push_back(std::string(line));
            }
            return;
        }

        // Otherwise: paragraph (non-grammar text)
        flush_pending_comment();
        pending_paragraph_lines.push_back(std::string(line));
    }

//---------------------------------------------------------------------------        

        void parser_impl::open_top_level_category(std::string_view name, parse_event& ev)
        {
            category_stack.resize(1); // back to root
            active_table = npos();

            open_category(name, ev);
        }

//---------------------------------------------------------------------------        

        void parser_impl::open_category(std::string_view name, parse_event& ev)
        {
            category cat;
            cat.id     = ctx.document.categories.size();
            cat.name   = to_lower(std::string(trim_sv(name)));
            cat.parent = category_stack.back();

            ctx.document.categories.push_back(cat);
            category_stack.push_back(cat.id);

            ev.kind   = parse_event_kind::category_open;
            ev.target = cat.id;

            ctx.document.events.push_back(ev);
        }

//---------------------------------------------------------------------------        

        void parser_impl::close_category(std::string_view name, parse_event& ev)
        {
            ev.kind = parse_event_kind::category_close;

            if (category_stack.size() <= 1)
            {
                // malformed close: preserve authored intent as comment
                // instead of discarding.Do NOT blob with any previous
                // comment; keep separate.
                flush_all_pending();
                pending_comment_lines.push_back(std::string("// ") + std::string(ev.text));
                return;
            }            

            // Named close: preserve the name as written
            if (!name.empty())
            {
                ev.target = unresolved_name{name};
                ctx.document.events.push_back(ev);
                return;
            }

            category_id closing = category_stack.back();
            category_stack.pop_back();

            active_table = invalid_id<table_tag>();

            ev.target = closing;
            ctx.document.events.push_back(ev);
        }


//---------------------------------------------------------------------------        

        void parser_impl::start_table(std::string_view header, parse_event& ev)
        {
            table tbl;
            tbl.id              = ctx.document.tables.size();
            tbl.owning_category = category_stack.back();

            auto cols = split_table_cells(header);
            for (auto& c : cols)
            {
                column col;
                col.id = next_column_id++;
                auto pos = c.find(':');
                if (pos != std::string::npos)
                {
                    col.name = to_lower(c.substr(0, pos));
                    col.type = value_type::unresolved;
                    col.type_source = type_ascription::declared;
                    col.declared_type = std::string(trim_sv(c.substr(pos + 1)));
                }
                else
                {
                    col.name = to_lower(c);
                    col.type = value_type::unresolved;
                    col.type_source = type_ascription::tacit;
                }
                tbl.columns.push_back(col);
            }

            ctx.document.tables.push_back(tbl);
            active_table = tbl.id;

            ev.kind   = parse_event_kind::table_header;
            ev.target = tbl.id;

            ctx.document.events.push_back(ev);
        }

//---------------------------------------------------------------------------        

        bool parser_impl::table_row(std::string_view text, parse_event& ev)
        {
            auto cells = split_table_cells(text);

            if (cells.empty())
                return false; // not a valid row

            struct table_row row;
            row.id = ctx.document.rows.size();
            row.owning_category = category_stack.back();

            const table& tbl = ctx.document.tables.at(static_cast<size_t>(active_table));

            for (size_t i = 0; i < tbl.columns.size(); ++i)
            {
                typed_value tv;

                tv.type           = value_type::unresolved;
                tv.type_source    = tbl.columns[i].type_source;
                tv.origin         = value_locus::table_cell;
                tv.source_literal =
                    (i < cells.size())
                        ? std::string(cells[i])
                        : std::string{};
                tv.val = tv.source_literal.has_value() ? *tv.source_literal : std::string{};

                row.cells.push_back(tv);
            }

            ctx.document.rows.push_back(row);
            ctx.document.tables.at(static_cast<size_t>(active_table)).rows.push_back(row.id);

            ev.kind   = parse_event_kind::table_row;
            ev.target = row.id;

            ctx.document.events.push_back(ev);
            return true;
        }

//---------------------------------------------------------------------------        

        bool parser_impl::key_value(parse_event& ev)
        {
            active_table = invalid_id<table_tag>();

            auto pos = ev.text.find('=');
            if (pos == std::string::npos)
            {
                return false;  // Malformed
            }

            std::string lhs = std::string(trim_sv(ev.text.substr(0, pos)));
            std::string rhs = std::string(trim_sv(ev.text.substr(pos + 1)));

            std::string name;
            std::optional<std::string> declared;

            auto type_pos = lhs.find(':');
            if (type_pos != std::string::npos)
            {
                name = to_lower(lhs.substr(0, type_pos));
                declared = std::string(trim_sv(lhs.substr(type_pos + 1)));
            }
            else
            {
                name = to_lower(lhs);
            }

            cst_key key;
            key.owner         = category_stack.back();
            key.name          = name;
            key.declared_type = declared;
            key.literal       = rhs;
            key.loc           = ev.loc;

            key_id id{ ctx.document.keys.size() };
            ctx.document.keys.push_back(std::move(key));

            ev.kind   = parse_event_kind::key_value;
            ev.target = id;

            ctx.document.events.push_back(ev);
            return true;
        }

    } // anon ns


//========================================================================
// Parser API implementation
//========================================================================

    parse_context parse(const std::string& input)
    {
        parser_impl p;
        p.parse(input);
        return std::move(p.ctx);
    }
    
    parse_context parse(const std::string_view input)
    {
        parser_impl p;
        p.parse(input);
        return std::move(p.ctx);
    }
        

} // namespace arf

#endif // ARF_PARSER_HPP
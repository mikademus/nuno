// arf_parser.hpp - A Readable Format (Arf!) - Parser
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_PARSER_HPP
#define ARF_PARSER_HPP

#include "arf_core.hpp"
#include <cstdlib>
#include <sstream>

namespace arf 
{
//========================================================================
// PARSER API
//========================================================================
    
    parse_context parse(const std::string& input);
    
    
//========================================================================
// Implementation details
//========================================================================
    
    namespace
    {
        using namespace detail;

        struct parser_impl
        {
            parse_context ctx;

            category_id  next_category_id {0};
            table_id     next_table_id {0};
            table_row_id next_row_id {0};

            // Active context
            std::vector<category_id> category_stack;
            table_id active_table {invalid_id<table_tag>()};

            void parse(const std::string& input);
            void add_error(const std::string& message);

            std::vector<std::string> split_lines(const std::string& input);
            std::vector<std::string> split_table_cells(std::string_view line);
            template<typename T, typename F>
            std::vector<T> split_array(const std::string& str, F converter);

            void parse_line(const std::string& line, size_t line_no);
            value_type parse_type(const std::string& type_str);
            std::optional<value> parse_value(const std::string& str, value_type type);

            void create_root_category();

            void open_top_level_category(std::string_view name, parse_event& ev);
            void open_category(std::string_view name, parse_event& ev);
            void close_category(std::string_view, parse_event& ev);

            void key_value(parse_event& ev);

            void start_table(std::string_view header, parse_event& ev);
            void table_row(std::string_view text, parse_event& ev);
        };

//---------------------------------------------------------------------------        

        void parser_impl::parse(const std::string& input)
        {
            std::istringstream ss(input);
            std::string line;
            size_t line_no = 0;

            // Create implicit root category
            create_root_category();

            while (std::getline(ss, line))
            {
                ++line_no;
                parse_line(line, line_no);
            }
        }

//---------------------------------------------------------------------------        

        void parser_impl::add_error(const std::string& message)
        {
            ctx.errors.push_back(parse_error{
                { 0 }, // line tracking can be improved later
                message,
                {}
            });            
        }

//---------------------------------------------------------------------------        

        void parser_impl::create_root_category()
        {
            category root;
            root.id     = next_category_id++;
            root.name   = "__root__";
            root.parent = invalid_id<category_tag>();

            ctx.doc.categories.push_back(root);
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

        value_type parser_impl::parse_type(const std::string& type_str) 
        {
            std::string lower = to_lower(type_str);
            if (lower == "int") return value_type::integer;
            if (lower == "float") return value_type::decimal;
            if (lower == "bool") return value_type::boolean;
            if (lower == "date") return value_type::date;
            if (lower == "str[]") return value_type::string_array;
            if (lower == "int[]") return value_type::int_array;
            if (lower == "float[]") return value_type::float_array;
            if (lower == "str") return value_type::string;
            
            add_error("key has unknown type \"" + type_str + "\" (defaulting to str)");
            return value_type::string;
        }

//---------------------------------------------------------------------------        
    
        template<typename T, typename F>
        std::vector<T> parser_impl::split_array(const std::string& str, F converter) 
        {
            std::vector<T> result;
            std::string current;
            
            for (char c : str) 
            {
                if (c == '|') 
                {
                    if (!current.empty()) 
                    {
                        try {
                            result.push_back(converter(std::string(trim_sv(current))));
                        } catch (...) {
                            // Skip invalid array elements
                        }
                        current.clear();
                    }
                } 
                else 
                {
                    current += c;
                }
            }
            
            if (!current.empty())
            {
                try {
                    result.push_back(converter(std::string(trim_sv(current))));
                } catch (...) {
                    // Skip invalid array elements
                }
            }
            
            return result;
        }

//---------------------------------------------------------------------------        

        std::optional<value> parser_impl::parse_value(const std::string& str, value_type type) 
        {
            try 
            {
                switch (type) 
                {
                    case value_type::integer:
                        return static_cast<int64_t>(std::stoll(str));
                    case value_type::decimal:
                        return std::stod(str);
                    case value_type::boolean:
                    {
                        std::string lower = to_lower(str);
                        return lower == "true" || lower == "yes" || lower == "1";
                    }
                    case value_type::date:
                        return str;
                    case value_type::string_array:
                        return split_array<std::string>(str, [](const std::string& s) { return s; });
                    case value_type::int_array:
                        return split_array<int64_t>(str, [](const std::string& s) { return std::stoll(s); });
                    case value_type::float_array:
                        return split_array<double>(str, [](const std::string& s) { return std::stod(s); });
                    default:
                        return str;
                }
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }
        
//---------------------------------------------------------------------------        

        void parser_impl::parse_line(const std::string& line, size_t line_no)
        {
            std::string_view trimmed = trim_sv(line);

            parse_event ev;
            ev.loc.line = line_no;
            ev.text     = line;

            if (trimmed.empty())
            {
                ev.kind = parse_event_kind::empty_line;
                ctx.doc.events.push_back(ev);
                return;
            }

            if (trimmed.starts_with("//"))
            {
                ev.kind = parse_event_kind::comment;
                ctx.doc.events.push_back(ev);
                return;
            }

            if (trimmed.starts_with(":"))
            {
                open_category(trimmed.substr(1), ev);
                return;
            }

            if (trimmed.starts_with("/") && trimmed.size() > 1)
            {
                close_category(trimmed.substr(1), ev);
                return;
            }

            if (trimmed.ends_with(":"))
            {
                open_top_level_category(trimmed.substr(0, trimmed.size() - 1), ev);
                return;
            }

            if (trimmed.starts_with("#"))
            {
                start_table(trimmed.substr(1), ev);
                return;
            }

            if (trimmed.find('=') != std::string_view::npos)
            {
                key_value(ev);
                return;
            }

            if (active_table != invalid_id<table_tag>())
            {
                table_row(trimmed, ev);
                return;
            }

            ev.kind = parse_event_kind::invalid;
            ctx.doc.events.push_back(ev);
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
            cat.id     = next_category_id++;
            cat.name   = to_lower(std::string(trim_sv(name)));
            cat.parent = category_stack.back();

            ctx.doc.categories.push_back(cat);
            category_stack.push_back(cat.id);

            ev.kind   = parse_event_kind::category_open;
            ev.target = cat.id;

            ctx.doc.events.push_back(ev);
        }

//---------------------------------------------------------------------------        

        void parser_impl::close_category(std::string_view, parse_event& ev)
        {
            if (category_stack.size() <= 1)
            {
                ev.kind = parse_event_kind::invalid;
                ctx.doc.events.push_back(ev);
                return;
            }

            category_id closing = category_stack.back();
            category_stack.pop_back();

            active_table = invalid_id<table_tag>();

            ev.kind   = parse_event_kind::category_close;
            ev.target = closing;

            ctx.doc.events.push_back(ev);
        }

//---------------------------------------------------------------------------        

        void parser_impl::start_table(std::string_view header, parse_event& ev)
        {
            table tbl;
            tbl.id             = next_table_id++;
            tbl.owning_category = category_stack.back();

            auto cols = split_table_cells(header);
            for (auto& c : cols)
            {
                column col;
                auto pos = c.find(':');
                if (pos != std::string::npos)
                {
                    col.name = to_lower(c.substr(0, pos));
                    col.type = parse_type(c.substr(pos + 1));
                    col.type_source = type_ascription::declared;
                }
                else
                {
                    col.name = to_lower(c);
                    col.type = value_type::string;
                    col.type_source = type_ascription::tacit;
                }
                tbl.columns.push_back(col);
            }

            ctx.doc.tables.push_back(tbl);
            active_table = tbl.id;

            ev.kind   = parse_event_kind::table_header;
            ev.target = tbl.id;

            ctx.doc.events.push_back(ev);
        }

//---------------------------------------------------------------------------        

        void parser_impl::table_row(std::string_view text, parse_event& ev)
        {
            auto cells = split_table_cells(text);
            if (cells.empty())
            {
                ev.kind = parse_event_kind::invalid;
                ctx.doc.events.push_back(ev);
                return;
            }

            struct table_row row;
            row.id = next_row_id++;
            row.owning_category = category_stack.back();

            const table& tbl = ctx.doc.tables.at(static_cast<size_t>(active_table));

            for (size_t i = 0; i < tbl.columns.size(); ++i)
            {
                typed_value tv;
                tv.type        = tbl.columns[i].type;
                tv.type_source = tbl.columns[i].type_source;
                tv.origin      = value_locus::table_cell;

                if (i < cells.size())
                {
                    tv.source_literal = cells[i];
                    auto parsed = parse_value(cells[i], tv.type);
                    tv.val = parsed.value_or(std::string{});
                }
                else
                {
                    tv.source_literal = std::nullopt;
                    tv.val = std::string{};
                }

                row.cells.push_back(tv);
            }

            ctx.doc.rows.push_back(row);
            ctx.doc.tables.at(static_cast<size_t>(active_table)).rows.push_back(row.id);

            ev.kind   = parse_event_kind::table_row;
            ev.target = row.id;

            ctx.doc.events.push_back(ev);
        }

//---------------------------------------------------------------------------        

        void parser_impl::key_value(parse_event& ev)
        {
            active_table = invalid_id<table_tag>();

            ev.kind   = parse_event_kind::key_value;
            ev.target = category_stack.back();

            ctx.doc.events.push_back(ev);
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
        
} // namespace arf

#endif // ARF_PARSER_HPP
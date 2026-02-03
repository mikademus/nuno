// arf_serializer.hpp - A Readable Format (Arf!) - Serializer
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_SERIALIZER_HPP
#define ARF_SERIALIZER_HPP

#include "arf_document.hpp"
#include <ostream>
#include <cassert>

namespace arf
{

//========================================================================
// SERIALIZER_OPTIONS
//========================================================================

    struct serializer_options
    {
        enum class type_policy
        {
            preserve,        // Emit types as declared in source
            force_tacit,     // Strip all type annotations
            force_explicit   // Force all values to show types
        } types = type_policy::preserve;

        enum class blank_line_policy
        {
            preserve,   // Emit paragraph events as-is (may include empty lines)
            compact,    // Skip empty paragraph events
            readable    // Add strategic blank lines (after categories, tables)
        } blank_lines = blank_line_policy::preserve;

        bool emit_comments = true;      // If false, skip comment events
        bool emit_paragraphs = true;    // If false, skip paragraph events
    };

//========================================================================
// SERIALIZER
//========================================================================

    class serializer
    {
    public:
        serializer(std::ostream& out, serializer_options opts = {})
            : out_(out), opts_(opts)
        {
        }

        document::root_node const & root(const document& doc) { return doc.root_; }

        void write(const document& doc)
        {
std::cout << "write: #items in root = " << doc.root_.ordered_items.size() << std::endl;
            // Walk root's ordered_items
            for (const auto& item : doc.root_.ordered_items)
            {
                write_source_item(doc, item);
            }
        }

    private:
        std::ostream&      out_;
        serializer_options opts_;
        size_t             indent_ {0};  // Current indentation level

    private:
        //----------------------------------------------------------------
        // Source item dispatcher
        //----------------------------------------------------------------

        void write_source_item(const document& doc, const document::source_item_ref& ref)
        {
std::cout << " - write_source_item\n";
            std::visit([&](auto&& id) { write_item(doc, id); }, ref.id);
        }

        //----------------------------------------------------------------
        // Item writers (overloaded on ID type)
        //----------------------------------------------------------------

        void write_item(const document& doc, key_id id)
        {
std::cout << " - writing key\n";
            auto it = doc.find_node_by_id(doc.keys_, id);
            assert(it != doc.keys_.end());
            write_key(*it);
        }

        void write_item(const document& doc, category_id id)
        {
std::cout << " - writing cat\n";
            auto it = doc.find_node_by_id(doc.categories_, id);
            assert(it != doc.categories_.end());
            write_category_open(doc, *it);
        }

        void write_item(const document& doc, const document::category_close_marker& marker)
        {
std::cout << " - writing cat close\n";
            write_category_close(doc, marker);
        }

        void write_item(const document& doc, table_id id)
        {
std::cout << " - writing table\n";
            auto it = doc.find_node_by_id(doc.tables_, id);
            assert(it != doc.tables_.end());
            write_table(doc, *it);
        }

        void write_item(const document& doc, table_row_id id)
        {
std::cout << "  - writing row\n";
            auto it = doc.find_node_by_id(doc.rows_, id);
            assert(it != doc.rows_.end());
            write_row(doc, *it);
        }

        void write_item(const document& doc, comment_id id)
        {
std::cout << " - writing comment\n";
            if (!opts_.emit_comments)
                return;

            auto it = std::ranges::find_if(doc.comments_,
                [id](auto const& c) { return c.id == id; });
            assert(it != doc.comments_.end());

            write_comment(*it);
        }

        void write_item(const document& doc, paragraph_id id)
        {
std::cout << " - writing paragraph\n";
            if (!opts_.emit_paragraphs)
                return;

            auto it = std::ranges::find_if(doc.paragraphs_,
                [id](auto const& p) { return p.id == id; });
            assert(it != doc.paragraphs_.end());

            write_paragraph(*it);
        }

        //----------------------------------------------------------------
        // Key
        //----------------------------------------------------------------

        void write_key(const document::key_node& k)
        {
            write_indent();
            out_ << k.name;

            // Type annotation
            if (should_emit_type(k.type_source, k.value.type))
            {
                out_ << ":" << type_string(k.value.type);
            }

            out_ << " = ";
            write_value(k.value);
            out_ << '\n';

            std::cout << "key/value: " << k.name << std::endl;
        }

        //----------------------------------------------------------------
        // Category open
        //----------------------------------------------------------------

        void write_category_open(const document& doc, const document::category_node& cat)
        {
            bool is_root = (cat.id == category_id{0});

            if (is_root)
            {
                // Root category: just emit its contents (no header)
                write_category_contents(doc, cat);
                return;
            }

            // Determine if top-level or subcategory
            bool is_top_level = false;
            if (cat.parent == category_id{0})
                is_top_level = true;

            if (is_top_level)
            {
                write_indent();
                out_ << cat.name << ":\n";
                ++indent_;
            }
            else
            {
                write_indent();
                out_ << ":" << cat.name << '\n';
                ++indent_;
            }

            write_category_contents(doc, cat);
        }

        void write_category_contents(const document& doc, const document::category_node& cat)
        {
            for (const auto& item : cat.ordered_items)
            {
                write_source_item(doc, item);
            }
        }

        //----------------------------------------------------------------
        // Category close
        //----------------------------------------------------------------

        void write_category_close(const document& doc, const document::category_close_marker& marker)
        {
            --indent_;
            write_indent();

            if (marker.form == document::category_close_form::shorthand)
            {
                out_ << "/\n";
            }
            else
            {
                // Named close: retrieve category name
                auto it = doc.find_node_by_id(doc.categories_, marker.which);
                assert(it != doc.categories_.end());
                out_ << "/" << it->name << '\n';
            }
        }

        //----------------------------------------------------------------
        // Table
        //----------------------------------------------------------------

        void write_table(const document& doc, const document::table_node& tbl)
        {
            // Emit header. 
            write_indent();
            
            // It is important to have at least one space after the table #
            // to allow possible table names in future extensions.
            out_ << "# ";

            bool first = true;
            for (auto col_id : tbl.columns)
            {
                auto it = doc.find_node_by_id(doc.columns_, col_id);
                assert(it != doc.columns_.end());

                if (!first)
                    out_ << "  ";

                out_ << it->col.name;

                if (should_emit_type(it->col.type_source, it->col.type))
                {
                    out_ << ":" << type_string(it->col.type);
                }

                first = false;
            }

            out_ << '\n';

            // Emit table contents (rows, comments, paragraphs, subcategories)
            for (const auto& item : tbl.ordered_items)
            {
                write_source_item(doc, item);
            }
        }

        //----------------------------------------------------------------
        // Table row
        //----------------------------------------------------------------

        void write_row(const document& doc, const document::row_node& row)
        {
            write_indent();

            bool first = true;
            for (const auto& cell : row.cells)
            {
                if (first)
                    out_ << std::string(indent_ * 4 + 2, ' ');
                else
                    out_ << "  ";

                write_value(cell);
                first = false;
            }

            out_ << '\n';
        }

        //----------------------------------------------------------------
        // Comment
        //----------------------------------------------------------------

        void write_comment(const document::comment_node& c)
        {
            // Comments are stored verbatim with leading whitespace and "//"
            // Just emit as-is
            out_ << c.text << '\n';
        }

        //----------------------------------------------------------------
        // Paragraph
        //----------------------------------------------------------------

        void write_paragraph(const document::paragraph_node& p)
        {
            if (opts_.blank_lines == serializer_options::blank_line_policy::compact
                && p.text.empty())
            {
                return;  // Skip empty paragraphs in compact mode
            }

            // Paragraphs stored verbatim with leading whitespace
            out_ << p.text << '\n';
        }

        //----------------------------------------------------------------
        // Value emission
        //----------------------------------------------------------------

        void write_value(const typed_value& tv)
        {
            // Emit from source literal if available (preserves authorship)
            if (tv.source_literal.has_value())
            {
                out_ << *tv.source_literal;
                return;
            }

            // Fallback: semantic reconstruction
            write_value_semantic(tv);
        }

        void write_value_semantic(const typed_value& tv)
        {
            switch (tv.type)
            {
                case value_type::string:
                    out_ << std::get<std::string>(tv.val);
                    break;

                case value_type::integer:
                    out_ << std::get<int64_t>(tv.val);
                    break;

                case value_type::decimal:
                    out_ << std::get<double>(tv.val);
                    break;

                case value_type::boolean:
                    out_ << (std::get<bool>(tv.val) ? "true" : "false");
                    break;

                case value_type::string_array:
                case value_type::int_array:
                case value_type::float_array:
                {
                    auto const& arr = std::get<std::vector<typed_value>>(tv.val);
                    bool first = true;
                    for (auto const& elem : arr)
                    {
                        if (!first)
                            out_ << '|';
                        write_value(elem);
                        first = false;
                    }
                    break;
                }

                case value_type::unresolved:
                    // Empty cell or missing value
                    break;

                default:
                    out_ << std::get<std::string>(tv.val);
                    break;
            }
        }

        //----------------------------------------------------------------
        // Type policy
        //----------------------------------------------------------------

        bool should_emit_type(type_ascription source, value_type type) const
        {
            switch (opts_.types)
            {
                case serializer_options::type_policy::force_tacit:
                    return false;

                case serializer_options::type_policy::force_explicit:
                    return type != value_type::unresolved;

                case serializer_options::type_policy::preserve:
                default:
                    return source == type_ascription::declared;
            }
        }

        std::string_view type_string(value_type type) const
        {
            switch (type)
            {
                case value_type::string:       return "str";
                case value_type::integer:      return "int";
                case value_type::decimal:      return "float";
                case value_type::boolean:      return "bool";
                case value_type::date:         return "date";
                case value_type::string_array: return "str[]";
                case value_type::int_array:    return "int[]";
                case value_type::float_array:  return "float[]";
                default:                       return "str";
            }
        }

        //----------------------------------------------------------------
        // Indentation
        //----------------------------------------------------------------

        void write_indent()
        {
            for (size_t i = 0; i < indent_; ++i)
                out_ << "    ";  // 4 spaces per level
        }
    };

} // namespace arf

#endif // ARF_SERIALIZER_HPP
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

    #define DBG_EMIT std::cout << "[S] "

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

        bool emit_comments {true};      // If false, skip comment events
        bool emit_paragraphs {true};    // If false, skip paragraph events

        bool echo_lines  {false};       // prints each node to be serialised
    };

//========================================================================
// SERIALIZER
//========================================================================

    class serializer
    {
    public:
        serializer(const document& doc, serializer_options opts = {})
            : doc_(doc), opts_(opts)
        {
        }

        void write(std::ostream& out)
        {
            out_ = &out;
            write_category_open(doc_.categories_.front());            
        }

    private:
        const document&    doc_;
        std::ostream*      out_;        
        serializer_options opts_;
        size_t             indent_ {0};  // Current indentation level

    private:
        //----------------------------------------------------------------
        // Source item dispatcher
        //----------------------------------------------------------------

        void write_source_item(const document::source_item_ref& ref)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_source_item\n";
            
            std::visit([&](auto&& id) { write_item(id); }, ref.id);
        }

        //----------------------------------------------------------------
        // Item writers (overloaded on ID type)
        //----------------------------------------------------------------

        void write_item(key_id id)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_item(keyid)\n";

            auto it = doc_.find_node_by_id(doc_.keys_, id);
            assert(it != doc_.keys_.end());
            write_key(*it);
        }

        void write_item(category_id id)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_item(category_id)\n";

            auto it = doc_.find_node_by_id(doc_.categories_, id);
            assert(it != doc_.categories_.end());
            write_category_open(*it);
        }

        void write_item(const document::category_close_marker& marker)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_item(category_close_id)\n";

            write_category_close(marker);
        }

        void write_item(table_id id)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_item(table_id)\n";

            auto it = doc_.find_node_by_id(doc_.tables_, id);
            assert(it != doc_.tables_.end());
            write_table(*it);
        }

        void write_item(table_row_id id)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_item(table_row_id)\n";

            auto it = doc_.find_node_by_id(doc_.rows_, id);
            assert(it != doc_.rows_.end());
            write_row(*it);
        }

        void write_item(comment_id id)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_item(comment_id{" << id.val << "})\n";

            if (!opts_.emit_comments)
                return;

            auto it = std::ranges::find_if(doc_.comments_,
                [id](auto const& c) { return c.id == id; });
            
            if (it == doc_.comments_.end())
            {
                DBG_EMIT << "ERROR: comment_id{" << id.val << "} not found!\n";
                return;
            }

            write_comment(*it);
        }

        void write_item(paragraph_id id)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_item(paragraph_id)\n";

            if (!opts_.emit_paragraphs)
                return;

            auto it = std::ranges::find_if(doc_.paragraphs_,
                [id](auto const& p) { return p.id == id; });
            assert(it != doc_.paragraphs_.end());

            write_paragraph(*it);
        }

//----------------------------------------------------------------
// Key
//----------------------------------------------------------------

        void write_key(const document::key_node& k)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_key\n";

            bool force_reconstruct = 
                (opts_.types != serializer_options::type_policy::preserve);

            // Can replay source?
            bool can_replay = (k.creation == creation_state::authored)
                            && !k.is_edited
                            && !force_reconstruct
                            && k.source_event_index.has_value()
                            && doc_.source_context_;

            if (can_replay)
            {
                const auto& event = doc_.source_context_->document.events[*k.source_event_index];
                *out_ << event.text << '\n';
                return;
            }

            // Reconstruct
            write_indent();
            *out_ << k.name;
            
            if (should_emit_type(k.value.type_source, k.value.type))
                *out_ << ":" << type_string(k.value.type);
            
            *out_ << " = ";
            write_value(k.value);
            *out_ << '\n';
        }

//----------------------------------------------------------------
// Category open
//----------------------------------------------------------------

        void write_category_open(const document::category_node& cat)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_category_open\n";

            bool is_root = (cat.id == category_id{0});

            if (is_root)
            {
                write_category_contents(cat);
                return;
            }

            // Can replay source?
            bool can_replay = (cat.creation == creation_state::authored)
                            && !cat.is_edited
                            && cat.source_event_index_open.has_value()
                            && doc_.source_context_;

            if (can_replay)
            {
                const auto& event = doc_.source_context_->document.events[*cat.source_event_index_open];
                *out_ << event.text << '\n';
                ++indent_;
                write_category_contents(cat);
                return;
            }

            // Reconstruct
            bool is_top_level = (cat.parent == category_id{0});

            if (is_top_level)
            {
                write_indent();
                *out_ << cat.name << ":\n";
                ++indent_;
            }
            else
            {
                write_indent();
                *out_ << ":" << cat.name << '\n';
                ++indent_;
            }

            write_category_contents(cat);
        }

//----------------------------------------------------------------
// Category contents
//----------------------------------------------------------------

        void write_category_contents(const document::category_node& cat)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_category_contents\n";

            for (const auto& item : cat.ordered_items)
            {
                write_source_item(item);
            }
        }

//----------------------------------------------------------------
// Category close
//----------------------------------------------------------------

        void write_category_close(const document::category_close_marker& marker)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_category_close\n";

            --indent_;

            auto it = doc_.find_node_by_id(doc_.categories_, marker.which);
            assert(it != doc_.categories_.end());
            const auto& cat = *it;

            // Can replay source?
            bool can_replay = (cat.creation == creation_state::authored)
                            && !cat.is_edited
                            && cat.source_event_index_close.has_value()
                            && doc_.source_context_;

            if (can_replay)
            {
                const auto& event = doc_.source_context_->document.events[*cat.source_event_index_close];
                *out_ << event.text << '\n';
                return;
            }

            // Reconstruct
            write_indent();
            if (marker.form == document::category_close_form::shorthand)
                *out_ << "/\n";
            else
                *out_ << "/" << cat.name << '\n';
        }

//----------------------------------------------------------------
// Table
//----------------------------------------------------------------

        void write_table(const document::table_node& tbl)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_table\n";

            bool force_reconstruct = 
                (opts_.types != serializer_options::type_policy::preserve);

            // Can replay source?
            bool can_replay = (tbl.creation == creation_state::authored)
                            && !tbl.is_edited
                            && !force_reconstruct
                            && tbl.source_event_index.has_value()
                            && doc_.source_context_;

            if (can_replay)
            {
                const auto& event = doc_.source_context_->document.events[*tbl.source_event_index];
                *out_ << event.text << '\n';
            }
            else
            {
                // Reconstruct header
                write_indent();
                *out_ << "# ";

                bool first = true;
                for (auto col_id : tbl.columns)
                {
                    auto it = doc_.find_node_by_id(doc_.columns_, col_id);
                    assert(it != doc_.columns_.end());

                    if (!first)
                        *out_ << "  ";

                    *out_ << it->col.name;

                    if (should_emit_type(it->col.type_source, it->col.type))
                        *out_ << ":" << type_string(it->col.type);

                    first = false;
                }

                *out_ << '\n';
            }

            // Emit table contents
            for (const auto& item : tbl.ordered_items)
                write_source_item(item);
        }

//----------------------------------------------------------------
// Table row
//----------------------------------------------------------------

        void write_row(const document::row_node& row)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_row\n";

            // Can replay source?
            bool can_replay = (row.creation == creation_state::authored)
                            && !row.is_edited
                            && row.source_event_index.has_value()
                            && doc_.source_context_;

            if (can_replay)
            {
                const auto& event = doc_.source_context_->document.events[*row.source_event_index];
                *out_ << event.text;
                
                if (!event.text.empty() && event.text.back() != '\n')
                    *out_ << '\n';
                
                return;
            }

            // Reconstruct
            write_indent();
            *out_ << "  ";  // Table row base indentation

            bool first = true;
            for (const auto& cell : row.cells)
            {
                if (!first)
                    *out_ << "  ";
                
                write_value(cell);
                first = false;
            }

            *out_ << '\n';
        }        

//----------------------------------------------------------------
// Comment
//----------------------------------------------------------------

        void write_comment(const document::comment_node& c)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_comment\n";

            // Comments are always emitted verbatim from stored text
            // (They don't have edit tracking - if you change a comment, you change its .text)
            *out_ << c.text << '\n';
        }

//----------------------------------------------------------------
// Paragraph
//----------------------------------------------------------------

        void write_paragraph(const document::paragraph_node& p)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_paragraph\n";

            if (opts_.blank_lines == serializer_options::blank_line_policy::compact
                && trim_sv(p.text).empty())
            {
                return;  // Skip empty paragraphs in compact mode
            }

            // Paragraphs are always emitted verbatim from stored text
            *out_ << p.text << '\n';
        }

//----------------------------------------------------------------
// Value emission
//----------------------------------------------------------------

        std::string variant_to_string(const value& v)
        {
            return std::visit([](auto&& val) -> std::string
            {
                using T = std::decay_t<decltype(val)>;
                
                if constexpr (std::is_same_v<T, std::string>)
                    return val;
                else if constexpr (std::is_same_v<T, int64_t>)
                    return std::to_string(val);
                else if constexpr (std::is_same_v<T, double>)
                    return std::to_string(val);
                else if constexpr (std::is_same_v<T, bool>)
                    return val ? "true" : "false";
                else
                    return "";
            }, v);
        }

        std::optional<int64_t> variant_to_int(const value& v)
        {
            return std::visit([](auto&& val) -> std::optional<int64_t>
            {
                using T = std::decay_t<decltype(val)>;
                
                if constexpr (std::is_same_v<T, int64_t>)
                    return val;
                else if constexpr (std::is_same_v<T, double>)
                    return static_cast<int64_t>(val);
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    int64_t result;
                    auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), result);
                    if (ec == std::errc{})
                        return result;
                }
                else if constexpr (std::is_same_v<T, bool>)
                    return val ? 1 : 0;
                
                return std::nullopt;
            }, v);
        }

        std::optional<double> variant_to_double(const value& v)
        {
            return std::visit([](auto&& val) -> std::optional<double>
            {
                using T = std::decay_t<decltype(val)>;
                
                if constexpr (std::is_same_v<T, double>)
                    return val;
                else if constexpr (std::is_same_v<T, int64_t>)
                    return static_cast<double>(val);
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    char* end;
                    double result = std::strtod(val.data(), &end);
                    if (end == val.data() + val.size())
                        return result;
                }
                else if constexpr (std::is_same_v<T, bool>)
                    return val ? 1.0 : 0.0;
                
                return std::nullopt;
            }, v);
        }

        bool variant_to_bool(const value& v)
        {
            return std::visit([](auto&& val) -> bool
            {
                using T = std::decay_t<decltype(val)>;
                
                if constexpr (std::is_same_v<T, bool>)
                    return val;
                else if constexpr (std::is_same_v<T, int64_t>)
                    return val != 0;
                else if constexpr (std::is_same_v<T, double>)
                    return val != 0.0;
                else if constexpr (std::is_same_v<T, std::string>)
                    return !val.empty() && val != "false" && val != "0";
                else
                    return false;
            }, v);
        }

        void write_value(const typed_value& tv)
        {
            // For cells and keys, we'd need to know their parent node to check source
            // Since we don't have that context here, always reconstruct values
            // (Authored preservation happens at the node level: keys, rows)
            write_value_semantic(tv);
        }

        void write_value_semantic(const typed_value& tv)
        {
            // Check if variant matches declared type
            bool matches = value_type_to_variant_index[static_cast<size_t>(tv.type)] 
                        == tv.val.index();
            
            if (!matches && tv.type_source == type_ascription::declared)
            {
                // DECLARED type - convert variant to match declaration
                write_converted_to_type(tv.val, tv.type);
                return;
            }
            
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_value_semantic\n";

            // TACIT type OR already matching - emit actual variant contents
            std::visit([this](auto&& value)
            {
                using T = std::decay_t<decltype(value)>;
                
                if constexpr (std::is_same_v<T, std::monostate>)
                {
                    // Empty
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    *out_ << value;
                }
                else if constexpr (std::is_same_v<T, int64_t>)
                {
                    *out_ << value;
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    *out_ << value;
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    *out_ << (value ? "true" : "false");
                }
                else if constexpr (std::is_same_v<T, std::vector<typed_value>>)
                {
                    bool first = true;
                    for (auto const& elem : value)
                    {
                        if (!first) *out_ << '|';
                        write_value(elem);
                        first = false;
                    }
                }
            }, tv.val);
        }

        void write_converted_to_type(const value& v, value_type target)
        {
            if (opts_.echo_lines)
                DBG_EMIT << "serializer::write_converted_to_type\n";

            switch (target)
            {
                case value_type::string:
                    *out_ << variant_to_string(v);
                    break;
                    
                case value_type::integer:
                    if (auto i = variant_to_int(v))
                        *out_ << *i;
                    else
                        *out_ << "0";  // Fallback for unconvertible
                    break;
                    
                case value_type::decimal:
                    if (auto d = variant_to_double(v))
                        *out_ << *d;
                    else
                        *out_ << "0.0";
                    break;
                    
                case value_type::boolean:
                    *out_ << (variant_to_bool(v) ? "true" : "false");
                    break;
                    
                default:
                    *out_ << variant_to_string(v);  // Fallback
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
                *out_ << "    ";  // 4 spaces per level
        }
    };

    #undef DBG_EMIT

} // namespace arf

#endif // ARF_SERIALIZER_HPP
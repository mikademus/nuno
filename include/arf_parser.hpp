// arf_parser.hpp - A Readable Format (Arf!) - Parser
// Version 0.2.0

#ifndef ARF_PARSER_HPP
#define ARF_PARSER_HPP

#include "arf_core.hpp"

namespace arf 
{
    //========================================================================
    // PARSER API
    //========================================================================
    
    parse_context parse(const std::string& input);
    
    //========================================================================
    // PARSER IMPLEMENTATION
    //========================================================================
    
    namespace detail 
    {
        class parser_impl 
        {
        public:
            parse_context parse(const std::string& input) 
            {
                lines_ = split_lines(input);
                
                if (lines_.size() > MAX_LINES)
                {
                    errors_.push_back({0, "File exceeds maximum line limit", ""});
                    return parse_context{std::nullopt, std::move(errors_)};
                }
                
                line_num_ = 0;
                doc_ = document{};
                category_stack_.clear();
                current_table_.clear();
                in_table_mode_ = false;
                table_mode_depth_ = 0;
                errors_.clear();
                
                // Create implicit root category for pre-category definitions
                auto root = std::make_unique<category>();
                root->name = std::string(ROOT_CATEGORY_NAME);
                category* root_ptr = root.get();
                doc_.categories[std::string(ROOT_CATEGORY_NAME)] = std::move(root);
                category_stack_.push_back(root_ptr);
                
                while (line_num_ < lines_.size()) 
                {
                    parse_line(lines_[line_num_]);
                    ++line_num_;
                }
                
                if (!errors_.empty())
                    return parse_context{std::nullopt, std::move(errors_)};
                
                return parse_context{std::move(doc_), {}};
            }
            
        private:
            std::vector<std::string> lines_;
            size_t line_num_ = 0;
            document doc_;
            std::vector<category*> category_stack_;
            std::vector<column> current_table_;
            bool in_table_mode_ = false;
            size_t table_mode_depth_ = 0;
            std::vector<parse_error> errors_;
            
            std::vector<std::string> split_lines(const std::string& input) 
            {
                std::vector<std::string> result;
                std::istringstream stream(input);
                std::string line;
                while (std::getline(stream, line))
                    result.push_back(line);
                return result;
            }
            
            void add_error(const std::string& message)
            {
                errors_.push_back({line_num_ + 1, message, lines_[line_num_]});
            }
            
            void parse_line(const std::string& line) 
            {
                std::string_view trimmed = trim_sv(line);
                
                if (trimmed.empty()) 
                {
                    return;
                }
                
                // Comments
                if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/')
                    return;
                
                // Subcategory close (must check before top-level category to handle /category:)
                if (trimmed[0] == '/' && trimmed.size() > 1 && trimmed[1] != '/') 
                {
                    parse_subcategory_close(trimmed);
                    return;
                }
                
                // Top-level category
                if (!trimmed.empty() && trimmed.back() == ':' && trimmed[0] != ':' && trimmed[0] != '#') 
                {
                    parse_top_level_category(trimmed);
                    return;
                }
                
                // Subcategory open
                if (trimmed[0] == ':') 
                {
                    parse_subcategory_open(trimmed);
                    return;
                }
                
                // Table header
                if (trimmed[0] == '#') 
                {
                    parse_table_header(trimmed);
                    return;
                }
                
                // Key-value pair (check BEFORE table rows, even in table mode)
                // This allows subcategories to override table mode with key-value pairs
                if (trimmed.find('=') != std::string_view::npos) 
                {
                    parse_key_value(trimmed);
                    return;
                }
                
                // Table row
                if (in_table_mode_ ||
                    (!category_stack_.empty() &&
                    !category_stack_.back()->table_columns.empty()))
                {
                    parse_table_row(trimmed);
                    return;
                }                
                
                // Unrecognized line
                add_error("Unrecognized line syntax");
            }
            
            void parse_top_level_category(std::string_view line) 
            {
                std::string_view name_view = line.substr(0, line.size() - 1);
                std::string name = to_lower(std::string(trim_sv(name_view)));
                
                // Close all subcategories and return to root
                category_stack_.clear();
                in_table_mode_ = false;
                current_table_.clear();
                table_mode_depth_ = 0;
                
                auto cat = std::make_unique<category>();
                cat->name = name;
                category* ptr = cat.get();
                doc_.categories[name] = std::move(cat);
                category_stack_.push_back(ptr);
            }
            
            void parse_subcategory_open(std::string_view line) 
            {
                if (category_stack_.empty()) 
                {
                    add_error("Subcategory without parent category");
                    return;
                }
                
                std::string name = to_lower(std::string(trim_sv(line.substr(1))));
                
                category* parent = category_stack_.back();
                auto subcat = std::make_unique<category>();
                subcat->name = name;
                                
                category* ptr = subcat.get();
                parent->subcategories[name] = std::move(subcat);
                category_stack_.push_back(ptr);
                
                // When entering a subcategory, we stay in table mode IF the parent was in table mode
                // But the subcategory can override this with its own # header or key=value pairs
            }
            
            void parse_subcategory_close(std::string_view line) 
            {
                std::string name = to_lower(std::string(trim_sv(line.substr(1))));
                
                // Check if this is a top-level category close (starts with /)
                // If we're at depth 1 (only root in stack), this closes the current top-level category
                if (category_stack_.size() == 1)
                {
                    // Closing a top-level category - this is fine, just ignore it
                    // The next top-level category will reset the stack anyway
                    in_table_mode_ = false;
                    current_table_.clear();
                    table_mode_depth_ = 0;
                    return;
                }
                
                // We're in a subcategory - try to close it
                if (name.empty() || category_stack_.back()->name == name) 
                {
                    category_stack_.pop_back();
                    
                    // Exit table mode if we're back at the level where table was defined
                    if (in_table_mode_ && category_stack_.size() < table_mode_depth_)
                    {
                        in_table_mode_ = false;
                        current_table_.clear();
                        table_mode_depth_ = 0;
                    }
                } 
                else 
                {
                    // Try to find matching name in stack
                    bool found = false;
                    for (auto it = category_stack_.rbegin(); it != category_stack_.rend(); ++it) 
                    {
                        if ((*it)->name == name) 
                        {
                            size_t pos = std::distance(category_stack_.begin(), it.base()) - 1;
                            category_stack_.erase(category_stack_.begin() + pos + 1, category_stack_.end());
                            found = true;
                            
                            // Exit table mode if appropriate
                            if (in_table_mode_ && category_stack_.size() < table_mode_depth_)
                            {
                                in_table_mode_ = false;
                                current_table_.clear();
                                table_mode_depth_ = 0;
                            }
                            break;
                        }
                    }
                    if (!found)
                    {
                        add_error("No matching subcategory found for closure: " + name);
                        if (category_stack_.size() > 1)
                            category_stack_.pop_back();
                    }
                }
            }
            
            void parse_table_header(std::string_view line) 
            {
                current_table_.clear();
                std::string_view header = trim_sv(line.substr(1));
                
                std::istringstream stream{std::string(header)};
                std::string token;
                while (stream >> token) 
                {
                    column col;
                    size_t colon_pos = token.find(':');
                    if (colon_pos != std::string::npos) 
                    {
                        col.name = to_lower(token.substr(0, colon_pos));
                        col.type = parse_type(token.substr(colon_pos + 1));
                    } 
                    else 
                    {
                        col.name = to_lower(token);
                        col.type = value_type::string;
                    }
                    current_table_.push_back(col);
                }
                
                in_table_mode_ = true;
                table_mode_depth_ = category_stack_.size();
                
                if (!category_stack_.empty()) 
                    category_stack_.back()->table_columns = current_table_;
            }
            
            value_type parse_type(const std::string& type_str) 
            {
                std::string lower = to_lower(type_str);
                if (lower == "int") return value_type::integer;
                if (lower == "float") return value_type::decimal;
                if (lower == "bool") return value_type::boolean;
                if (lower == "date") return value_type::date;
                if (lower == "str[]") return value_type::string_array;
                if (lower == "int[]") return value_type::int_array;
                if (lower == "float[]") return value_type::float_array;
                return value_type::string;
            }
            
            void parse_key_value(std::string_view line) 
            {
                size_t eq_pos = line.find('=');
                if (eq_pos == std::string_view::npos) return;
                
                std::string_view key_part = trim_sv(line.substr(0, eq_pos));
                std::string_view val_str = trim_sv(line.substr(eq_pos + 1));
                
                // Check for type annotation
                size_t colon_pos = key_part.find(':');
                std::string key;
                value_type type = value_type::string;
                
                if (colon_pos != std::string_view::npos) 
                {
                    key = to_lower(std::string(trim_sv(key_part.substr(0, colon_pos))));
                    type = parse_type(std::string(trim_sv(key_part.substr(colon_pos + 1))));
                } 
                else 
                {
                    key = to_lower(std::string(key_part));
                }
                
                // If we see a key-value pair, we're no longer in table mode
                // This handles the case where a subcategory has key-value pairs
                // after inheriting a table structure
                if (in_table_mode_)
                {
                    in_table_mode_ = false;
                    current_table_.clear(); // Clear the parser's table state
                    // Clear the inherited table structure from current category
                    if (!category_stack_.empty())
                        category_stack_.back()->table_columns.clear();
                }
                
                // Parse and store typed value
                if (!category_stack_.empty())
                {
                    auto val = parse_value(std::string(val_str), type);
                    if (val.has_value())
                        category_stack_.back()->key_values[key] = std::move(*val);
                    else
                        add_error("Failed to parse value for key: " + key);
                }
            }
            
            void parse_table_row(std::string_view line) 
            {
                if (current_table_.empty() || category_stack_.empty()) return;
                
                std::vector<std::string> cells = split_table_cells(line);
                
                // If split_table_cells returned empty, this isn't a valid table row
                if (cells.empty()) return;
                
                if (cells.size() != current_table_.size())
                {
                    add_error("Row has " + std::to_string(cells.size()) + " cells but expected " + 
                             std::to_string(current_table_.size()));
                    
                    // Pad or truncate
                    while (cells.size() < current_table_.size()) 
                        cells.push_back("");
                    if (cells.size() > current_table_.size()) 
                        cells.resize(current_table_.size());
                }
                
                table_row row;
                for (size_t i = 0; i < cells.size(); i++)
                {
                    auto val = parse_value(cells[i], current_table_[i].type);
                    if (val.has_value())
                        row.push_back(std::move(*val));
                    else
                    {
                        add_error("Failed to parse cell " + std::to_string(i) + " as " + 
                                 type_to_string(current_table_[i].type));
                        row.push_back(std::string{""}); // Use braced initialization
                    }
                }
                
                category_stack_.back()->table_rows.push_back(std::move(row));
            }
            
            std::vector<std::string> split_table_cells(std::string_view line) 
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
            
            std::optional<value> parse_value(const std::string& str, value_type type) 
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
            
            template<typename T, typename F>
            std::vector<T> split_array(const std::string& str, F converter) 
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
        };
        
    } // namespace detail
    
    //========================================================================
    // PUBLIC PARSER API IMPLEMENTATION
    //========================================================================
    
    inline parse_context parse(const std::string& input)
    {
        detail::parser_impl parser;
        return parser.parse(input);
    }
    
} // namespace arf

#endif // ARF_PARSER_HPP
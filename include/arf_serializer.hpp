// arf_serializer.hpp - A Readable Format (Arf!) - Serializer
// Version 0.2.0

#ifndef ARF_SERIALIZER_HPP
#define ARF_SERIALIZER_HPP

#include "arf_core.hpp"

namespace arf 
{
    //========================================================================
    // SERIALIZER API
    //========================================================================
    
    std::string serialize(const document& doc, bool pretty_print = false);
    
    //========================================================================
    // SERIALIZER IMPLEMENTATION
    //========================================================================
    
    namespace detail 
    {
        class serializer_impl 
        {
        public:
            std::string serialize(const document& doc, bool pretty_print) 
            {
                pretty_print_ = pretty_print;
                std::ostringstream out;
                
                bool first_category = true;
                for (const auto& [name, cat] : doc.categories)
                {
                    // Skip root category if empty
                    if (name == ROOT_CATEGORY_NAME && cat->key_values.empty() && 
                        cat->table_rows.empty() && cat->subcategories.empty())
                        continue;
                    
                    if (!first_category)
                        out << "\n";
                    first_category = false;
                    
                    // Root category doesn't need header/footer
                    if (name == ROOT_CATEGORY_NAME)
                    {
                        serialize_category_content(out, *cat, 0, true);
                    }
                    else
                    {
                        serialize_category(out, *cat, 0);
                    }
                }
                
                return out.str();
            }
            
        private:
            bool pretty_print_ = false;
            
            void serialize_category(std::ostringstream& out, const category& cat, int depth) 
            {
                std::string indent(depth * 2, ' ');
                
                // Category name
                out << indent << cat.name << ":\n";
                
                serialize_category_content(out, cat, depth, false);
                
                // Close category
                out << "/" << cat.name << "\n";
            }

            void serialize_category_content(
                std::ostringstream& out,
                const category& cat,
                int depth,
                bool is_root
            )
            {
                for (const auto& decl : cat.source_order)
                {
                    switch (decl.kind)
                    {
                        case decl_kind::key:
                            serialize_key(out, cat, decl.name, depth, is_root);
                            break;

//                        case decl_kind::table:
//                            serialize_table(out, cat, depth + (is_root ? 0 : 1));
//                            break;

                        case decl_kind::subcategory:
                            serialize_subcategory(out, cat, decl.name, depth, is_root);
                            break;
                    }
                }
            }

            void serialize_key(
                std::ostringstream& out,
                const category& cat,
                const std::string& key,
                int depth,
                bool is_root
            )
            {
                auto it = cat.key_values.find(key);
                if (it == cat.key_values.end())
                    return; // defensive

                std::string indent(depth * 2, ' ');
                out << indent << (is_root ? "" : "  ") << key << " = ";
                serialize_value(out, it->second);
                out << "\n";
            }

            void serialize_subcategory(
                std::ostringstream& out,
                const category& cat,
                const std::string& name,
                int depth,
                bool is_root
            )
            {
                auto it = cat.subcategories.find(name);
                if (it == cat.subcategories.end())
                    return;

                const category& sub = *it->second;

                std::string indent(depth * 2, ' ');
                out << "\n" << indent << (is_root ? "" : "  ") << ":" << name << "\n";

                serialize_category_content(out, sub, depth + 1, false);

                out << indent << (is_root ? "" : "  ") << "/" << name << "\n";
            }

            void serialize_table(std::ostringstream& out, const category& cat, int depth)
            {
                if (cat.table_columns.empty())
                    return; // malformed AST, or future extension

                std::string indent(depth * 2, ' ');
                
                std::vector<size_t> col_widths;
                if (pretty_print_)
                    col_widths = compute_column_widths(cat);
                
                // Table header
                out << indent << "#";
                for (size_t i = 0; i < cat.table_columns.size(); ++i)
                {
                    out << "  " << cat.table_columns[i].name << ":" << type_to_string(cat.table_columns[i].type);
                }
                out << "\n";
                
                // Table rows
                serialize_table_rows(out, cat, depth, col_widths);
            }
            
            void serialize_table_rows(std::ostringstream& out, const category& cat, int depth, 
                                     const std::vector<size_t>& col_widths = {})
            {
                std::string indent(depth * 2, ' ');
                
                for (const auto& row : cat.table_rows)
                {
                    out << indent;
                    for (size_t i = 0; i < row.size(); ++i)
                    {
                        if (i > 0) out << "  ";
                        
                        std::ostringstream cell;
                        serialize_value(cell, row[i]);
                        std::string cell_str = cell.str();
                        
                        if (!col_widths.empty() && i < col_widths.size())
                        {
                            out << std::left << std::setw(col_widths[i]) << cell_str;
                        }
                        else
                        {
                            out << cell_str;
                        }
                    }
                    out << "\n";
                }
            }
            
            std::vector<size_t> compute_column_widths(const category& cat)
            {
                std::vector<size_t> widths(cat.table_columns.size(), 0);
                
                for (const auto& row : cat.table_rows)
                {
                    for (size_t i = 0; i < row.size() && i < widths.size(); ++i)
                    {
                        std::ostringstream cell;
                        serialize_value(cell, row[i]);
                        widths[i] = std::max(widths[i], cell.str().size());
                    }
                }
                
                return widths;
            }
            
            void serialize_value(std::ostringstream& out, const value& val)
            {
                std::visit([&out](const auto& v) 
                {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>)
                        out << v;
                    else if constexpr (std::is_same_v<T, int64_t>)
                        out << v;
                    else if constexpr (std::is_same_v<T, double>)
                        out << v;
                    else if constexpr (std::is_same_v<T, bool>)
                        out << (v ? "true" : "false");
                    else if constexpr (std::is_same_v<T, std::vector<std::string>>)
                    {
                        for (size_t i = 0; i < v.size(); ++i)
                        {
                            out << v[i];
                            if (i < v.size() - 1) out << "|";
                        }
                    }
                    else if constexpr (std::is_same_v<T, std::vector<int64_t>>)
                    {
                        for (size_t i = 0; i < v.size(); ++i)
                        {
                            out << v[i];
                            if (i < v.size() - 1) out << "|";
                        }
                    }
                    else if constexpr (std::is_same_v<T, std::vector<double>>)
                    {
                        for (size_t i = 0; i < v.size(); ++i)
                        {
                            out << v[i];
                            if (i < v.size() - 1) out << "|";
                        }
                    }
                }, val);
            }
        };
        
    } // namespace detail
    
    //========================================================================
    // PUBLIC SERIALIZER API IMPLEMENTATION
    //========================================================================
    
    inline std::string serialize(const document& doc, bool pretty_print)
    {
        detail::serializer_impl serializer;
        return serializer.serialize(doc, pretty_print);
    }
    
} // namespace arf

#endif // ARF_SERIALIZER_HPP
// arf_query.hpp - A Readable Format (Arf!) - Query Interface
// Version 0.2.0

#ifndef ARF_QUERY_HPP
#define ARF_QUERY_HPP

#include "arf_core.hpp"

namespace arf 
{
    //========================================================================
    // QUERY API
    //========================================================================
    
    std::optional<value> get_value(const document& doc, const std::string& path);
    std::optional<std::string> get_string(const document& doc, const std::string& path);
    std::optional<int64_t> get_int(const document& doc, const std::string& path);
    std::optional<double> get_float(const document& doc, const std::string& path);
    std::optional<bool> get_bool(const document& doc, const std::string& path);
    
    //========================================================================
    // QUERY IMPLEMENTATION
    //========================================================================
    
    namespace detail 
    {
        inline std::vector<std::string> split_path(const std::string& path)
        {
            std::vector<std::string> parts;
            std::string current;
            
            for (char c : path)
            {
                if (c == '.')
                {
                    if (!current.empty())
                    {
                        parts.push_back(to_lower(current));
                        current.clear();
                    }
                }
                else
                {
                    current += c;
                }
            }
            
            if (!current.empty())
                parts.push_back(to_lower(current));
            
            return parts;
        }
        
        inline const category* find_category(const document& doc, const std::vector<std::string>& path)
        {
            if (path.empty()) return nullptr;
            
            auto it = doc.categories.find(path[0]);
            if (it == doc.categories.end()) 
            {
                // Try root category
                it = doc.categories.find(std::string(ROOT_CATEGORY_NAME));
                if (it == doc.categories.end()) return nullptr;
            }
            const category* current = it->second.get();
            size_t start_idx = (path[0] == std::string(ROOT_CATEGORY_NAME)) ? 1 : 1;
            
            for (size_t i = start_idx; i < path.size() - 1; ++i)
            {
                auto sub_it = current->subcategories.find(path[i]);
                if (sub_it == current->subcategories.end()) return nullptr;
                current = sub_it->second.get();
            }

            return current;
        }
    
    } // namespace detail

    //========================================================================
    // PUBLIC QUERY API IMPLEMENTATION
    //========================================================================

    inline std::optional<value> get_value(const document& doc, const std::string& path)
    {
        auto parts = detail::split_path(path);
        if (parts.empty()) return std::nullopt;
        
        const category* cat = detail::find_category(doc, parts);
        if (!cat) return std::nullopt;
        
        const std::string& key = parts.back();
        auto it = cat->key_values.find(key);
        if (it == cat->key_values.end()) return std::nullopt;
        
        return it->second;
    }

    inline std::optional<std::string> get_string(const document& doc, const std::string& path)
    {
        auto val = get_value(doc, path);
        if (!val) return std::nullopt;
        
        if (auto* str = std::get_if<std::string>(&*val))
            return *str;
        
        return std::nullopt;
    }

    inline std::optional<int64_t> get_int(const document& doc, const std::string& path)
    {
        auto val = get_value(doc, path);
        if (!val) return std::nullopt;
        
        if (auto* i = std::get_if<int64_t>(&*val))
            return *i;
        
        // Try to convert from string
        if (auto* str = std::get_if<std::string>(&*val))
        {
            try {
                return std::stoll(*str);
            } catch (...) {
                return std::nullopt;
            }
        }
        
        return std::nullopt;
    }

    inline std::optional<double> get_float(const document& doc, const std::string& path)
    {
        auto val = get_value(doc, path);
        if (!val) return std::nullopt;
        
        if (auto* d = std::get_if<double>(&*val))
            return *d;
        
        // Try to convert from string
        if (auto* str = std::get_if<std::string>(&*val))
        {
            try {
                return std::stod(*str);
            } catch (...) {
                return std::nullopt;
            }
        }
        
        return std::nullopt;
    }

    inline std::optional<bool> get_bool(const document& doc, const std::string& path)
    {
        auto val = get_value(doc, path);
        if (!val) return std::nullopt;
        
        if (auto* b = std::get_if<bool>(&*val))
            return *b;
        
        // Try to convert from string
        if (auto* str = std::get_if<std::string>(&*val))
        {
            std::string lower = detail::to_lower(*str);
            if (lower == "true" || lower == "yes" || lower == "1")
                return true;
            if (lower == "false" || lower == "no" || lower == "0")
                return false;
        }
        
        return std::nullopt;
    }
    
} // namespace arf

#endif // ARF_QUERY_HPP
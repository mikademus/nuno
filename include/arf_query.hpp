// arf_query.hpp - A Readable Format (Arf!) - Query Interface
// Version 0.2.0

#ifndef ARF_QUERY_HPP
#define ARF_QUERY_HPP

#include "arf_core.hpp"
#include <iterator>
#include <span>
#include <cassert>

namespace arf 
{
    //========================================================================
    // FORWARD DECLARATIONS
    //========================================================================
    
    class table_view;
    class row_view;
    
    //========================================================================
    // TABLE ROW VIEW
    //========================================================================
    
    class row_view 
    {
    public:
        row_view(const table_row* row, const table_view* table, const category* source)
            : row_(row), table_(table), source_category_(source) {}
        
        // Index-based access
        const value& operator[](size_t index) const { return (*row_)[index]; }
        
        // Name-based value access
        const value* get_ptr(const std::string& column_name) const;

        template<typename T>
        const T* get_ptr(const std::string& column_name) const
        {
            if (auto v = get_ptr(column_name))
                return std::get_if<T>(v);
            return nullptr;
        }
        
        // Convenience typed getters
        std::optional<std::string> get_string(const std::string& col) const;
        std::optional<int64_t> get_int(const std::string& col) const;
        std::optional<double> get_float(const std::string& col) const;
        std::optional<bool> get_bool(const std::string& col) const;
        
        // Array getters
        std::optional<std::span<const std::string>> get_string_array(const std::string& col) const;
        std::optional<std::span<const int64_t>> get_int_array(const std::string& col) const;
        std::optional<std::span<const double>> get_float_array(const std::string& col) const;
        
        // Provenance information
        const category* source() const { return source_category_; }
        std::string source_name() const { return source_category_->name; }
        bool is_base_row() const;
        
        // Raw access
        const table_row& raw() const { return *row_; }
        
    private:
        const table_row* row_;
        const table_view* table_;
        const category* source_category_;
    };
    
    //========================================================================
    // TABLE VIEW
    //========================================================================
    
    class table_view 
    {
    public:
            // Iterator for document-order row traversal
        class document_iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = row_view;

            document_iterator(const table_view* table, bool at_end = false);

            row_view operator*() const;
            document_iterator& operator++();
            bool operator==(const document_iterator& other) const;

        private:
            void advance();

            const table_view* table_;
            bool at_end_;

            struct frame
            {
                const category* cat;
                size_t decl_index;   // Position in source_order
                const decl_ref* current_row = nullptr;
            };

            std::vector<frame> stack_;
        };

        // Range wrapper for rows_document()
        class document_range
        {
        public:
            document_range(const table_view* table) : table_(table) {}
            document_iterator begin() const { return table_->rows_document_begin(); }
            document_iterator end() const { return table_->rows_document_end(); }
        private:
            const table_view* table_;
        };

        // Iterator for recursive row traversal
        class recursive_iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = row_view;

            recursive_iterator(const table_view* table, bool at_end = false);

            row_view operator*() const;
            recursive_iterator& operator++();
            bool operator==(const recursive_iterator& other) const;

        private:
            void advance();

            const table_view* table_;
            bool at_end_;

            struct frame
            {
                const category* cat;
                size_t row_index;
                std::map<std::string, std::unique_ptr<category>>::const_iterator child_it;
            };

            std::vector<frame> stack_;
        };
        
        // Range wrapper for rows_recursive()
        class recursive_range 
        {
        public:
            recursive_range(const table_view* table) : table_(table) {}
            recursive_iterator begin() const { return table_->rows_recursive_begin(); }
            recursive_iterator end() const { return table_->rows_recursive_end(); }
        private:
            const table_view* table_;
        };
        
        explicit table_view(const category* cat) : cat_(cat) {}
        
        // Column information
        const std::vector<column>& columns() const { return cat_->table_columns; }
        std::optional<size_t> column_index(const std::string& name) const;
        
        // Row access (current category only)
        const std::vector<table_row>& rows() const { return cat_->table_rows; }
        row_view row(size_t index) const;
        
        // Recursive row iteration (includes subcategories, depth-first)
        recursive_iterator rows_recursive_begin() const { return recursive_iterator(this, false); }
        recursive_iterator rows_recursive_end() const { return recursive_iterator(this, true); }
        recursive_range rows_recursive() const { return recursive_range(this); }
        
        // Document-order row iteration (preserves author's ordering)
        document_iterator rows_document_begin() const { return document_iterator(this, false); }
        document_iterator rows_document_end() const { return document_iterator(this, true); }
        document_range rows_document() const { return document_range(this); }

        // Subcategory access
        const std::map<std::string, std::unique_ptr<category>>& subcategories() const 
        { 
            return cat_->subcategories; 
        }
        
        std::optional<table_view> subcategory(const std::string& name) const;
        
        // Raw category access
        const category* raw() const { return cat_; }
        
    private:
        const category* cat_;
        
        friend class recursive_iterator;
        friend class row_view;
    };
    
    //========================================================================
    // VALUE PROXY FOR REFLECTION
    //========================================================================
    
    class value_ref 
    {
    public:
        value_ref(const value& v) : val_(&v) {}
        
        bool is_scalar() const 
        {
            return std::holds_alternative<std::string>(*val_) ||
                   std::holds_alternative<int64_t>(*val_) ||
                   std::holds_alternative<double>(*val_) ||
                   std::holds_alternative<bool>(*val_);
        }
        
        bool is_array() const 
        {
            return std::holds_alternative<std::vector<std::string>>(*val_) ||
                   std::holds_alternative<std::vector<int64_t>>(*val_) ||
                   std::holds_alternative<std::vector<double>>(*val_);
        }
        
        bool is_string() const { return std::holds_alternative<std::string>(*val_); }
        bool is_int() const { return std::holds_alternative<int64_t>(*val_); }
        bool is_float() const { return std::holds_alternative<double>(*val_); }
        bool is_bool() const { return std::holds_alternative<bool>(*val_); }
        
        bool is_string_array() const { return std::holds_alternative<std::vector<std::string>>(*val_); }
        bool is_int_array() const { return std::holds_alternative<std::vector<int64_t>>(*val_); }
        bool is_float_array() const { return std::holds_alternative<std::vector<double>>(*val_); }
        
        std::optional<std::string> as_string() const 
        {
            if (auto* s = std::get_if<std::string>(val_))
                return *s;
            return std::nullopt;
        }
        
        std::optional<int64_t> as_int() const 
        {
            if (auto* i = std::get_if<int64_t>(val_))
                return *i;
            return std::nullopt;
        }
        
        std::optional<double> as_float() const 
        {
            if (auto* d = std::get_if<double>(val_))
                return *d;
            return std::nullopt;
        }
        
        std::optional<bool> as_bool() const 
        {
            if (auto* b = std::get_if<bool>(val_))
                return *b;
            return std::nullopt;
        }
                
        template<typename T>
        std::optional<std::span<const T>> as_array() const
        {
            if (auto* arr = std::get_if<std::vector<T>>(val_))
                return std::span<const T>(*arr);
            return std::nullopt;
        }

        const value& raw() const { return *val_; }
        
    private:
        const value* val_;
    };
    
    //========================================================================
    // QUERY API
    //========================================================================
    
    // Basic value query (returns proxy for reflection)
    std::optional<value_ref> get(const document& doc, const std::string& path);
    
    // Direct value access
    std::optional<value> get_value(const document& doc, const std::string& path);
    std::optional<std::string> get_string(const document& doc, const std::string& path);
    std::optional<int64_t> get_int(const document& doc, const std::string& path);
    std::optional<double> get_float(const document& doc, const std::string& path);
    std::optional<bool> get_bool(const document& doc, const std::string& path);
    
    // Array access helpers
    template<typename T>
    std::optional<std::span<const T>> get_array(const document& doc, const std::string& path);
    
    // Table access
    std::optional<table_view> get_table(const document& doc, const std::string& path);
    
    // Path utilities
    std::string category_path(const category* cat, const document& doc);
    std::string to_path(const row_view& row, const document& doc);
    
    //========================================================================
    // QUERY IMPLEMENTATION
    //========================================================================
    
    namespace detail 
    {
        enum class path_resolution
        {
            key_owner,   // stop before last segment
            category     // consume full path
        };

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
        

        inline const category* resolve_category(
            const document& doc,
            const std::vector<std::string>& path,
            path_resolution target
        )
        {
            if (path.empty()) return nullptr;

            auto it = doc.categories.find(path[0]);
            if (it == doc.categories.end())
            {
                it = doc.categories.find(std::string(ROOT_CATEGORY_NAME));
                if (it == doc.categories.end())
                    return nullptr;
            }

            const category* current = it->second.get();

            const size_t limit =
                (target == path_resolution::key_owner)
                    ? path.size() - 1
                    : path.size();

            for (size_t i = 1; i < limit; ++i)
            {
                auto sub = current->subcategories.find(path[i]);
                if (sub == current->subcategories.end())
                    return nullptr;

                current = sub->second.get();
            }

            return current;
        }

        inline const value* find_value_ptr(const document& doc, const std::string& path)
        {
            auto parts = split_path(path);
            if (parts.empty()) return nullptr;

            const category* cat =
                resolve_category(doc, parts, path_resolution::key_owner);
            if (!cat) return nullptr;

            auto it = cat->key_values.find(parts.back());
            if (it == cat->key_values.end())
                return nullptr;

            return &it->second;
        }
    } // namespace detail

    //========================================================================
    // TABLE VIEW IMPLEMENTATION
    //========================================================================
    
    // Document iterator implementation
    inline table_view::document_iterator::document_iterator(
        const table_view* table,
        bool at_end
    )
        : table_(table), at_end_(at_end)
    {
        if (!at_end_)
        {
            stack_.push_back({ table_->cat_, 0, nullptr });
            advance();
        }
    }
            
    inline row_view table_view::document_iterator::operator*() const
    {
        const auto& f = stack_.back();
        assert(f.current_row && f.current_row->kind == decl_kind::table_row);

        return row_view(
            &f.cat->table_rows[f.current_row->row_index],
            table_,
            f.cat
        );    
    }
    
    inline table_view::document_iterator&
    table_view::document_iterator::operator++()
    {
        advance();
        return *this;
    }
    
    inline bool table_view::document_iterator::operator==(
        const document_iterator& other
    ) const
    {
        if (at_end_ && other.at_end_) return true;
        if (at_end_ != other.at_end_) return false;
        
        const auto& a = stack_.back();
        const auto& b = other.stack_.back();
        
        return a.cat == b.cat && a.decl_index == b.decl_index;
    }
    
    inline void table_view::document_iterator::advance()
    {
        while (!stack_.empty())
        {
            frame& f = stack_.back();

            while (f.decl_index < f.cat->source_order.size())
            {
                const auto& decl = f.cat->source_order[f.decl_index++];

                switch (decl.kind)
                {
                    case decl_kind::table_row:
                        f.current_row = &decl;
                        return;

                    case decl_kind::subcategory:
                    {
                        auto it = f.cat->subcategories.find(decl.name);
                        if (it != f.cat->subcategories.end())
                        {
                            stack_.push_back({ it->second.get(), 0 });
                            // Continue from top of outer while - will reacquire 'f'
                            goto next_frame;
                        }
                        break;
                    }

                    case decl_kind::key:
                        break;
                }
            }

            stack_.pop_back();
            
            if (stack_.empty())
            {
                at_end_ = true;
                return;
            }

            stack_.back().current_row = nullptr;
            
        next_frame:;  // Label for goto
        }
    }


    inline std::optional<size_t> table_view::column_index(const std::string& name) const 
    {
        std::string lower = detail::to_lower(name);
        for (size_t i = 0; i < cat_->table_columns.size(); ++i) 
        {
            if (cat_->table_columns[i].name == lower)
                return i;
        }
        return std::nullopt;
    }
    
    inline row_view table_view::row(size_t index) const 
    {
        return row_view(&cat_->table_rows[index], this, cat_);
    }
    
    inline std::optional<table_view> table_view::subcategory(const std::string& name) const 
    {
        std::string lower = detail::to_lower(name);
        auto it = cat_->subcategories.find(lower);
        if (it == cat_->subcategories.end()) return std::nullopt;
        return table_view(it->second.get());
    }
    
    // Recursive iterator implementation
    inline table_view::recursive_iterator::recursive_iterator(
        const table_view* table,
        bool at_end
    )
        : table_(table), at_end_(at_end)
    {
        if (at_end_) return;

        stack_.push_back({
            table_->cat_,
            0,
            table_->cat_->subcategories.begin()
        });

        // Ensure first dereference is valid
        if (stack_.back().cat->table_rows.empty())
            advance();
    }
    
    inline row_view table_view::recursive_iterator::operator*() const
    {
        assert(!at_end_ && !stack_.empty());

        const auto& f = stack_.back();
        return row_view(
            &f.cat->table_rows[f.row_index],
            table_,
            f.cat
        );
    }
    
    inline table_view::recursive_iterator&
    table_view::recursive_iterator::operator++()
    {
        advance();
        return *this;
    }
    
    inline bool table_view::recursive_iterator::operator==(
        const recursive_iterator& other
    ) const
    {
        if (at_end_ && other.at_end_) return true;
        if (at_end_ != other.at_end_) return false;

        // Both valid iterators: compare *position*, not history
        const auto& a = stack_.back();
        const auto& b = other.stack_.back();

        return a.cat == b.cat
            && a.row_index == b.row_index;
    }

    inline void table_view::recursive_iterator::advance()
    {
        while (!stack_.empty())
        {
            auto& f = stack_.back();

            // 1. Advance within current category rows
            if (++f.row_index < f.cat->table_rows.size())
                return;

            // 2. Descend into next subcategory with rows
            while (f.child_it != f.cat->subcategories.end())
            {
                const category* child = f.child_it->second.get();
                ++f.child_it;
                stack_.push_back({child, 0, child->subcategories.begin()});
                return;
            }

            // 3. Exhausted this category → pop and continue upward
            stack_.pop_back();
        }

        at_end_ = true;
    }
    
    //========================================================================
    // ROW VIEW IMPLEMENTATION
    //========================================================================
    
    inline const value* row_view::get_ptr(const std::string& column_name) const
    {
        auto idx = table_->column_index(column_name);
        if (!idx || *idx >= row_->size())
            return nullptr;

        return &(*row_)[*idx];
    }
    
    inline std::optional<std::string> row_view::get_string(const std::string& col) const 
    {
        if (auto p = get_ptr<std::string>(col))
            return *p;
        return std::nullopt;
    }
    
    inline std::optional<int64_t> row_view::get_int(const std::string& col) const
    {
        if (auto p = get_ptr<int64_t>(col))
            return *p;
        
        if (auto p = get_ptr<std::string>(col))
        {
            try { return std::stoll(*p); } 
            catch (...) { return std::nullopt; }
        }
        
        return std::nullopt;
    }

    inline std::optional<double> row_view::get_float(const std::string& col) const 
    {
        if (auto p = get_ptr<double>(col))
            return *p;
        
        if (auto p = get_ptr<std::string>(col))
        {
            try { return std::stod(*p); } 
            catch (...) { return std::nullopt; }
        }
        
        return std::nullopt;
    }

    inline std::optional<bool> row_view::get_bool(const std::string& col) const 
    {
        if (auto p = get_ptr<bool>(col))
            return *p;
        
        if (auto p = get_ptr<std::string>(col))
        {
            std::string lower = detail::to_lower(*p);
            if (lower == "true" || lower == "yes" || lower == "1")
                return true;
            if (lower == "false" || lower == "no" || lower == "0")
                return false;
        }
        
        return std::nullopt;
    }
    
    inline std::optional<std::span<const std::string>>
    row_view::get_string_array(const std::string& col) const
    {
        if (auto* v = get_ptr(col))
        {
            if (auto* arr = std::get_if<std::vector<std::string>>(v))
                return std::span<const std::string>(*arr);
        }
        return std::nullopt;
    }

    inline std::optional<std::span<const int64_t>> 
    row_view::get_int_array(const std::string& col) const 
    {
        if (auto* v = get_ptr(col))
        {
            if (auto* arr = std::get_if<std::vector<int64_t>>(v))
                return std::span<const int64_t>(*arr);
        }
        return std::nullopt;
    }
    
    inline std::optional<std::span<const double>> 
    row_view::get_float_array(const std::string& col) const 
    {
        if (auto* v = get_ptr(col))
        {
            if (auto* arr = std::get_if<std::vector<double>>(v))
                return std::span<const double>(*arr);
        }
        return std::nullopt;
    }
    
    inline bool row_view::is_base_row() const 
    {
        return source_category_ == table_->cat_;
    }

    //========================================================================
    // PUBLIC QUERY API IMPLEMENTATION
    //========================================================================

    inline std::optional<value_ref> get(const document& doc, const std::string& path)
    {
        if (const value* v = detail::find_value_ptr(doc, path))
            return value_ref(*v);
        return std::nullopt;
    }

    inline std::optional<value> get_value(const document& doc, const std::string& path)
    {
        if (const value* v = detail::find_value_ptr(doc, path))
            return *v;
        return std::nullopt;
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
    
    template<typename T>
    inline std::optional<std::span<const T>> get_array(const document& doc, const std::string& path)
    {
        if (auto ref = get(doc, path))
            return ref->as_array<T>();
        return std::nullopt;
    }
    
    inline std::optional<table_view> get_table(const document& doc, const std::string& path)
    {
        auto parts = detail::split_path(path);
        if (parts.empty()) return std::nullopt;
        
        const category* cat = detail::resolve_category(doc, parts, detail::path_resolution::category);
        if (!cat) return std::nullopt;
        
        return table_view(cat);
    }
    
    inline std::string category_path(const category* cat, const document& doc)
    {
        if (!cat) return "";

        std::vector<std::string> parts;
        while (cat)
        {
            parts.push_back(cat->name);
            cat = cat->parent;
        }

        std::reverse(parts.begin(), parts.end());

        std::string path;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            if (i) path += '.';
            path += parts[i];
        }

        return path;        
    }
    
    inline std::string to_path(const row_view& row, const document& doc)
    {
        return category_path(row.source(), doc);
    }
    
} // namespace arf

#endif // ARF_QUERY_HPP
// arf_reflect.hpp - A Readable Format (Arf!) - Reflection Interface
// Version 0.2.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_REFLECT_HPP
#define ARF_REFLECT_HPP

#include "arf_core.hpp"
#include <functional>
#include <span>
#include <cassert>

namespace arf
{
//========================================================================
// Forward declarations
//========================================================================
    
    // Value metadata
    class value_ref;

    // Document metadata
    // TODO

    // Table metadata
    class table_ref;
    class table_column_ref;
    class table_row_ref;
    class table_partition_ref;

//========================================================================
// value_ref
//========================================================================
    
    class value_ref 
    {
    public:
        value_ref(const typed_value& v) : tv_(&v) {}

    // --- provenance ---
        value_type      type()          const { return tv_->type; }
        bool            declared()      const { return tv_->type_source == type_ascription::declared; }
        type_ascription type_source()   const { return tv_->type_source; }
        value_locus     origin_site()   const { return tv_->origin_site; }

    // --- raw access ---
        const value& raw() const { return tv_->val; }

        std::optional<std::string_view> source_str() const;
        
    // --- format ---        
        bool is_scalar() const; 
        bool is_array()  const;

        bool is_string() const { return std::holds_alternative<std::string>(tv_->val); }
        bool is_int()    const { return std::holds_alternative<int64_t>(tv_->val); }
        bool is_float()  const { return std::holds_alternative<double>(tv_->val); }
        bool is_bool()   const { return std::holds_alternative<bool>(tv_->val); }
        
        bool is_string_array() const { return std::holds_alternative<std::vector<std::string>>(tv_->val); }
        bool is_int_array()    const { return std::holds_alternative<std::vector<int64_t>>(tv_->val); }
        bool is_float_array()  const { return std::holds_alternative<std::vector<double>>(tv_->val); }
        
    // --- scalar access: allow conversions ---
        std::optional<std::string> as_string() const;        
        std::optional<int64_t>     as_int()    const { return to_int(tv_->val); }
        std::optional<double>      as_float()  const { return to_float(tv_->val); }
        std::optional<bool>        as_bool()   const { return to_bool(tv_->val); }
        
    // --- array access: non-converting views ---
        std::optional<std::span<const std::string>> string_array() const { return array_view<std::string>(); }
        std::optional<std::span<const int64_t>>     int_array()    const { return array_view<int64_t>(); }
        std::optional<std::span<const double>>      float_array()  const { return array_view<double>(); }


    private:
        const typed_value* tv_;

        template<typename T>
        std::optional<std::span<const T>> array_view() const;

        std::optional<std::string> to_string(const value& v) const;
        std::optional<int64_t>     to_int(const value& v) const;
        std::optional<double>      to_float(const value& v) const;
        std::optional<bool>        to_bool(const value& v) const;
    };

//========================================================================
// table_column_ref
//========================================================================

    class table_column_ref
    {
    public:
        table_column_ref(const column& col, size_t index)
            : col_(&col), index_(index) {}

        const std::string& name() const { return col_->name; }
        size_t             index() const { return index_; }

        value_type      type()        const { return col_->type; }
        type_ascription type_source() const { return col_->type_source; }

    private:
        const column* col_;
        size_t        index_;
    };

//========================================================================
// table_cell_ref
//========================================================================

    class table_cell_ref
    {
    public:
        table_cell_ref(const table_row_ref& row, const table_column_ref& col)
            : row_(&row)
            , col_(&col)
        {
        }

        size_t row_index()    const; 
        size_t column_index() const;

        value_ref  value()  const;

        const table_column_ref& column() const { return *col_; }
        const table_row_ref&    row()    const { return *row_; }   
             
    private:
        const table_row_ref* row_;
        const table_column_ref* col_;
    };

//========================================================================
// table_row_ref
//========================================================================

    class table_row_ref
    {
    public:
        table_row_ref(const table_ref& table, size_t row_index)
            : table_(&table), row_index_(row_index) {}

        // --- identity ---
        size_t index() const { return row_index_; }
        const table_ref& table() const { return *table_; }

        // --- structure ---
        table_partition_ref subcategory() const;

        size_t cell_count() const;
        auto cells() const;
        table_cell_ref cell(const table_column_ref& col) const;

    private:
        const table_ref* table_;
        size_t           row_index_; // ALWAYS table-space index
    };

//========================================================================
// table_partition_ref
//========================================================================

    class table_partition_ref
    {
    public:
        table_partition_ref(const table_ref& table, size_t subcat_index);

        const std::string& name() const;

    //-- Direct rows -----------------------
        // rows declared directly in this subcategory (no descendants)
        size_t direct_row_count() const;

        // global table row index (direct only)
        size_t direct_row_in_table(size_t i) const;

        // iteration (direct only)
        auto direct_rows() const;        

    //-- All rows -------------------------
        // number of rows in this subcategory (all descendants included)
        size_t row_count() const;

        // global table row index
        size_t row_in_table(size_t i) const;

        // index relative to parent subcategory
        size_t row_in_parent(size_t i) const;


    //-- Subcategory/partition structure -------------------------
        // hierarchy
        bool has_parent() const;
        table_partition_ref parent() const;

        size_t child_count() const;
        table_partition_ref child(size_t i) const;

        // iteration
        auto rows() const;

    private:
        const table_ref* table_;
        size_t           subcat_index_;
    };  

//========================================================================
// table_ref
// table_partition_info
//========================================================================

    struct table_partition_info
    {
        const category*     cat;
        size_t              parent;      // index or npos
        std::vector<size_t> children;

        // rows declared directly while this partition is active
        std::vector<size_t> direct_rows;

        // rows including descendants
        std::vector<size_t> all_rows;

        std::vector<size_t> late_rows;        
    };

    class table_ref
    {
    public:
        table_ref(const category& cat)
            : category_(&cat) {}

        const category& schema() const { return *category_; }
                    
        // columns
        size_t      column_count() const;
        table_column_ref  column(size_t i) const;
        auto        columns() const;
        //size_t      column_index(std::string_view name) const { return category_->column_index(name); }
        
        // rows (document order)
        size_t      row_count() const { return category_->table_rows.size(); }
        table_row_ref     row(size_t table_row_index) const { return table_row_ref(*this, table_row_index); }
        auto        rows() const;

        // subcategories
        table_partition_ref root_subcategory() const;
        size_t          subcategory_count() const;
        table_partition_ref subcategory(size_t i) const;
        auto            subcategories() const;

        const std::vector<table_partition_info>& partitions() const;
        
    private:
        const category* category_;
        mutable std::vector<table_partition_info> partitions_;
    };

//========================================================================
// Document reflection API implementation
//========================================================================

    inline std::optional<std::string_view> value_ref::source_str() const
    {
        if (!tv_->source_literal)
            return std::nullopt;

        return std::string_view(*tv_->source_literal);
    }        
    
    inline bool value_ref::is_scalar() const 
    {
        return std::holds_alternative<std::string>(tv_->val) ||
               std::holds_alternative<int64_t>(tv_->val) ||
               std::holds_alternative<double>(tv_->val) ||
               std::holds_alternative<bool>(tv_->val);
    }
    
    inline bool value_ref::is_array() const 
    {
        return std::holds_alternative<std::vector<std::string>>(tv_->val) ||
               std::holds_alternative<std::vector<int64_t>>(tv_->val) ||
               std::holds_alternative<std::vector<double>>(tv_->val);
    }
    
    inline std::optional<std::string> value_ref::as_string() const
    {
        if (auto s = source_str())
            return std::string(*s);

        return to_string(tv_->val);
    }

    template<typename T>
    std::optional<std::span<const T>> value_ref::array_view() const
    {
        if (auto* arr = std::get_if<std::vector<T>>(&tv_->val))
            return std::span<const T>(*arr);
        return std::nullopt;
    }

    inline std::optional<std::string> value_ref::to_string(const value& v) const
    {
        if (auto* str = std::get_if<std::string>(&v))
            return *str;

        if (auto* i = std::get_if<int64_t>(&v))
            return std::to_string(*i);

        if (auto* d = std::get_if<double>(&v))
            return std::to_string(*d);

        if (auto* b = std::get_if<bool>(&v))
            return *b ? "true" : "false";

        return std::nullopt;
    }

    inline std::optional<int64_t> value_ref::to_int(const value& v) const
    {
        if (auto* i = std::get_if<int64_t>(&v))
            return *i;

        if (auto* d = std::get_if<double>(&v))
            return static_cast<int64_t>(*d);

        if (auto* s = std::get_if<std::string>(&v))
        {
            try { return std::stoll(*s); }
            catch (...) {}
        }
        return std::nullopt;
    }

    inline std::optional<double> value_ref::to_float(const value& v) const
    {
        if (auto* d = std::get_if<double>(&v))
            return *d;

        if (auto* i = std::get_if<int64_t>(&v))
            return static_cast<double>(*i);

        if (auto* s = std::get_if<std::string>(&v))
        {
            try { return std::stod(*s); }
            catch (...) {}
        }
        return std::nullopt;
    }

    inline std::optional<bool> value_ref::to_bool(const value& v) const
    {
        if (auto* b = std::get_if<bool>(&v))
            return *b;

        if (auto* s = std::get_if<std::string>(&v))
        {
            std::string lower = detail::to_lower(*s);
            if (lower == "true" || lower == "yes" || lower == "1") return true;
            if (lower == "false" || lower == "no" || lower == "0") return false;
        }
        return std::nullopt;
    }        

//========================================================================
// Table reflection API implementation
//========================================================================

    namespace detail
    {
        constexpr size_t npos = static_cast<size_t>(-1);

        //inline void collect_rows_depth_first(
        //    const category& cat,
        //    std::vector<size_t>& out
        //)
        //{
        //    for (const decl_ref& decl : cat.source_order)
        //    {
        //        switch (decl.kind)
        //        {
        //            case decl_kind::table_row:
        //                out.push_back(decl.row_index);
        //                break;
//
        //            case decl_kind::subcategory:
        //                collect_rows_depth_first(*decl.subcategory, out);
        //                break;
//
        //            case decl_kind::key:
        //                break;
        //        }
        //    }
        //}        

        // Invariant: direct_rows are assigned strictly by lexical scope,
        // not by visibility or traversal order.        
        inline std::vector<table_partition_info>
        build_partitions(const category& root)
        {
            std::vector<table_partition_info> parts;

            parts.push_back({
                &root,
                npos,
                {},
                {},
                {},
                {}
            });

            std::vector<size_t> stack;
            stack.push_back(0);

            //std::vector<size_t> document_rows;

            // Once a subcategory is seen, direct rows are sealed
            std::vector<bool> direct_rows_sealed;
            direct_rows_sealed.push_back(false); // root

            auto enter_partition =
            [&](const category* cat)
            {
                const size_t parent = stack.back();

                direct_rows_sealed[parent] = true;

                const size_t idx = parts.size();
                parts.push_back({
                    cat,
                    parent,
                    {},
                    {},
                    {},
                    {}
                });

                parts[parent].children.push_back(idx);
                direct_rows_sealed.push_back(false);
                stack.push_back(idx);
            };

            auto leave_partition =
            [&]()
            {
                assert(stack.size() > 1);
                stack.pop_back();
            };

            // Pass 1: assign ownership, direct_rows, late_rows
            std::function<void(const category&)> walk;
            walk =
            [&](const category& cat)
            {
                for (const decl_ref& decl : cat.source_order)
                {
                    switch (decl.kind)
                    {
                        case decl_kind::table_row:
                        {
                            //document_rows.push_back(decl.row_index);

                            const size_t current = stack.back();

                            if (!direct_rows_sealed[current])
                                parts[current].direct_rows.push_back(decl.row_index);
                            else
                                parts[current].late_rows.push_back(decl.row_index);

                            break;
                        }

                        case decl_kind::subcategory:
                        {
                            const category* sub = decl.subcategory;
                            assert(sub);

                            enter_partition(sub);
                            walk(*sub);
                            leave_partition();
                            break;
                        }

                        case decl_kind::key:
                            break;
                    }
                }
            };

            walk(root);

            std::unordered_map<const category*, size_t> cat_to_partition;
            for (size_t i = 0; i < parts.size(); ++i)
                cat_to_partition[parts[i].cat] = i;            

            // Pass 2: build all_rows bottom-up, strictly by partition ownership
            for (size_t i = parts.size(); i-- > 0; )
            {
                auto& p = parts[i];
                p.all_rows.clear();

                // 1. Direct rows (authored before first subcategory)
                p.all_rows.insert(
                    p.all_rows.end(),
                    p.direct_rows.begin(),
                    p.direct_rows.end()
                );

                // 2. Children, in source order
                for (const decl_ref& decl : p.cat->source_order)
                {
                    if (decl.kind != decl_kind::subcategory)
                        continue;

                    auto it = cat_to_partition.find(decl.subcategory);
                    if (it == cat_to_partition.end())
                        continue;

                    const auto& child = parts[it->second];
                    if (child.parent != i)
                        continue; // safety: ensure immediate child

                    p.all_rows.insert(
                        p.all_rows.end(),
                        child.all_rows.begin(),
                        child.all_rows.end()
                    );
                }

                // 3. Late rows (authored after subcategories)
                p.all_rows.insert(
                    p.all_rows.end(),
                    p.late_rows.begin(),
                    p.late_rows.end()
                );
            }

            return parts;
        }
        
    } // ns detail

    size_t table_cell_ref::row_index() const 
    { 
        return row_->index(); 
    }

    size_t table_cell_ref::column_index() const
    { 
        return col_->index(); 
    }


    inline value_ref table_cell_ref::value() const
    {
        const table_row& row =
            row_->table().schema().table_rows[row_->index()];
        const typed_value& tv = row.cells[col_->index()];
        return value_ref(tv);
    }

    inline auto table_row_ref::cells() const
    {
        struct cell_range
        {
            const table_row_ref* row;
            struct iterator
            {
                const table_row_ref* row;
                size_t col;

                table_cell_ref operator*() const
                {
                    auto cr = row->table().column(col);
                    return row->cell(cr);
                }

                iterator& operator++()
                {
                    ++col;
                    return *this;
                }

                bool operator!=(const iterator& other) const
                {
                    return col != other.col;
                }
            };

            iterator begin() const { return { row, 0 }; }
            iterator end()   const { return { row, row->cell_count() }; }
        };

        return cell_range{ this };
    }

    inline size_t table_row_ref::cell_count() const 
    { 
        return table_->column_count(); 
    }

    inline table_cell_ref table_row_ref::cell(const table_column_ref& col) const
    {
        return table_cell_ref(*this, col);
    }
 
    inline table_partition_ref::table_partition_ref( const table_ref& table, size_t subcat_index )
        : table_(&table)
        , subcat_index_(subcat_index)
    {
    }

    inline const std::string& table_partition_ref::name() const
    {
        return table_->partitions().at(subcat_index_).cat->name;
    }    

    inline size_t table_partition_ref::direct_row_count() const
    {
        return table_->partitions().at(subcat_index_).direct_rows.size();
    }

    inline size_t table_partition_ref::direct_row_in_table(size_t i) const
    {
        return table_->partitions().at(subcat_index_).direct_rows.at(i);
    }

    inline auto table_partition_ref::direct_rows() const
    {
        struct row_range
        {
            const table_partition_ref* part;

            struct iterator
            {
                const table_partition_ref* part;
                size_t i;

                table_row_ref operator*() const
                {
                    size_t row = part->direct_row_in_table(i);
                    return part->table_->row(row);
                }

                iterator& operator++()
                {
                    ++i;
                    return *this;
                }

                bool operator!=(const iterator& other) const
                {
                    return i != other.i;
                }
            };

            iterator begin() const { return { part, 0 }; }
            iterator end()   const { return { part, part->direct_row_count() }; }
        };

        return row_range{ this };
    }

    inline size_t table_partition_ref::row_count() const
    {
        return table_->partitions().at(subcat_index_).all_rows.size();
    }

    inline size_t table_partition_ref::row_in_table(size_t i) const
    {
        return table_->partitions().at(subcat_index_).all_rows.at(i);
    }

    inline size_t table_partition_ref::row_in_parent(size_t i) const
    {
        size_t parent = table_->partitions().at(subcat_index_).parent;
        if (parent == detail::npos)
            return i;

        size_t row = row_in_table(i);
        const auto& parent_rows =
            table_->partitions().at(parent).all_rows;

        auto it = std::find(parent_rows.begin(), parent_rows.end(), row);
        return static_cast<size_t>(std::distance(parent_rows.begin(), it));
    }

    inline bool table_partition_ref::has_parent() const
    {
        return table_->partitions().at(subcat_index_).parent != detail::npos;
    }

    inline table_partition_ref table_partition_ref::parent() const
    {
        size_t p = table_->partitions().at(subcat_index_).parent;
        return table_partition_ref(*table_, p);
    }

    inline size_t table_partition_ref::child_count() const
    {
        return table_->partitions().at(subcat_index_).children.size();
    }

    inline table_partition_ref table_partition_ref::child(size_t i) const
    {
        size_t idx = table_->partitions().at(subcat_index_).children.at(i);
        return table_partition_ref(*table_, idx);
    }

    inline auto table_partition_ref::rows() const
    {
        struct row_range
        {
            const table_partition_ref* part;

            struct iterator
            {
                const table_partition_ref* part;
                size_t i;

                table_row_ref operator*() const
                {
                    size_t row = part->row_in_table(i);
                    return part->table_->row(row);
                }

                iterator& operator++()
                {
                    ++i;
                    return *this;
                }

                bool operator!=(const iterator& other) const
                {
                    return i != other.i;
                }
            };

            iterator begin() const { return { part, 0 }; }
            iterator end()   const { return { part, part->row_count() }; }
        };

        return row_range{ this };
    }

    const std::vector<table_partition_info>& table_ref::partitions() const
    {
        if (partitions_.empty())
            partitions_ = detail::build_partitions(*category_);
        return partitions_;
    }
    
    inline size_t table_ref::column_count() const
    {
        return category_->table_columns.size();
    }

    inline table_column_ref table_ref::column(size_t i) const
    {
        return table_column_ref(category_->table_columns.at(i), i);
    }

    inline auto table_ref::columns() const
    {
        struct column_range
        {
            const table_ref* table;
            
            struct iterator
            {
                const table_ref* table;
                size_t index;
                
                table_column_ref operator*() const { return table->column(index); }
                iterator& operator++() { ++index; return *this; }
                bool operator!=(const iterator& other) const { return index != other.index; }
            };
            
            iterator begin() const { return {table, 0}; }
            iterator end() const { return {table, table->column_count()}; }
        };
        
        return column_range{this};
    }

    inline auto table_ref::rows() const
    {
        struct row_range
        {
            const table_ref* table;
            
            struct iterator
            {
                const table_ref* table;
                size_t index;
                
                table_row_ref operator*() const { return table->row(index); }
                iterator& operator++() { ++index; return *this; }
                bool operator!=(const iterator& other) const { return index != other.index; }
            };
            
            iterator begin() const { return {table, 0}; }
            iterator end() const { return {table, table->row_count()}; }
        };
        
        return row_range{this};
    }

    inline table_partition_ref table_ref::root_subcategory() const
    {
        return table_partition_ref(*this, 0);
    }

    inline size_t table_ref::subcategory_count() const
    {
        return partitions().size();
    }

    inline table_partition_ref table_ref::subcategory(size_t i) const
    {
        return table_partition_ref(*this, i);
    }

    inline auto table_ref::subcategories() const
    {
        struct subcat_range
        {
            const table_ref* table;
            
            struct iterator
            {
                const table_ref* table;
                size_t index;
                
                table_partition_ref operator*() const { return table->subcategory(index); }
                iterator& operator++() { ++index; return *this; }
                bool operator!=(const iterator& other) const { return index != other.index; }
            };
            
            iterator begin() const { return {table, 0}; }
            iterator end() const { return {table, table->subcategory_count()}; }
        };
        
        return subcat_range{this};
    }

    inline table_partition_ref table_row_ref::subcategory() const
    {
        const auto& parts = table_->partitions();
        
        for (size_t i = 0; i < parts.size(); ++i)
        {
            const auto& rows = parts[i].direct_rows;
            if (std::find(rows.begin(), rows.end(), row_index_) != rows.end())
                return table_partition_ref(*table_, i);
        }
    
        // Fallback: root partition
        return table_partition_ref(*table_, 0);
    }    
}

#endif
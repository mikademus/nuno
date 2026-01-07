// arf_document.hpp - A Readable Format (Arf!) - Authoritative Document Model
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_DOCUMENT_HPP
#define ARF_DOCUMENT_HPP

#include "arf_core.hpp"

#include <cassert>
#include <iterator>
#include <iostream>
#include <ranges>
#include <span>

namespace arf
{
    namespace 
    {
        template<typename T> struct node_to_view;
    }

    class document
    {
    public:
        //------------------------------------------------------------------------
        // Public, read-only views
        //------------------------------------------------------------------------

        struct category_view;
        struct table_view;
        struct column_view;
        struct table_row_view;
        struct key_view;

        //------------------------------------------------------------------------
        // Construction
        //------------------------------------------------------------------------

        document() = default;

        //------------------------------------------------------------------------
        // Category access
        //------------------------------------------------------------------------

        size_t category_count() const noexcept { return categories_.size(); }
        std::optional<category_view> root() const noexcept;
        std::optional<category_view> category(std::string_view name) const noexcept;
        std::optional<category_view> category(category_id id) const noexcept;
        std::vector<category_view>   categories() const noexcept;

        //------------------------------------------------------------------------
        // Table access
        //------------------------------------------------------------------------

        size_t table_count() const noexcept { return tables_.size(); }
        std::optional<table_view> table(table_id id) const noexcept;    
        std::vector<table_view>   tables() const noexcept;

        //------------------------------------------------------------------------
        // Column access
        //------------------------------------------------------------------------

        size_t column_count() const noexcept { return columns_.size(); }
        std::optional<column_view> column(column_id id) const noexcept;
        std::vector<column_view>   columns() const noexcept;

        //------------------------------------------------------------------------
        // Row access
        //------------------------------------------------------------------------

        size_t row_count() const noexcept { return rows_.size(); }
        std::optional<table_row_view> row(table_row_id id) const noexcept;
        std::vector<table_row_view>   rows() const noexcept;

        //------------------------------------------------------------------------
        // Key access
        //------------------------------------------------------------------------

        size_t key_count() const noexcept { return keys_.size(); }
        std::optional<key_view> key(std::string_view name) const noexcept;
        std::optional<key_view> key(key_id id) const noexcept;
        std::vector<key_view>   keys() const noexcept;

        category_id create_category(category_id id, std::string_view name, category_id parent);

        contamination_state contamination {contamination_state::clean};

    private:

        category_id create_root();

        //------------------------------------------------------------------------
        // Internal storage (fully normalised)
        //------------------------------------------------------------------------
        
        struct category_node
        {
            typedef category_id id_type;
            id_type _id() const noexcept { return id; }
            std::string_view _name() const noexcept { return name; }
            
            category_id              id;
            std::string              name;
            category_id              parent;
            std::vector<category_id> children;
            std::vector<table_id>    tables;
            std::vector<key_id>      keys; 
            semantic_state           semantic      {semantic_state::valid};
            contamination_state      contamination {contamination_state::clean};
        };

        struct table_node
        {
            typedef table_id id_type;
            id_type _id() const noexcept { return id; }
            std::string_view _name() const noexcept = delete;
            
            table_id                    id;
            category_id                 owner;
            // std::vector<struct column>  columns;
            std::vector<column_id>      columns;
            std::vector<table_row_id>   rows;   // authored order
            semantic_state              semantic      {semantic_state::valid};
            contamination_state         contamination {contamination_state::clean};
        };

        struct column_node
        {
            typedef column_id id_type;
            id_type _id() const noexcept { return col.id; }
            std::string_view _name() const noexcept { return col.name; }
            
            struct column   col;
            table_id        table;
            category_id     owner;

        };

        struct row_node
        {
            typedef table_row_id id_type;
            id_type _id() const noexcept { return id; }
            std::string_view _name() const noexcept = delete;
            
            table_row_id             id;
            table_id                 table;
            category_id              owner;
            std::vector<typed_value> cells;
            semantic_state           semantic      {semantic_state::valid};
            contamination_state      contamination {contamination_state::clean};
        };

        struct key_node
        {
            typedef key_id id_type;
            id_type _id() const noexcept { return id; }
            std::string_view _name() const noexcept { return name; }
            
            key_id               id;
            std::string          name;
            category_id          owner;
            value_type           type;
            type_ascription      type_source;
            typed_value          value;
            semantic_state       semantic      {semantic_state::valid};
            contamination_state  contamination {contamination_state::clean};
        };

        std::vector<category_node> categories_;
        std::vector<table_node>    tables_;
        std::vector<column_node>   columns_;
        std::vector<row_node>      rows_;
        std::vector<key_node>      keys_;

        bool row_is_valid(document::row_node const& r);
        bool table_is_valid(document::table_node const& t);

        template<typename T>
        typename std::vector<T>::iterator 
        find_node_by_id(std::vector<T> & cont, typename T::id_type id) noexcept;

        template<typename T>
        typename std::vector<T>::const_iterator 
        find_node_by_id(std::vector<T> const & cont, typename T::id_type id) const noexcept;

        template<typename T>
        typename std::vector<T>::const_iterator 
        find_node_by_name(std::vector<T> const & cont, std::string_view name) const noexcept;

        template<typename T>
        std::optional<typename node_to_view<T>::view_type>
        to_view(std::vector<T> const & cont, typename std::vector<T>::const_iterator it) const noexcept;


        //------------------------------------------------------------------------
        // View construction helpers
        //------------------------------------------------------------------------

        friend struct materialiser;
    };

//========================================================================
// Views
//========================================================================

    struct document::category_view
    {
        const document* doc;
        const category_node* node;

        category_id id() const noexcept { return node->id; }
        std::string_view name() const noexcept { return node->name; }
        bool is_root() const noexcept { return node->parent == category_id{}; }

        std::span<const category_id> children() const noexcept { return node->children; }
        std::span<const table_id> tables() const noexcept { return node->tables; }
        std::span<const key_id> keys() const noexcept { return node->keys;}

        std::optional<category_view> parent() const noexcept;
        std::optional<category_view> child(std::string_view name) const noexcept;
        std::optional<key_view> key(std::string_view name) const noexcept;

        bool is_locally_valid() const noexcept { return node->semantic == semantic_state::valid; }
        bool is_contaminated() const noexcept { return node->contamination == contamination_state::contaminated; }
    };

    struct document::table_view
    {
        const document* doc;
        const table_node* node;

        table_id id() const noexcept { return node->id; }
        std::span<const column_id> columns() const noexcept { return node->columns; }
        std::span<const table_row_id> rows() const noexcept { return node->rows; }

        category_view owner() const noexcept;
        std::optional<size_t> column_index(std::string_view name) const noexcept;        
        std::optional<size_t> column_index(column_id id) const noexcept;        

        bool is_locally_valid() const noexcept { return node->semantic == semantic_state::valid; }
        bool is_contaminated() const noexcept { return node->contamination == contamination_state::contaminated; }
    };

    struct document::column_view
    {
        const document*    doc;
        const column_node* node;

        column_id id() const noexcept { return node->col.id; }
        std::string_view name() const noexcept { return node->col.name; }
        value_type type() const noexcept { return node->col.type; }

        table_view table() const noexcept;
        category_view owner() const noexcept;
        size_t index() const noexcept;

        bool is_locally_valid() const noexcept { return node->col.semantic == semantic_state::valid; }
    };

    struct document::table_row_view
    {
        const document* doc;
        const row_node* node;

        table_row_id id() const noexcept { return node->id; }
        std::span<const typed_value> cells() const noexcept { return node->cells; }

        category_view owner() const noexcept;
        table_view table() const noexcept;

        bool is_locally_valid() const noexcept { return node->semantic == semantic_state::valid; }
        bool is_contaminated() const noexcept { return node->contamination == contamination_state::contaminated; }
    };

    struct document::key_view
    {
        const document* doc;
        const key_node* node;

        key_id id() const noexcept { return node->id; }
        const std::string& name() const noexcept { return node->name; }
        const typed_value& value() const noexcept { return node->value; }
        category_view owner() const noexcept;
        bool is_locally_valid() const noexcept { return node->semantic == semantic_state::valid; }
        bool is_contaminated() const noexcept { return node->contamination == contamination_state::contaminated; }
    };


//========================================================================
// document member implementations
//========================================================================

    inline category_id document::create_root()
    {
        category_node root;
        root.id     = category_id{0};
        root.name   = detail::ROOT_CATEGORY_NAME.data();
        root.parent = invalid_id<category_tag>();

        categories_.push_back(std::move(root));
        return category_id{0};
    }    

    inline category_id document::create_category( category_id id, std::string_view name, category_id parent )
    {
        category_node node;
        node.id     = id;
        node.name   = std::string(name);
        node.parent = parent;

        categories_.push_back(std::move(node));

        auto it = std::ranges::find_if(categories_, [parent](category_node const & cn){return cn.id == parent;});
        if (it != categories_.end())
            it->children.push_back(id);

        return id;
    }

    inline std::optional<document::category_view>
    document::root() const noexcept
    {
        if (categories_.empty())
            return std::nullopt;

        return category_view{ this, &categories_.front() };
    }

    namespace
    {
        template<> struct node_to_view<document::category_node> { typedef document::category_view   view_type; };
        template<> struct node_to_view<document::table_node>    { typedef document::table_view      view_type; };
        template<> struct node_to_view<document::column_node>   { typedef document::column_view     view_type; };
        template<> struct node_to_view<document::row_node>      { typedef document::table_row_view  view_type; };
        template<> struct node_to_view<document::key_node>      { typedef document::key_view        view_type; };
    }

    template<typename T>
    std::optional<typename node_to_view<T>::view_type>
    document::to_view(std::vector<T> const & cont, typename std::vector<T>::const_iterator it) const noexcept
    {
        if (it != cont.end())
           return typename node_to_view<T>::view_type{this, &*it};        
        return std::nullopt;
    }

    template<typename T>
    typename std::vector<T>::const_iterator
    document::find_node_by_name(std::vector<T> const & cont, std::string_view name) const noexcept
    {
        return std::ranges::find_if(cont, [&name](auto const & node) {
            return node._name() == name;
        });
    }

    std::optional<document::category_view> document::category(std::string_view name) const noexcept
    {
        return to_view(categories_, find_node_by_name(categories_, name));
    }

    std::optional<document::key_view> document::key(std::string_view name) const noexcept
    {
        return to_view(keys_, find_node_by_name(keys_, name));
    }

    std::optional<size_t> document::table_view::column_index(std::string_view name) const noexcept
    {
        auto & cols = node->columns;

        auto it = std::ranges::find_if(cols, [&](column_id col_id)
        {
            auto c = doc->column(col_id);
            if (c.has_value())
                return c->name() == name;
            return false;
        });

        if (it != cols.end())
            return std::distance(cols.begin(), it);

        return std::nullopt;
    }

    std::optional<size_t> document::table_view::column_index(column_id id) const noexcept
    {
        auto & cols = node->columns;
        if (auto it = std::ranges::find(cols, id); it != cols.end())
            return std::distance(cols.begin(), it);

        return std::nullopt;
    }


    template<typename T>
    typename std::vector<T>::iterator
    document::find_node_by_id(std::vector<T> & cont, typename T::id_type id) noexcept
    {
        return std::ranges::find_if(cont, [id](auto const & node) {
            return node._id() == id;
        });
    }

    template<typename T>
    typename std::vector<T>::const_iterator
    document::find_node_by_id(std::vector<T> const & cont, typename T::id_type id) const noexcept
    {
        return std::ranges::find_if(cont, [id](auto const & node) {
            return node._id() == id;
        });
    }

    inline std::optional<document::category_view>
    document::category(category_id id) const noexcept { return to_view(categories_, find_node_by_id(categories_, id)); }

    inline std::optional<document::table_view>
    document::table(table_id id) const noexcept { return to_view(tables_, find_node_by_id(tables_, id)); }

    inline std::optional<document::column_view> 
    document::column(column_id id) const noexcept { return to_view(columns_, find_node_by_id(columns_, id)); }

    inline std::optional<document::table_row_view>
    document::row(table_row_id id) const noexcept { return to_view(rows_, find_node_by_id(rows_, id)); }

    inline std::optional<document::key_view>
    document::key(key_id id) const noexcept { return to_view(keys_, find_node_by_id(keys_, id)); }

    std::optional<document::category_view> 
    document::category_view::parent() const noexcept 
    { 
        if (node->parent != invalid_id<category_tag>())
            return doc->to_view(doc->categories_, doc->find_node_by_id(doc->categories_, node->parent));
        return std::nullopt;
    }    

    std::optional<document::category_view> 
    document::category_view::child(std::string_view name) const noexcept
    {
        for (auto child_id : node->children)
            if (auto child_view = doc->category(child_id); child_view.has_value() && child_view->name() == name)
                return child_view;
        return std::nullopt;
    }

    std::optional<document::key_view> 
    document::category_view::key(std::string_view name) const noexcept
    {
        for (auto kid : node->keys)
            if (auto k_view = doc->key(kid); k_view.has_value() && k_view->name() == name)
                return k_view;
        return std::nullopt;
    }

    document::category_view document::table_view::owner()     const noexcept { return *doc->to_view(doc->categories_, doc->find_node_by_id(doc->categories_, node->owner)); }
    document::category_view document::column_view::owner()    const noexcept { return *doc->to_view(doc->categories_, doc->find_node_by_id(doc->categories_, node->owner)); }
    document::table_view    document::column_view::table()    const noexcept { return *doc->to_view(doc->tables_,     doc->find_node_by_id(doc->tables_,     node->table)); }
    document::category_view document::table_row_view::owner() const noexcept { return *doc->to_view(doc->categories_, doc->find_node_by_id(doc->categories_, node->owner)); }
    document::table_view    document::table_row_view::table() const noexcept { return *doc->to_view(doc->tables_,     doc->find_node_by_id(doc->tables_,     node->table)); }
    document::category_view document::key_view::owner()       const noexcept { return *doc->to_view(doc->categories_, doc->find_node_by_id(doc->categories_, node->owner)); }

    size_t document::column_view::index() const noexcept
    {
        auto cols = table().columns();
        size_t count = 0;
        for (auto c : cols)
        {
            if (c == id()) break;
            ++count;
        }
        assert (count < cols.size());
        return count;
    }

    namespace 
    {
        template<typename View, typename T2>
        std::vector<View> collect_views(document const * doc_ptr, std::vector<T2> const & cont)
        {
            std::vector<View> res;
            for (auto const & c : cont)
                res.push_back(View{ doc_ptr, &c });
            return res;
        }
    }

    std::vector<document::category_view>  document::categories() const noexcept { return collect_views<category_view>(this, categories_); }
    std::vector<document::table_view>     document::tables()     const noexcept { return collect_views<table_view>(this, tables_); }
    std::vector<document::column_view>    document::columns()    const noexcept { return collect_views<column_view>(this, columns_); }
    std::vector<document::table_row_view> document::rows()       const noexcept { return collect_views<table_row_view>(this, rows_); }
    std::vector<document::key_view>       document::keys()       const noexcept { return collect_views<key_view>(this, keys_); }

} // namespace arf

#endif // ARF_DOCUMENT_HPP

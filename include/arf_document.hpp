// arf_document.hpp - A Readable Format (Arf!) - Authoritative Document Model
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_DOCUMENT_HPP
#define ARF_DOCUMENT_HPP

#include "arf_core.hpp"

#include <cassert>
#include <iostream>
#include <ranges>
#include <span>

namespace arf
{
    template<typename T> struct node_to_view;

    class document
    {
    public:
        //------------------------------------------------------------------------
        // Public, read-only views
        //------------------------------------------------------------------------

        struct category_view;
        struct table_view;
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
        std::optional<category_view> category(category_id id) const noexcept;
        std::vector<category_view>   categories() const noexcept;

        //------------------------------------------------------------------------
        // Table access
        //------------------------------------------------------------------------

        size_t table_count() const noexcept { return tables_.size(); }
        std::optional<table_view> table(table_id id) const noexcept;    
        std::vector<table_view>   tables() const noexcept;

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
            
            table_id                  id;
            category_id               owner;
            std::vector<column>       columns;
            std::vector<table_row_id> rows;   // authored order
            semantic_state            semantic      {semantic_state::valid};
            contamination_state       contamination {contamination_state::clean};
        };

        struct row_node
        {
            typedef table_row_id id_type;
            
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
        std::vector<row_node>      rows_;
        std::vector<key_node>      keys_;

        bool row_is_valid(document::row_node const& r);
        bool table_is_valid(document::table_node const& t);

        template<typename T>
        std::optional<typename node_to_view<T>::view_type>
        find_view(std::vector<T> const & cont, typename T::id_type id) const noexcept;

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
        bool is_locally_valid() const noexcept { return node->semantic == semantic_state::valid; }
        bool is_contaminated() const noexcept { return node->contamination == contamination_state::contaminated; }
        std::optional<category_view> parent() const noexcept ;
    };

    struct document::table_view
    {
        const document* doc;
        const table_node* node;

        table_id id() const noexcept { return node->id; }
        std::span<const column> columns() const noexcept { return node->columns; }
        std::span<const table_row_id> rows() const noexcept { return node->rows; }
        bool is_locally_valid() const noexcept { return node->semantic == semantic_state::valid; }
        bool is_contaminated() const noexcept { return node->contamination == contamination_state::contaminated; }
        category_view owner() const noexcept;
    };

    struct document::table_row_view
    {
        const document* doc;
        const row_node* node;

        table_row_id id() const noexcept { return node->id; }
        std::span<const typed_value> cells() const noexcept { return node->cells; }
        bool is_locally_valid() const noexcept { return node->semantic == semantic_state::valid; }
        bool is_contaminated() const noexcept { return node->contamination == contamination_state::contaminated; }
        category_view owner() const noexcept;
        table_view table() const noexcept;
    };

    struct document::key_view
    {
        const document* doc;
        const key_node* node;

        key_id id() const noexcept { return node->id; }
        const std::string& name() const noexcept { return node->name; }
        const typed_value& value() const noexcept { return node->value; }
        bool is_locally_valid() const noexcept { return node->semantic == semantic_state::valid; }
        bool is_contaminated() const noexcept { return node->contamination == contamination_state::contaminated; }
        category_view owner() const noexcept;
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

    template<> struct node_to_view<document::category_node> { typedef document::category_view   view_type; };
    template<> struct node_to_view<document::table_node>    { typedef document::table_view      view_type; };
    template<> struct node_to_view<document::row_node>      { typedef document::table_row_view  view_type; };
    template<> struct node_to_view<document::key_node>      { typedef document::key_view        view_type; };

    template<typename T>
    std::optional<typename node_to_view<T>::view_type>
    document::find_view(std::vector<T> const & cont, typename T::id_type id) const noexcept
    {
        auto it = std::ranges::find_if(cont, [id](auto const & node)
        {
            return node.id == id;
        });

        if (it != cont.end())
            return typename node_to_view<T>::view_type{this, &*it};
        
        return std::nullopt;
    }

    inline std::optional<document::category_view>
    document::category(category_id id) const noexcept { return find_view(categories_, id); }

    inline std::optional<document::table_view>
    document::table(table_id id) const noexcept { return find_view(tables_, id); }

    inline std::optional<document::table_row_view>
    document::row(table_row_id id) const noexcept { return find_view(rows_, id); }

    inline std::optional<document::key_view>
    document::key(key_id id) const noexcept { return find_view(keys_, id); }

    std::optional<document::category_view> document::category_view::parent() const noexcept 
    { 
        if (node->parent != invalid_id<category_tag>())
            return doc->find_view(doc->categories_, node->parent);
        return std::nullopt;
    }
    document::category_view document::table_view::owner()     const noexcept { return *doc->find_view(doc->categories_, node->owner); }
    document::category_view document::table_row_view::owner() const noexcept { return *doc->find_view(doc->categories_, node->owner); }
    document::table_view    document::table_row_view::table() const noexcept { return *doc->find_view(doc->tables_,     node->table); }
    document::category_view document::key_view::owner()       const noexcept { return *doc->find_view(doc->categories_, node->owner); }

    std::vector<document::category_view> document::categories() const noexcept
    {
        std::vector<category_view> res;
        for (auto const & c : categories_)
            res.push_back(category_view{ this, &c });
        return res;
    }
    std::vector<document::table_view> document::tables() const noexcept
    {
        std::vector<table_view> res;
        for (auto const & c : tables_)
            res.push_back(table_view{ this, &c });
        return res;
    }
    std::vector<document::table_row_view> document::rows() const noexcept
    {
        std::vector<table_row_view> res;
        for (auto const & c : rows_)
            res.push_back(table_row_view{ this, &c });
        return res;
    }
    std::vector<document::key_view> document::keys() const noexcept
    {
        std::vector<key_view> res;
        for (auto const & c : keys_)
            res.push_back(key_view{ this, &c });
        return res;
    }

} // namespace arf

#endif // ARF_DOCUMENT_HPP

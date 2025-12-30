// arf_document.hpp - A Readable Format (Arf!) - Authoritative Document Model
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_DOCUMENT_HPP
#define ARF_DOCUMENT_HPP

#include "arf_core.hpp"

#include <span>
#include <cassert>
#include <ranges>

namespace arf
{
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

        size_t category_count() const noexcept
        {
            return categories_.size();
        }

        std::optional<category_view> root() const noexcept;

        std::optional<category_view> category(category_id id) const noexcept;

        //------------------------------------------------------------------------
        // Table access
        //------------------------------------------------------------------------

        size_t table_count() const noexcept
        {
            return tables_.size();
        }

        std::optional<table_view> table(table_id id) const noexcept;

        //------------------------------------------------------------------------
        // Row access
        //------------------------------------------------------------------------

        size_t row_count() const noexcept
        {
            return rows_.size();
        }

        std::optional<table_row_view> row(table_row_id id) const noexcept;

        //------------------------------------------------------------------------
        // Key access
        //------------------------------------------------------------------------

        size_t key_count() const noexcept
        {
            return keys_.size();
        }

        std::optional<key_view> key(key_id id) const noexcept;

        category_id create_category(category_id id, std::string_view name, category_id parent);

    private:

        category_id create_root();

        //------------------------------------------------------------------------
        // Internal storage (fully normalised)
        //------------------------------------------------------------------------
        
        struct category_node
        {
            category_id              id;
            std::string              name;
            category_id              parent;
            std::vector<category_id> children;
            std::vector<table_id>    tables;
            std::vector<key_id>      keys; 
        };

        struct table_node
        {
            table_id                  id;
            category_id               owner;
            std::vector<column>       columns;
            std::vector<table_row_id> rows;   // authored order
        };

        struct row_node
        {
            table_row_id             id;
            table_id                 table;
            category_id              owner;
            std::vector<typed_value> cells;
        };

        struct key_node
        {
            std::string     name;
            category_id     owner;
            value_type      type;
            type_ascription type_source;
            typed_value     value;
            semantic_state  semantic = semantic_state::valid;
        };

        std::vector<category_node> categories_;
        std::vector<table_node>    tables_;
        std::vector<row_node>      rows_;
        std::vector<key_node>      keys_;

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

        std::string_view name() const noexcept
        {
            return node->name;
        }

        bool is_root() const noexcept
        {
            return node->parent == category_id{};
        }

        std::span<const category_id> children() const noexcept
        {
            return node->children;
        }

        std::span<const table_id> tables() const noexcept
        {
            return node->tables;
        }

        std::span<const key_id> keys() const noexcept
        {
            return node->keys;
        }
    };

    struct document::table_view
    {
        const document* doc;
        const table_node* node;

        std::span<const column> columns() const noexcept
        {
            return node->columns;
        }

        std::span<const table_row_id> rows() const noexcept
        {
            return node->rows;
        }
    };

    struct document::table_row_view
    {
        const document* doc;
        const row_node* node;

        std::span<const typed_value> cells() const noexcept
        {
            return node->cells;
        }
    };

    struct document::key_view
    {
        const document* doc;
        const key_node* node;

        const std::string& name() const noexcept
        {
            return node->name;
        }

        const typed_value& value() const noexcept
        {
            return node->value;
        }
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

        // Link parent → child
        if (parent.val < categories_.size())
            categories_[parent.val].children.push_back(id);

        return id;
    }

    inline std::optional<document::category_view>
    document::root() const noexcept
    {
        if (categories_.empty())
            return std::nullopt;

        return category_view{ this, &categories_.front() };
    }

    inline std::optional<document::category_view>
    document::category(category_id id) const noexcept
    {
        if (id.val >= categories_.size())
            return std::nullopt;

        return category_view{ this, &categories_[id.val] };
    }

    inline std::optional<document::table_view>
    document::table(table_id id) const noexcept
    {
        if (id.val >= tables_.size())
            return std::nullopt;

        return table_view{ this, &tables_[id.val] };
    }

    inline std::optional<document::table_row_view>
    document::row(table_row_id id) const noexcept
    {
        if (id.val >= rows_.size())
            return std::nullopt;

        return table_row_view{ this, &rows_[id.val] };
    }

    inline std::optional<document::key_view>
    document::key(key_id id) const noexcept
    {
        if (id.val >= keys_.size())
            return std::nullopt;

        return key_view{ this, &keys_[id.val] };
    }

} // namespace arf

#endif // ARF_DOCUMENT_HPP

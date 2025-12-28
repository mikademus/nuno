// arf_document.hpp - A Readable Format (Arf!) - Authoritative Document Model
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_DOCUMENT_HPP
#define ARF_DOCUMENT_HPP

#include "arf_core.hpp"
#include "arf_parser.hpp"

#include <span>
#include <cassert>

namespace arf
{
//========================================================================
// Forward declarations
//========================================================================

    class arf_document;
    std::optional<arf_document> load(std::string_view text);
    std::optional<arf_document> materialise(const document& cst);

//========================================================================
// Authoritative document
//========================================================================

    class arf_document
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

        arf_document() = default;

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


    private:
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
        };

        std::vector<category_node> categories_;
        std::vector<table_node>    tables_;
        std::vector<row_node>      rows_;
        std::vector<key_node>      keys_;

        //------------------------------------------------------------------------
        // View construction helpers
        //------------------------------------------------------------------------

        friend std::optional<arf_document> materialise(const document& cst);
    };

//========================================================================
// Views
//========================================================================

    struct arf_document::category_view
    {
        const arf_document* doc;
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

    struct arf_document::table_view
    {
        const arf_document* doc;
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

    struct arf_document::table_row_view
    {
        const arf_document* doc;
        const row_node* node;

        std::span<const typed_value> cells() const noexcept
        {
            return node->cells;
        }
    };

    struct arf_document::key_view
    {
        const arf_document* doc;
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
// materialise()
//========================================================================

    inline std::optional<arf_document> materialise(const document& cst)
    {
        arf_document doc;

        //========================================================================
        // 1. Copy categories
        //========================================================================

        doc.categories_.reserve(cst.categories.size());
        for (auto const& c : cst.categories)
        {
            arf_document::category_node node;
            node.id     = c.id;
            node.name   = c.name;
            node.parent = c.parent;
            doc.categories_.push_back(std::move(node));
        }

        // Build child lists
        for (auto const& cat : doc.categories_)
        {
            if (cat.parent.val < doc.categories_.size())
                doc.categories_[cat.parent.val].children.push_back(cat.id);
        }

        //========================================================================
        // 2. Copy tables (anonymous, positional)
        //========================================================================

        doc.tables_.reserve(cst.tables.size());
        for (auto const& t : cst.tables)
        {
            arf_document::table_node node;
            node.id      = t.id;
            node.owner   = t.owning_category;
            node.columns = t.columns;
            node.rows    = t.rows;

            doc.tables_.push_back(std::move(node));
            doc.categories_[t.owning_category.val].tables.push_back(t.id);
        }

        //========================================================================
        // 3. Copy rows (direct table binding, no scanning)
        //========================================================================

        doc.rows_.reserve(cst.rows.size());
        for (auto const& r : cst.rows)
        {
            arf_document::row_node node;
            node.id    = r.id;
            node.owner = r.owning_category;
            node.cells = r.cells;

            // Table ownership is already encoded in cst.tables[].rows
            table_id table {};
            for (auto const& t : doc.tables_)
            {
                if (std::find(t.rows.begin(), t.rows.end(), r.id) != t.rows.end())
                {
                    table = t.id;
                    break;
                }
            }

            if (table == table_id{})
                return std::nullopt; // orphan row → semantic error

            node.table = table;
            doc.rows_.push_back(std::move(node));
        }

        //========================================================================
        // 4. Materialise keys via event walk
        //========================================================================

        category_id current {0}; // root

        for (auto const& ev : cst.events)
        {
            switch (ev.kind)
            {
                case parse_event_kind::category_open:
                    current = std::get<category_id>(ev.target);
                    break;

                case parse_event_kind::category_close:
                    current = doc.categories_[current.val].parent;
                    break;

                case parse_event_kind::key_value:
                {
                    auto pos = ev.text.find('=');
                    if (pos == std::string::npos)
                        return std::nullopt;

                    std::string lhs = std::string(trim_sv(ev.text.substr(0, pos)));
                    std::string rhs = std::string(trim_sv(ev.text.substr(pos + 1)));

                    // ------------------------------------------------------------
                    // Parse name[:type]
                    // ------------------------------------------------------------

                    std::string name;
                    std::optional<value_type> declared_type;

                    if (auto type_pos = lhs.find(':'); type_pos != std::string::npos)
                    {
                        name = to_lower(lhs.substr(0, type_pos));

                        auto type_sv = trim_sv(lhs.substr(type_pos + 1));
                        declared_type = parse_declared_type(type_sv);
                        if (!declared_type)
                            return std::nullopt; // unknown declared type
                    }
                    else
                    {
                        name = to_lower(lhs);
                    }

                    auto& cat = doc.categories_[current];

                    // ------------------------------------------------------------
                    // Collision check (keys only)
                    // ------------------------------------------------------------

                    for (auto kid : cat.keys)
                    {
                        if (doc.keys_[kid.val].name == name)
                            return std::nullopt;
                    }

                    // ------------------------------------------------------------
                    // Determine type (classification only)
                    // ------------------------------------------------------------

                    value_type final_type = declared_type
                        ? *declared_type
                        : infer_value_type(rhs);

                    // ------------------------------------------------------------
                    // Construct key
                    // ------------------------------------------------------------

                    arf_document::key_node kn;
                    kn.name  = name;
                    kn.owner = current;
                    kn.type  = final_type;
                    kn.type_source = declared_type
                        ? type_ascription::declared
                        : type_ascription::tacit;

                    typed_value tv;
                    tv.type           = kn.type;
                    tv.type_source    = kn.type_source;
                    tv.origin         = value_locus::key_value;
                    tv.source_literal = rhs;

                    kn.value = std::move(tv);

                    key_id kid{ doc.keys_.size() };
                    doc.keys_.push_back(std::move(kn));
                    cat.keys.push_back(kid);

                    break;
                }

                default:
                    break;
            }
        }

        return doc;
    }

//========================================================================
// load
//========================================================================

    inline std::optional<arf_document> load(std::string_view text)
    {
        auto parsed = parse(text);
        return materialise(parsed.doc);
    }

//========================================================================
// document member implementations
//========================================================================

    inline std::optional<arf_document::category_view>
    arf_document::root() const noexcept
    {
        if (categories_.empty())
            return std::nullopt;

        return category_view{ this, &categories_.front() };
    }

    inline std::optional<arf_document::category_view>
    arf_document::category(category_id id) const noexcept
    {
        if (id.val >= categories_.size())
            return std::nullopt;

        return category_view{ this, &categories_[id.val] };
    }

    inline std::optional<arf_document::table_view>
    arf_document::table(table_id id) const noexcept
    {
        if (id.val >= tables_.size())
            return std::nullopt;

        return table_view{ this, &tables_[id.val] };
    }

    inline std::optional<arf_document::table_row_view>
    arf_document::row(table_row_id id) const noexcept
    {
        if (id.val >= rows_.size())
            return std::nullopt;

        return table_row_view{ this, &rows_[id.val] };
    }

    inline std::optional<arf_document::key_view>
    arf_document::key(key_id id) const noexcept
    {
        if (id.val >= keys_.size())
            return std::nullopt;

        return key_view{ this, &keys_[id.val] };
    }

} // namespace arf

#endif // ARF_DOCUMENT_HPP

// arf_reflect.hpp - A Readable Format (Arf!) - Reflection Interface
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.
//
// This reflection surface is *value-centred* and *address-oriented*.
// There are no node identities, no row indices, no cell objects.
// Only values exist. Everything else is an address that can reach them.

#ifndef ARF_REFLECT_HPP
#define ARF_REFLECT_HPP

#include "arf_document.hpp"

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace arf::reflect
{
    //============================================================
    // Address steps (canonical, ID-based)
    //============================================================

    struct category_step   { category_id   id; };
    struct key_step        { key_id        id; };
    struct table_step      { table_id      id; };
    struct row_step        { table_row_id  id; };
    struct column_step     { column_id     id; };
    struct array_index_step{ size_t        index; };

    using address_step = std::variant<
        category_step,
        key_step,
        table_step,
        row_step,
        column_step,
        array_index_step
    >;

    using address = std::vector<address_step>;

    //============================================================
    // Evaluation context
    //============================================================

    struct context
    {
        const document* doc = nullptr;

        std::optional<document::category_view> category;
        std::optional<document::table_view>    table;
        std::optional<table_row_id>            row;

        const typed_value* value = nullptr;

        void reset_value()
        {
            value = nullptr;
            row.reset();
            table.reset();
        }
    };

    //============================================================
    // Step application
    //============================================================

    inline bool apply(context& ctx, const category_step& s)
    {
        auto cat = ctx.doc->category(s.id);
        if (!cat)
            return false;
        
        if (ctx.category && cat->parent()->id() != ctx.category->id())
            return false;

        ctx.category = *cat;
        ctx.reset_value();
        return true;
    }

    inline bool apply(context& ctx, const key_step& s)
    {
        if (!ctx.category)
            return false;

        auto key = ctx.doc->key(s.id);
        if (!key || key->owner().id() != ctx.category->id())
            return false;

        ctx.value = &key->value();
        return true;
    }

    inline bool apply(context& ctx, const table_step& s)
    {
        if (!ctx.category)
            return false;

        auto tbl = ctx.doc->table(s.id);
        if (!tbl || tbl->owner().id() != ctx.category->id())
            return false;

        ctx.table = *tbl;
        ctx.reset_value();
        return true;
    }

    inline bool apply(context& ctx, const row_step& s)
    {
        if (!ctx.table)
            return false;

        auto row = ctx.doc->row(s.id);
        if (!row || row->table().id() != ctx.table->id())
            return false;

        ctx.row = s.id;
        ctx.reset_value();
        return true;
    }

    inline bool apply(context& ctx, const column_step& s)
    {
        if (!ctx.table || !ctx.row)
            return false;

        auto col = ctx.doc->column(s.id);
        if (!col || col->table().id() != ctx.table->id())
            return false;

        auto row = ctx.doc->row(*ctx.row);
        if (!row)
            return false;

        auto idx = col->index();
        if (idx >= row->cells().size())
            return false;

        ctx.value = &row->cells()[idx];
        return true;
    }

    inline bool apply(context& ctx, const array_index_step& s)
    {
        if (!ctx.value)
            return false;

        auto* arr = std::get_if<std::vector<typed_value>>(&ctx.value->val);
        if (!arr || s.index >= arr->size())
            return false;

        ctx.value = &(*arr)[s.index];
        return true;
    }

    //============================================================
    // Address resolution
    //============================================================

    inline std::optional<const typed_value*>
    resolve(const document& doc, const address& addr)
    {
        context ctx;
        ctx.doc = &doc;
        ctx.category = doc.root();

        for (auto const& step : addr)
        {
            bool ok = std::visit(
                [&](auto const& s) { return apply(ctx, s); },
                step
            );

            if (!ok)
                return std::nullopt;
        }

        return ctx.value;
    }

    //============================================================
    // Fluent address builder
    //============================================================

    class address_builder
    {
        const document* doc_;
        address          addr_;
        context          ctx_;

    public:
        explicit address_builder(const document& doc)
            : doc_(&doc)
        {
            ctx_.doc = &doc;
            ctx_.category = doc.root();
        }

        const address& build() const { return addr_; }

        // ----- ID-based -----

        address_builder& category(category_id id)
        {
            category_step s{id};
            if (!apply(ctx_, s)) ctx_ = {};
            else addr_.push_back(s);
            return *this;
        }

        address_builder& key(key_id id)
        {
            key_step s{id};
            if (!apply(ctx_, s)) ctx_ = {};
            else addr_.push_back(s);
            return *this;
        }

        address_builder& table(table_id id)
        {
            table_step s{id};
            if (!apply(ctx_, s)) ctx_ = {};
            else addr_.push_back(s);
            return *this;
        }

        address_builder& row(table_row_id id)
        {
            row_step s{id};
            if (!apply(ctx_, s)) ctx_ = {};
            else addr_.push_back(s);
            return *this;
        }

        address_builder& column(column_id id)
        {
            column_step s{id};
            if (!apply(ctx_, s)) ctx_ = {};
            else addr_.push_back(s);
            return *this;
        }

        // ----- Name-based -----

        address_builder& category(std::string_view name)
        {
            if (!ctx_.category)
                return *this;

            auto child = ctx_.category->child(name);
            if (!child)
                return *this;

            return category(child->id());
        }

        address_builder& key(std::string_view name)
        {
            if (!ctx_.category)
                return *this;

            auto k = ctx_.category->key(name);
            if (!k)
                return *this;

            return key(k->id());
        }

        address_builder& column(std::string_view name)
        {
            if (!ctx_.table)
                return *this;

            auto idx = ctx_.table->column_index(name);
            if (!idx)
                return *this;

            auto col = ctx_.doc->column(ctx_.table->columns()[*idx]);
            if (!col)
                return *this;

            return column(col->id());
        }

        // ----- Convenience -----

        address_builder& local_table(size_t ordinal)
        {
            if (!ctx_.category)
                return *this;

            auto tables = ctx_.category->tables();
            if (ordinal >= tables.size())
                return *this;

            return table(tables[ordinal]);
        }

        address_builder& local_row(size_t ordinal)
        {
            if (!ctx_.table)
                return *this;

            auto rows = ctx_.table->rows();
            if (ordinal >= rows.size())
                return *this;

            return row(rows[ordinal]);
        }

        address_builder& local_column(size_t ordinal)
        {
            if (!ctx_.table)
                return *this;

            auto cols = ctx_.table->columns();
            if (ordinal >= cols.size())
                return *this;

            return column(cols[ordinal]);
        }

        address_builder& index(size_t i)
        {
            array_index_step s{i};
            if (!apply(ctx_, s)) ctx_ = {};
            else addr_.push_back(s);
            return *this;
        }
    };

    inline address_builder root(const document& doc)
    {
        return address_builder(doc);
    }

} // namespace arf::reflect

#endif // ARF_REFLECT_HPP

// arf_reflect.hpp - A Readable Format (Arf!) - Reflection Interface
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_REFLECT_HPP
#define ARF_REFLECT_HPP

#include "arf_document.hpp"

#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace arf
{
//========================================================================
// value_ref
//========================================================================

    class value_ref
    {
    public:
        explicit value_ref(const typed_value& v) : tv_(&v) {}

        value_type      type()        const { return tv_->type; }
        type_ascription type_source() const { return tv_->type_source; }
        value_locus     origin_site() const { return tv_->origin; }

        bool is_valid()        const { return tv_->semantic == semantic_state::valid; }
        bool is_contaminated() const { return tv_->contamination == contamination_state::contaminated; }

        const value& raw() const { return tv_->val; }

        std::optional<std::string_view> source_str() const
        {
            if (tv_->source_literal) return *tv_->source_literal;
            return std::nullopt;
        }

        bool is_scalar() const;
        bool is_array()  const;

        size_t array_size() const;
        std::optional<value_ref> array_element(size_t index) const;

        std::optional<std::string> as_string() const;
        std::optional<int64_t>     as_int()    const;
        std::optional<double>      as_float()  const;
        std::optional<bool>        as_bool()   const;

    private:
        const typed_value* tv_;
    };

//========================================================================
// table_column_ref
//========================================================================

    class table_column_ref
    {
    public:
        table_column_ref(const column& c, size_t index)
            : col_(&c), index_(index) {}

        const std::string& name() const { return col_->name; }
        size_t index() const { return index_; }

        value_type      type()        const { return col_->type; }
        type_ascription type_source() const { return col_->type_source; }

        bool is_valid() const { return col_->semantic == semantic_state::valid; }

    private:
        const column* col_;
        size_t index_;
    };

//========================================================================
// table_row_ref
//========================================================================

    class table_row_ref
    {
    public:
        table_row_ref(const document* doc, table_row_id id)
            : doc_(doc), id_(id) {}

        table_row_id id() const { return id_; }

        bool is_valid() const;
        bool is_contaminated() const;

        document::category_view owning_category() const;

        size_t cell_count() const;
        std::optional<value_ref> cell_value(size_t col_index) const;

        auto cells() const;

    private:
        const document* doc_;
        table_row_id id_;
    };

//========================================================================
// table_partition_ref
//========================================================================

    class table_ref;

    class table_partition_ref
    {
    public:
        table_partition_ref(const table_ref* table, category_id id);

        category_id id() const { return cat_id_; }
        std::string_view name() const;

        size_t direct_row_count() const;
        table_row_ref direct_row(size_t i) const;
        auto direct_rows() const;

        size_t row_count() const;
        table_row_ref row(size_t i) const;
        auto rows() const;

        bool has_parent() const;
        std::optional<table_partition_ref> parent() const;

        size_t child_count() const;
        table_partition_ref child(size_t i) const;
        auto children() const;

    private:
        const table_ref* table_;
        category_id cat_id_;
    };

//========================================================================
// table_ref
//========================================================================

    class table_ref
    {
    public:
        table_ref(const document* doc, table_id id)
            : doc_(doc), id_(id) {}

        table_id id() const { return id_; }

        bool is_valid() const;
        bool is_contaminated() const;

        size_t column_count() const;
        table_column_ref column(size_t i) const;
        std::optional<size_t> column_index(std::string_view name) const;
        auto columns() const;

        size_t row_count() const;
        table_row_ref row(size_t i) const;
        auto rows() const;

        table_partition_ref root_partition() const;
        size_t partition_count() const;
        table_partition_ref partition(size_t i) const;
        auto partitions() const;

        const document* document() const { return doc_; }

    private:
        const document* doc_;
        table_id id_;

        struct partition_info
        {
            category_id cat;
            category_id parent;
            std::vector<category_id> children;
            std::vector<table_row_id> direct_rows;
            std::vector<table_row_id> all_rows;
        };

        mutable std::vector<partition_info> partitions_;
        mutable std::unordered_map<category_id, size_t> partition_index_;
        mutable bool partitions_built_ = false;

        void build_partitions() const;
    };

//========================================================================
// value_ref impl
//========================================================================

    inline bool value_ref::is_scalar() const
    {
        return !std::holds_alternative<std::vector<typed_value>>(tv_->val);
    }

    inline bool value_ref::is_array() const
    {
        return std::holds_alternative<std::vector<typed_value>>(tv_->val);
    }

    inline size_t value_ref::array_size() const
    {
        auto* arr = std::get_if<std::vector<typed_value>>(&tv_->val);
        return arr ? arr->size() : 0;
    }

    inline std::optional<value_ref> value_ref::array_element(size_t index) const
    {
        auto* arr = std::get_if<std::vector<typed_value>>(&tv_->val);
        if (!arr || index >= arr->size()) return std::nullopt;
        return value_ref((*arr)[index]);
    }

//========================================================================
// table_row_ref impl
//========================================================================

    inline bool table_row_ref::is_valid() const
    {
        auto r = doc_->row(id_);
        return r && r->node->semantic == semantic_state::valid;
    }

    inline bool table_row_ref::is_contaminated() const
    {
        auto r = doc_->row(id_);
        return r && r->node->contamination == contamination_state::contaminated;
    }

    inline document::category_view table_row_ref::owning_category() const
    {
        return doc_->row(id_)->owner();
    }

    inline size_t table_row_ref::cell_count() const
    {
        auto r = doc_->row(id_);
        return r ? r->node->cells.size() : 0;
    }

    inline std::optional<value_ref> table_row_ref::cell_value(size_t col_index) const
    {
        auto r = doc_->row(id_);
        if (!r || col_index >= r->node->cells.size()) return std::nullopt;
        return value_ref(r->node->cells[col_index]);
    }

    inline auto table_row_ref::cells() const
    {
        struct range
        {
            const table_row_ref* row;

            struct it
            {
                const table_row_ref* row;
                size_t i;
                value_ref operator*() const { return *row->cell_value(i); }
                it& operator++() { ++i; return *this; }
                bool operator!=(const it& o) const { return i != o.i; }
            };

            it begin() const { return {row, 0}; }
            it end()   const { return {row, row->cell_count()}; }
        };

        return range{this};
    }

//========================================================================
// table_ref partition impl
//========================================================================

    inline void table_ref::build_partitions() const
    {
        if (partitions_built_) return;

        partitions_.clear();
        partition_index_.clear();

        auto owner = document()->table(id_)->owner().id();

        partitions_.push_back({owner, invalid_id<category_tag>(), {}, {}, {}});
        partition_index_[owner] = 0;

        auto visit = [&](auto&& self, category_id cat, category_id parent) -> void
        {
            size_t idx = partitions_.size();
            partitions_.push_back({cat, parent, {}, {}, {}});
            partition_index_[cat] = idx;

            if (parent != invalid_id<category_tag>())
                partitions_[partition_index_[parent]].children.push_back(cat);

            auto cv = doc_->category(cat);
            if (!cv) return;

            for (auto child : cv->children())
                self(self, child, cat);
        };

        auto root = doc_->category(owner);
        if (root)
        {
            for (auto child : root->children())
                visit(visit, child, owner);
        }

        for (auto row_id : document()->table(id_)->rows)
        {
            auto rv = doc_->row(row_id);
            if (!rv) continue;

            auto it = partition_index_.find(rv->owner().id());
            if (it != partition_index_.end())
                partitions_[it->second].direct_rows.push_back(row_id);
        }

        for (size_t i = partitions_.size(); i-- > 0;)
        {
            auto& p = partitions_[i];
            p.all_rows = p.direct_rows;
            for (auto c : p.children)
            {
                auto it = partition_index_.find(c);
                if (it != partition_index_.end())
                {
                    auto& cr = partitions_[it->second].all_rows;
                    p.all_rows.insert(p.all_rows.end(), cr.begin(), cr.end());
                }
            }
        }

        partitions_built_ = true;
    }

} // namespace arf

#endif

// arf_editor.hpp - A Readable Format (Arf!) - Document Editor
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_EDITOR_HPP
#define ARF_EDITOR_HPP

#include "arf_document.hpp"

namespace arf
{

    class editor
    {
    public:
        explicit editor(document& doc) noexcept
            : doc_(doc)
        {}

    //============================================================
    // Key / value manipulation
    //============================================================

        template<typename T>
        key_id append_key(
            category_id where,
            std::string_view name,
            T value,
            bool untyped = false
        );

        template<typename K, typename T>
        key_id insert_key_before(
            id<K> anchor,
            std::string_view name,
            T value,
            bool untyped = false
        );

        template<typename K, typename T>
        key_id insert_key_after(
            id<K> anchor,
            std::string_view name,
            T value,
            bool untyped = false
        );

        bool erase_key(key_id id);

    //============================================================
    // Comments
    //============================================================

        comment_id append_comment(
            category_id where,
            std::string_view text
        );

        template<typename K>
        comment_id insert_comment_before(
            id<K> anchor,
            std::string_view text
        );

        template<typename K>
        comment_id insert_comment_after(
            id<K> anchor,
            std::string_view text
        );

        bool erase_comment(comment_id id);

    //============================================================
    // Paragraphs (category scope only)
    //============================================================

        paragraph_id append_paragraph(
            category_id where,
            std::string_view text
        );

        template<typename K>
        paragraph_id insert_paragraph_before(
            id<K> anchor,
            std::string_view text
        );

        template<typename K>
        paragraph_id insert_paragraph_after(
            id<K> anchor,
            std::string_view text
        );

        bool erase_paragraph(paragraph_id id);

    //============================================================
    // Tables
    //============================================================

        table_id append_table(
            category_id where,
            std::vector<std::string> column_names
        );

        bool erase_table(table_id id);

        // Rows are table-scoped only
        table_row_id append_row(
            table_id table,
            std::vector<std::string> cells
        );

        bool erase_row(table_row_id id);

    private:
        document& doc_;

    private:
        //========================================================
        // Internal helpers — NEVER exposed
        //========================================================

        document::source_item_ref make_ref(key_id id)        noexcept;
        document::source_item_ref make_ref(comment_id id)    noexcept;
        document::source_item_ref make_ref(paragraph_id id)  noexcept;
        document::source_item_ref make_ref(table_id id)      noexcept;
        document::source_item_ref make_ref(table_row_id id)  noexcept;

        template<typename K>
        document::source_item_ref const* locate_anchor(id<K> anchor) const noexcept;

        bool scope_allows_paragraph(category_id where) const noexcept;
        bool scope_allows_key(category_id where) const noexcept;
    };

}

#endif
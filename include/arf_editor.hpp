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

        bool erase_key( key_id id );

    // Ordinal keys
    //----------------------------
        key_id append_key( category_id where, std::string_view name, value v, bool untyped = false );
        void set_key_value( key_id key, value val );

        template<typename K> key_id insert_key_before( id<K> anchor, std::string_view name, value v, bool untyped = false );
        template<typename K> key_id insert_key_after( id<K> anchor, std::string_view name, value v, bool untyped = false );

    // Array keys
    //----------------------------
        key_id append_key( category_id where, std::string_view name, std::vector<value> arr, bool untyped = false );
        void set_key_value( key_id key, std::vector<value> arr ); 
        
        template<typename K> key_id insert_key_before( id<K> anchor, std::string_view name, std::vector<value> arr, bool untyped = false );
        template<typename K> key_id insert_key_after( id<K> anchor, std::string_view name, std::vector<value> arr, bool untyped = false );

    //============================================================
    // Comments
    //============================================================

        bool erase_comment(comment_id id);

        comment_id append_comment( category_id where, std::string_view text );
        void set_comment( comment_id id, std::string_view text );

        template<typename K> comment_id insert_comment_before( id<K> anchor, std::string_view text );
        template<typename K> comment_id insert_comment_after( id<K> anchor, std::string_view text );

    //============================================================
    // Paragraphs (category scope only)
    //============================================================

        bool erase_paragraph(paragraph_id id);

        paragraph_id append_paragraph( category_id where, std::string_view text );
        void set_paragraph( paragraph_id id, std::string_view text );

        template<typename K> paragraph_id insert_paragraph_before( id<K> anchor, std::string_view text );
        template<typename K> paragraph_id insert_paragraph_after( id<K> anchor, std::string_view text );

    //============================================================
    // Tables
    //============================================================

    // Tables themselves
    //----------------------------
        bool erase_table(table_id id);

        table_id append_table( category_id where, std::vector<std::string> column_names );
        table_id append_table( category_id where, std::vector<std::pair<std::string, std::optional<value_type>>> columns );

        template<typename Tag> table_id insert_table_before( id<Tag> anchor, std::vector<std::string> column_names );
        template<typename Tag> table_id insert_table_before( id<Tag> anchor, std::vector<std::pair<std::string, std::optional<value_type>>> columns );

        template<typename Tag> table_id insert_table_after( id<Tag> anchor, std::vector<std::string> column_names );
        template<typename Tag> table_id insert_table_after( id<Tag> anchor, std::vector<std::pair<std::string, std::optional<value_type>>> columns );

    // Rows
    //----------------------------        
        bool erase_row(table_row_id id);

        table_row_id append_row( table_id table, std::vector<value> cells );

        template<typename Tag> table_row_id insert_row_before(id<Tag> anchor, std::vector<value> cells);
        template<typename Tag> table_row_id insert_row_after(id<Tag> anchor, std::vector<value> cells);

    // Columns
    //----------------------------        
        bool erase_column(column_id id);

        column_id append_column( table_id table_id, std::string_view name, std::optional<value_type> declared_type );

        column_id insert_column_before( column_id anchor, std::string_view name, std::optional<value_type> declared_type );
        column_id insert_column_after( column_id anchor, std::string_view name, std::optional<value_type> declared_type );

    // Cells
    //----------------------------        
        void set_cell_value( table_row_id row, column_id col, value val );
        void set_cell_value( table_row_id row, column_id col, std::vector<value> arr );

    //============================================================
    // Array element manipulation
    //============================================================

    // Key/value specialisation
    //-------------------------------
        void erase_array_element( key_id key, size_t index );

        void append_array_element( key_id key, value val );

        void set_array_element( key_id key, size_t index, value val );
        void set_array_elements( key_id key, std::vector<value> vals );

    // Table specialisation
    //-------------------------------
        void erase_array_element( table_row_id row, column_id col, size_t index );

        void append_array_element( table_row_id row, column_id col, value val );

        void set_array_element( table_row_id row, column_id col, size_t index, value val );
        void set_array_elements( table_row_id row, column_id col, std::vector<value> vals );

    //============================================================
    // Type control (explicit, opt-in)
    //============================================================

        bool set_key_type( key_id id, value_type type, type_ascription ascription = type_ascription::declared );
        bool set_column_type( column_id id, value_type type, type_ascription ascription = type_ascription::declared );

    //============================================================
    // Low-level access to document internals
    //============================================================

        // Provides access to the document's internal container
        // node corresponding to an entity ID.  
        // Warning: power-user territory and should be avoided in 
        // general use. 
        template<typename Tag>
        typename document::to_node_type<Tag>::type* 
        _unsafe_access_internal_document_container( id<Tag> id_ )
        {
            return doc_.get_node(id_);
        }

    private:

        document& doc_;

    //========================================================
    // Internal helpers; not exposed for clients
    //========================================================

        enum class insert_direction { before, after };

        template<typename Tag>
        document::source_item_ref const* locate_anchor(id<Tag> anchor) const noexcept;

        // Convert raw value to typed_value with validation
        typed_value make_array_element( 
            value val, 
            value_type expected_type, 
            value_locus origin
        );
        
        // Validate and set array, mark contamination if needed
        void update_array_and_check(
            typed_value& target_array,
            value_type expected_type,
            std::function<void()> mark_contaminated,
            std::function<void()> try_clear
        );        

        template<typename Tag>
        table_row_id insert_row_impl(
            id<Tag> anchor,
            std::vector<value> cells,
            insert_direction dir);

        template<typename EntityId, typename Tag, typename... AppendArgs>
        EntityId insert_category_child_before(
            id<Tag> anchor,
            EntityId (editor::*append_fn)(category_id, AppendArgs...),
            AppendArgs&&... args);

        template<typename EntityId, typename Tag, typename... AppendArgs>
        EntityId insert_category_child_after(
            id<Tag> anchor,
            EntityId (editor::*append_fn)(category_id, AppendArgs...),
            AppendArgs&&... args);

        template<typename EntityId, typename NodeType>
        bool erase_category_child(
            EntityId id,
            std::vector<NodeType>& storage);

        // Helper to create keys without adding to document's ordered_list
        key_id create_key_node_only(
            category_id where,
            std::string_view name,
            value v,
            bool untyped
        );

        column_id create_column_node_only( 
            table_id table, 
            std::string_view name, 
            std::optional<value_type> declared_type );

        table_row_id create_row_node_only(
            table_id table,
            std::vector<typed_value> cells
        );
    };

//================================================================================================================
//
// Editor implementations
//
//================================================================================================================

//========================================================
// Internal helpers; not exposed for clients
//========================================================

    template<typename Tag>
    document::source_item_ref const*
    editor::locate_anchor(id<Tag> anchor) const noexcept
    {
        // Anchor must exist in some category ordered_items
        for (auto& cat : doc_.categories_)
        {
            for (auto& item : cat.ordered_items)
            {
                if (std::holds_alternative<id<Tag>>(item.id) &&
                    std::get<id<Tag>>(item.id) == anchor)
                    return &item;
            }
        }
        return nullptr;
    }

    inline typed_value editor::make_array_element(
        value val,
        value_type expected_type,
        value_locus origin
    )
    {
        typed_value elem;
        elem.val = std::move(val);
        elem.origin = origin;
        elem.creation = creation_state::generated;
        elem.is_edited = false;
        
        // Determine held type from variant
        elem.type = elem.held_type();
        
        // Validate against expected array element type
        value_type expected_elem_type;
        switch (expected_type) 
        {
            case value_type::int_array:    expected_elem_type = value_type::integer; break;
            case value_type::float_array:  expected_elem_type = value_type::decimal; break;
            case value_type::string_array: expected_elem_type = value_type::string; break;
            default:
                // Untyped array or unresolved
                elem.type_source = type_ascription::tacit;
                elem.semantic = semantic_state::valid;
                elem.contamination = contamination_state::clean;
                return elem;
        }
        
        // Check type match
        if (elem.type == expected_elem_type) 
        {
            elem.type_source = type_ascription::declared;
            elem.semantic = semantic_state::valid;
            elem.contamination = contamination_state::clean;
        } 
        else 
        {
            // Type mismatch → invalid element
            elem.type_source = type_ascription::tacit;
            elem.semantic = semantic_state::invalid;
            elem.contamination = contamination_state::clean; // Element itself clean, but invalid
        }
        
        return elem;
    }

    void editor::update_array_and_check(
        typed_value& target_array,
        value_type expected_type,
        std::function<void()> mark_contaminated,
        std::function<void()> try_clear
    )
    {
        if (!is_array(target_array)) return;
        
        auto& arr = std::get<std::vector<typed_value>>(target_array.val);
        
        bool has_invalid = false;
        for (auto& elem : arr)
        {
            if (elem.semantic == semantic_state::invalid ||
                elem.contamination == contamination_state::contaminated)
            {
                has_invalid = true;
                break;
            }
        }
        
        if (has_invalid)
        {
            target_array.contamination = contamination_state::contaminated;
            mark_contaminated();
        }
        else
        {
            target_array.contamination = contamination_state::clean;
            try_clear();
        }
    }

    template<typename EntityId, typename Tag, typename... AppendArgs>
    EntityId editor::insert_category_child_before(
        id<Tag> anchor,
        EntityId (editor::*append_fn)(category_id, AppendArgs...),
        AppendArgs&&... args)
    {
        auto* ref = locate_anchor(anchor);
        if (!ref) return invalid_id<typename EntityId::tag_type>();

        auto* anchor_node = doc_.get_node(anchor);
        if (!anchor_node) return invalid_id<typename EntityId::tag_type>();

        category_id where = anchor_node->owner;

        EntityId id = (this->*append_fn)(where, std::forward<AppendArgs>(args)...);
        if (!valid(id)) return id;

        auto* cat = doc_.get_node(where);
        auto it = std::ranges::find(cat->ordered_items, *ref);

        if (it != cat->ordered_items.end())
        {
            std::erase(cat->ordered_items, document::source_item_ref{id});
            cat->ordered_items.insert(it, {id});
        }

        return id;
    }

    template<typename EntityId, typename Tag, typename... AppendArgs>
    EntityId editor::insert_category_child_after(
        id<Tag> anchor,
        EntityId (editor::*append_fn)(category_id, AppendArgs...),
        AppendArgs&&... args)
    {
        auto* ref = locate_anchor(anchor);
        if (!ref) return invalid_id<typename EntityId::tag_type>();

        auto* anchor_node = doc_.get_node(anchor);
        if (!anchor_node) return invalid_id<typename EntityId::tag_type>();

        category_id where = anchor_node->owner;

        EntityId id = (this->*append_fn)(where, std::forward<AppendArgs>(args)...);
        if (!valid(id)) return id;

        auto* cat = doc_.get_node(where);
        auto it = std::ranges::find(cat->ordered_items, *ref);

        if (it != cat->ordered_items.end())
        {
            ++it;
            std::erase(cat->ordered_items, document::source_item_ref{id});
            cat->ordered_items.insert(it, {id});
        }

        return id;
    }    

    template<typename EntityId, typename NodeType>
    bool editor::erase_category_child(
        EntityId id,
        std::vector<NodeType>& storage)
    {
        auto* node = doc_.get_node(id);
        if (!node) return false;

        auto* cat = doc_.get_node(node->owner);
        if (!cat) return false;

        std::erase_if(cat->ordered_items, [&](auto const& r) {
            return std::holds_alternative<EntityId>(r.id)
                && std::get<EntityId>(r.id) == id;
        });

        storage.erase(
            std::ranges::find_if(storage, [&](auto const& n) {
                return n.id == id;
            })
        );

        return true;
    }

    key_id editor::create_key_node_only(
        category_id where,
        std::string_view name,
        value v,
        bool untyped)
    {
        auto* cat = doc_.get_node(where);
        if (!cat)
            return invalid_id<key_tag>();

        key_id id = doc_.create_key_id();

        document::key_node kn;
        kn.id        = id;
        kn.name      = std::string(name);
        kn.owner     = where;
        kn.creation  = creation_state::generated;
        kn.is_edited = true;

        kn.type        = untyped ? value_type::unresolved : held_type(v);
        kn.type_source = untyped ? type_ascription::tacit
                                : type_ascription::declared;

        kn.value.val           = std::move(v);
        kn.value.type          = kn.type;
        kn.value.type_source   = kn.type_source;
        kn.value.origin        = value_locus::key_value;
        kn.value.semantic      = semantic_state::valid;
        kn.value.contamination = contamination_state::clean;
        kn.value.creation      = creation_state::generated;

        doc_.keys_.push_back(std::move(kn));
        cat->keys.push_back(id);

        return id;
    }

/*    
    inline table_id editor::create_table_node_only(category_id owner)
    {
        table_id id = doc_.create_table_id();

        document::table_node tbl;
        tbl.id            = id;
        tbl.owner         = owner;
        tbl.creation      = creation_state::generated;
        tbl.is_edited     = true;
        tbl.semantic      = semantic_state::valid;
        tbl.contamination = contamination_state::clean;

        doc_.tables_.push_back(std::move(tbl));

        return id;
    }
*/

    inline column_id editor::create_column_node_only(
        table_id table,
        std::string_view name, 
        std::optional<value_type> declared_type
    )
    {
        column_id id = doc_.create_column_id();

        document::column_node col;
        col.col.id        = id;
        col.col.name      = name;
        col.table         = table;
        col.creation      = creation_state::generated;
        col.is_edited     = true;
        col.col.semantic      = semantic_state::valid;
        //col.contamination = contamination_state::clean;

        col.col.type = declared_type.has_value()
             ? *declared_type
             : value_type::unresolved;

        doc_.columns_.push_back(std::move(col));

        return id;
    }

    inline table_row_id editor::create_row_node_only(
        table_id table,
        std::vector<typed_value> cells
    )
    {
        table_row_id id = doc_.create_row_id();

        document::row_node row;
        row.id    = id;
        row.table = table;
        row.cells = std::move(cells);

        row.creation  = creation_state::generated;
        row.is_edited = true;

        // Initial semantic state:
        bool has_invalid = false;
        for (auto const& cell : row.cells)
        {
            if (cell.semantic == semantic_state::invalid ||
                cell.contamination == contamination_state::contaminated)
            {
                has_invalid = true;
                break;
            }
        }

        row.semantic =
            has_invalid
                ? semantic_state::invalid
                : semantic_state::valid;

        row.contamination =
            has_invalid
                ? contamination_state::contaminated
                : contamination_state::clean;

        doc_.rows_.push_back(std::move(row));

        return id;
    }

//============================================================
// Array element manipulation
//============================================================

    inline void editor::set_array_element(key_id key, size_t index, value val)
    {
        auto* kn = doc_.get_node(key);
        if (!kn) return;
        
        auto& tv = kn->value;
        
        if (!is_array(tv))
            return;
        
        auto& arr = std::get<std::vector<typed_value>>(tv.val);
        
        if (index >= arr.size())
            return; // Or throw?
        
        // Create replacement element
        typed_value elem = make_array_element(
            std::move(val),
            tv.type,
            value_locus::key_value
        );
        
        // Replace
        arr[index] = std::move(elem);
        
        // Re-evaluate contamination
        bool has_invalid = false;
        for (auto& e : arr) 
        {
            if (e.semantic == semantic_state::invalid) {
                has_invalid = true;
                break;
            }
        }
        
        if (has_invalid) 
        {
            tv.contamination = contamination_state::contaminated;
            kn->contamination = contamination_state::contaminated;
            doc_.mark_key_contaminated(key);
        } 
        else 
        {
            tv.contamination = contamination_state::clean;
            doc_.request_clear_contamination(key);
        }
        
        kn->is_edited = true;
    }

    inline void editor::append_array_element(key_id key, value val)
    {
        auto* kn = doc_.get_node(key);
        if (!kn) return;
        
        auto& tv = kn->value;
        
        // Must be an array
        if (!is_array(tv))
            return; // Or throw? Design decision
        
        // Get array reference
        auto& arr = std::get<std::vector<typed_value>>(tv.val);
        
        // Create new element
        typed_value elem = make_array_element(
            std::move(val),
            tv.type,
            value_locus::key_value
        );
        
        // Append
        arr.push_back(std::move(elem));
        
        // Check if new element is invalid
        if (arr.back().semantic == semantic_state::invalid) {
            tv.contamination = contamination_state::contaminated;
            kn->contamination = contamination_state::contaminated;
            doc_.mark_key_contaminated(key);
        }
        
        kn->is_edited = true;
    }

    inline void editor::set_array_elements(key_id key, std::vector<value> vals)
    {
        auto* kn = doc_.get_node(key);
        if (!kn) return;
        
        auto& tv = kn->value;
        
        if (!is_array(tv))
            return;
        
        // Build new array
        std::vector<typed_value> new_arr;
        new_arr.reserve(vals.size());
        
        bool has_invalid = false;
        for (auto& v : vals) {
            auto elem = make_array_element(
                std::move(v),
                tv.type,
                value_locus::key_value
            );
            
            if (elem.semantic == semantic_state::invalid)
                has_invalid = true;
            
            new_arr.push_back(std::move(elem));
        }
        
        // Replace entire array
        tv.val = std::move(new_arr);
        
        // Update contamination
        if (has_invalid) 
        {
            tv.contamination = contamination_state::contaminated;
            kn->contamination = contamination_state::contaminated;
            doc_.mark_key_contaminated(key);
        } 
        else 
        {
            tv.contamination = contamination_state::clean;
            doc_.request_clear_contamination(key);
        }
        
        kn->is_edited = true;
    }    

    inline void editor::erase_array_element(key_id key, size_t index)
    {
        auto* kn = doc_.get_node(key);
        if (!kn) return;
        
        auto& tv = kn->value;
        
        if (!is_array(tv))
            return;
        
        auto& arr = std::get<std::vector<typed_value>>(tv.val);
        
        if (index >= arr.size())
            return;
        
        // Remove element
        arr.erase(arr.begin() + index);
        
        // Re-evaluate contamination (might clear if removed element was the problem)
        bool has_invalid = false;
        for (auto& e : arr) 
        {
            if (e.semantic == semantic_state::invalid) 
            {
                has_invalid = true;
                break;
            }
        }
        
        if (has_invalid) 
        {
            tv.contamination = contamination_state::contaminated;
            kn->contamination = contamination_state::contaminated;
            doc_.mark_key_contaminated(key);
        } 
        else 
        {
            tv.contamination = contamination_state::clean;
            doc_.request_clear_contamination(key);
        }
        
        kn->is_edited = true;
    }    

//============================================================
// Key / value manipulation
//============================================================

    key_id editor::append_key(
        category_id where,
        std::string_view name,
        value v,
        bool untyped)
    {
        key_id id = create_key_node_only(where, name, std::move(v), untyped);
        if (!valid(id)) return id;

        auto* cat = doc_.get_node(where);
        cat->ordered_items.push_back(document::source_item_ref{id});
        return id;
    }

    template<typename K>
    key_id editor::insert_key_before(
        id<K> anchor,
        std::string_view name,
        value v,
        bool untyped)
    {
        auto* ref = locate_anchor(anchor);
        if (!ref)
            return invalid_id<key_tag>();

        auto* anchor_node = doc_.get_node(anchor);
        if (!anchor_node)
            return invalid_id<key_tag>();

        category_id where = anchor_node->owner;

        key_id id = create_key_node_only(where, name, std::move(v), untyped);
        if (!valid(id)) return id;

        auto* cat = doc_.get_node(where);

        auto it = std::ranges::find_if(
            cat->ordered_items,
            [&](auto const& r)
            {
                return r.id == ref->id;
            });

        cat->ordered_items.insert(it, {id});
        return id;        
    }

    template<typename K>
    key_id editor::insert_key_after(
        id<K> anchor,
        std::string_view name,
        value v,
        bool untyped)
    {
        auto* ref = locate_anchor(anchor);
        if (!ref)
            return invalid_id<key_tag>();

        auto* anchor_node = doc_.get_node(anchor);
        if (!anchor_node)
            return invalid_id<key_tag>();

        category_id where = anchor_node->owner;

        key_id id = create_key_node_only(where, name, std::move(v), untyped);
        if (!valid(id)) return id;

        auto* cat = doc_.get_node(where);

        auto it = std::ranges::find_if(
            cat->ordered_items,
            [&](auto const& r)
            {
                return r.id == ref->id;
            });

        if (it != cat->ordered_items.end()) ++it;                    
        cat->ordered_items.insert(it, {id});
        return id;        
    }

    inline key_id editor::append_key(
        category_id where,
        std::string_view name,
        std::vector<value> arr,
        bool untyped)
    {
        auto* cat = doc_.get_node(where);
        if (!cat)
            return invalid_id<key_tag>();

        key_id id = doc_.create_key_id();

        document::key_node kn;
        kn.id    = id;
        kn.name  = std::string(name);
        kn.owner = where;

        // Infer array type from first element if untyped
        value_type array_type = value_type::unresolved;
        value_type elem_type = value_type::unresolved;
        
        if (!untyped && !arr.empty())
        {
            elem_type = held_type(arr[0]);
            
            // Map element type to array type
            switch (elem_type)
            {
                case value_type::string:  array_type = value_type::string_array; break;
                case value_type::integer: array_type = value_type::int_array;    break;
                case value_type::decimal: array_type = value_type::float_array;  break;
                default:                  array_type = value_type::unresolved;   break;
            }
        }
        
        kn.type        = array_type;
        kn.type_source = untyped ? type_ascription::tacit
                                 : type_ascription::declared;

        // Build typed array
        std::vector<typed_value> typed_arr;
        typed_arr.reserve(arr.size());
        
        bool has_invalid = false;
        
        for (auto& v : arr)
        {
            typed_value elem = make_array_element(
                std::move(v),
                array_type,
                value_locus::array_element
            );
            
            if (elem.semantic == semantic_state::invalid)
                has_invalid = true;
            
            typed_arr.push_back(std::move(elem));
        }

        // Set up key value
        kn.value.val           = std::move(typed_arr);
        kn.value.type          = array_type;
        kn.value.type_source   = kn.type_source;
        kn.value.origin        = value_locus::key_value;
        kn.value.semantic      = semantic_state::valid;
        kn.value.creation      = creation_state::generated;
        kn.value.contamination = has_invalid 
            ? contamination_state::contaminated 
            : contamination_state::clean;
        
        // Mark key contamination if array has invalid elements
        if (has_invalid)
        {
            kn.contamination = contamination_state::contaminated;
            doc_.mark_key_contaminated(id);
        }

        doc_.keys_.push_back(std::move(kn));
        cat->keys.push_back(id);
        cat->ordered_items.push_back(document::source_item_ref{id});

        return id;
    }

    template<typename K>
    inline key_id editor::insert_key_before(
        id<K> anchor,
        std::string_view name,
        std::vector<value> arr,
        bool untyped)
    {
        auto* ref = locate_anchor(anchor);
        if (!ref)
            return invalid_id<key_tag>();

        auto where = std::get<category_id>(
            doc_.get_node(anchor)->owner);

        key_id id = append_key(where, name, std::move(arr), untyped);

        auto* cat = doc_.get_node(where);
        auto it = std::ranges::find(cat->ordered_items, *ref);
        cat->ordered_items.insert(it, {id});

        return id;
    }

    template<typename K>
    inline key_id editor::insert_key_after(
        id<K> anchor,
        std::string_view name,
        std::vector<value> arr,
        bool untyped)
    {
        auto* ref = locate_anchor(anchor);
        if (!ref)
            return invalid_id<key_tag>();

        auto where = std::get<category_id>(
            doc_.get_node(anchor)->owner);

        key_id id = append_key(where, name, std::move(arr), untyped);

        auto* cat = doc_.get_node(where);
        auto it = std::ranges::find(cat->ordered_items, *ref);

        if (it != cat->ordered_items.end()) ++it;
        cat->ordered_items.insert(it, {id});

        return id;
    }

    bool editor::erase_key(key_id id)
    {
        auto* kn = doc_.get_node(id);
        if (!kn) return false;

        auto* cat = doc_.get_node(kn->owner);
        if (!cat) return false;

        // Remove contamination source if present
        doc_.request_clear_contamination(id);

        // ordered_items
        std::erase_if(cat->ordered_items, [&](auto const& r)
        {
            return std::holds_alternative<key_id>(r.id)
                && std::get<key_id>(r.id) == id;
        });

        // category key list
        std::erase_if(cat->keys, [&](auto const& kid) {return kid == id;});

        // key storage
        auto& keys = doc_.keys_;
        keys.erase(
            std::ranges::find_if(keys, [&](auto const& k){return k.id == id;})
        );

        return true;
    }


    inline void editor::set_key_value(key_id key, value val)
    {
        auto* kn = doc_.get_node(key);
        if (!kn) return;
        
        auto& tv = kn->value;
        
        // Replace payload
        tv.val = std::move(val);
        tv.origin = value_locus::key_value;
        tv.is_edited = true;
        
        // Re-evaluate semantic validity
        if (tv.type != value_type::unresolved)
        {
            if (tv.held_type() != tv.type)
            {
                // Type mismatch → contaminate
                tv.semantic = semantic_state::invalid;
                kn->semantic = semantic_state::invalid;
                doc_.mark_key_contaminated(key);
            }
            else
            {
                // Valid → try to clear
                if (doc_.request_clear_contamination(key))
                    tv.semantic = semantic_state::valid;
            }
        }
        else
        {
            if (doc_.request_clear_contamination(key))
                tv.semantic = semantic_state::valid;
        }
        
        kn->is_edited = true;
    }

    inline void editor::set_key_value(key_id key, std::vector<value> arr)
    {
        auto* kn = doc_.get_node(key);
        if (!kn) return;

        auto& tv = kn->value;

        // Determine array type from key's declared type or infer from first element
        value_type array_type = kn->type;
        
        // If key is untyped, infer from first element
        if (array_type == value_type::unresolved && !arr.empty())
        {
            value_type elem_type = held_type(arr[0]);
            
            switch (elem_type)
            {
                case value_type::string:  array_type = value_type::string_array; break;
                case value_type::integer: array_type = value_type::int_array;    break;
                case value_type::decimal: array_type = value_type::float_array;  break;
                default:                  array_type = value_type::unresolved;   break;
            }
        }

        // Validate: if key has declared type, it must be array type
        bool structural_invalid = false;
        if (kn->type != value_type::unresolved && 
            !is_array_type(kn->type))
        {
            structural_invalid = true;
        }

        // Build typed array
        std::vector<typed_value> typed_arr;
        typed_arr.reserve(arr.size());

        bool has_invalid_element = false;

        for (auto& v : arr)
        {
            typed_value elem = make_array_element(
                std::move(v),
                array_type,
                value_locus::key_value
            );

            if (elem.semantic == semantic_state::invalid)
                has_invalid_element = true;

            typed_arr.push_back(std::move(elem));
        }

        // Replace value
        tv.val           = std::move(typed_arr);
        tv.type          = array_type;
        tv.type_source   = kn->type_source;
        tv.origin        = value_locus::key_value;
        tv.creation      = creation_state::generated;
        tv.is_edited     = true;
        tv.semantic      = semantic_state::valid; // array container itself valid

        bool key_invalid = structural_invalid || has_invalid_element;

        if (key_invalid)
        {
            kn->semantic      = semantic_state::invalid;
            kn->contamination = contamination_state::contaminated;
            tv.contamination  = contamination_state::contaminated;

            doc_.mark_key_contaminated(key);
        }
        else
        {
            kn->semantic      = semantic_state::valid;
            kn->contamination = contamination_state::clean;
            tv.contamination  = contamination_state::clean;

            doc_.request_clear_contamination(key);
        }

        kn->is_edited = true;
    }    

//-------------------------------
// Table cell array specialisation implementations
//-------------------------------

    void editor::append_array_element(
        table_row_id row,
        column_id col,
        value val)
    {
        auto* rn = doc_.get_node(row);
        if (!rn) return;
        
        auto* tbl = doc_.get_node(rn->table);
        if (!tbl) return;
        
        // Find column to determine expected type
        auto* cn = doc_.get_node(col);
        if (!cn) return;
        
        auto col_it = std::ranges::find(tbl->columns, col);
        if (col_it == tbl->columns.end()) return;
        
        size_t col_idx = std::distance(tbl->columns.begin(), col_it);
        if (col_idx >= rn->cells.size()) return;
        
        auto& cell = rn->cells[col_idx];
        
        if (!is_array(cell)) return;
        
        auto& arr = std::get<std::vector<typed_value>>(cell.val);
        
        // Create element with validation
        typed_value elem = make_array_element(
            std::move(val),
            cell.type,
            value_locus::table_cell
        );
        
        arr.push_back(std::move(elem));
        
        // Re-evaluate contamination
        update_array_and_check(
            cell,
            cell.type,
            [this, row]() { doc_.mark_row_contaminated(row); },
            [this, row]() { doc_.request_clear_contamination(row); }
        );
        
        cell.is_edited = true;
        rn->is_edited = true;
    }

    void editor::set_array_element(
        table_row_id row,
        column_id col,
        size_t index,
        value val)
    {
        auto* rn = doc_.get_node(row);
        if (!rn) return;
        
        auto* tbl = doc_.get_node(rn->table);
        if (!tbl) return;
        
        auto col_it = std::ranges::find(tbl->columns, col);
        if (col_it == tbl->columns.end()) return;
        
        size_t col_idx = std::distance(tbl->columns.begin(), col_it);
        if (col_idx >= rn->cells.size()) return;
        
        auto& cell = rn->cells[col_idx];
        if (!is_array(cell)) return;
        
        auto& arr = std::get<std::vector<typed_value>>(cell.val);
        if (index >= arr.size()) return;
        
        // Create replacement element
        typed_value elem = make_array_element(
            std::move(val),
            cell.type,
            value_locus::table_cell
        );
        
        arr[index] = std::move(elem);
        
        // Re-evaluate contamination
        update_array_and_check(
            cell,
            cell.type,
            [this, row]() { doc_.mark_row_contaminated(row); },
            [this, row]() { doc_.request_clear_contamination(row); }
        );
        
        cell.is_edited = true;
        rn->is_edited = true;
    }

    void editor::set_array_elements(
        table_row_id row,
        column_id col,
        std::vector<value> vals)
    {
        auto* rn = doc_.get_node(row);
        if (!rn) return;
        
        auto* tbl = doc_.get_node(rn->table);
        if (!tbl) return;
        
        auto col_it = std::ranges::find(tbl->columns, col);
        if (col_it == tbl->columns.end()) return;
        
        size_t col_idx = std::distance(tbl->columns.begin(), col_it);
        if (col_idx >= rn->cells.size()) return;
        
        auto& cell = rn->cells[col_idx];
        if (!is_array(cell)) return;
        
        // Build new array
        std::vector<typed_value> new_arr;
        new_arr.reserve(vals.size());
        
        for (auto& v : vals)
        {
            typed_value elem = make_array_element(
                std::move(v),
                cell.type,
                value_locus::table_cell
            );
            new_arr.push_back(std::move(elem));
        }
        
        // Replace entire array
        cell.val = std::move(new_arr);
        
        // Re-evaluate contamination
        update_array_and_check(
            cell,
            cell.type,
            [this, row]() { doc_.mark_row_contaminated(row); },
            [this, row]() { doc_.request_clear_contamination(row); }
        );
        
        cell.is_edited = true;
        rn->is_edited = true;
    }

    void editor::erase_array_element(
        table_row_id row,
        column_id col,
        size_t index)
    {
        auto* rn = doc_.get_node(row);
        if (!rn) return;
        
        auto* tbl = doc_.get_node(rn->table);
        if (!tbl) return;
        
        auto col_it = std::ranges::find(tbl->columns, col);
        if (col_it == tbl->columns.end()) return;
        
        size_t col_idx = std::distance(tbl->columns.begin(), col_it);
        if (col_idx >= rn->cells.size()) return;
        
        auto& cell = rn->cells[col_idx];
        if (!is_array(cell)) return;
        
        auto& arr = std::get<std::vector<typed_value>>(cell.val);
        if (index >= arr.size()) return;
        
        arr.erase(arr.begin() + index);
        
        // Re-evaluate contamination (might clear if removed element was invalid)
        update_array_and_check(
            cell,
            cell.type,
            [this, row]() { doc_.mark_row_contaminated(row); },
            [this, row]() { doc_.request_clear_contamination(row); }
        );
        
        cell.is_edited = true;
        rn->is_edited = true;
    }

//============================================================
// Tables
//============================================================

    table_row_id editor::append_row(
        table_id table,
        std::vector<value> cells)
    {
        auto* tbl = doc_.get_node(table);
        if (!tbl) 
            return invalid_id<table_row_tag>();

        table_row_id id = doc_.create_row_id();

        document::row_node rn;
        rn.id    = id;
        rn.table = table;
        rn.owner = tbl->owner;

        rn.cells.reserve(tbl->columns.size());

        bool row_has_invalid = false;

        for (size_t i = 0; i < tbl->columns.size(); ++i)
        {
            auto* col = doc_.get_node(tbl->columns[i]);
            if (!col)
                return invalid_id<table_row_tag>(); // structural corruption

            typed_value tv;

            if (i < cells.size())
                tv.val = std::move(cells[i]);
            else
                tv.val = value{}; // default/empty

            tv.origin   = value_locus::table_cell;
            tv.creation = creation_state::generated;
            tv.is_edited = false;

            value_type declared_type = col->_type();

            tv.type = tv.held_type();

            if (declared_type != value_type::unresolved)
            {
                if (tv.type != declared_type)
                {
                    tv.semantic = semantic_state::invalid;
                    tv.type_source = type_ascription::tacit;
                    row_has_invalid = true;
                }
                else
                {
                    tv.semantic = semantic_state::valid;
                    tv.type_source = type_ascription::declared;
                }
            }
            else
            {
                tv.semantic = semantic_state::valid;
                tv.type_source = type_ascription::tacit;
            }

            tv.contamination = contamination_state::clean;

            rn.cells.push_back(std::move(tv));
        }

        if (row_has_invalid)
        {
            rn.contamination = contamination_state::contaminated;
            doc_.mark_row_contaminated(id);
        }
        else
        {
            rn.contamination = contamination_state::clean;
        }

        doc_.rows_.push_back(std::move(rn));
        tbl->rows.push_back(id);
        tbl->ordered_items.push_back({id});

        return id;
    }

    template<typename Tag>
    table_row_id editor::insert_row_impl(
        id<Tag> anchor,
        std::vector<value> cells,
        insert_direction dir)
    {
        if constexpr (!std::is_same_v<Tag, table_row_tag>)
            return invalid_id<table_row_tag>();

        auto* anchor_node = doc_.get_node(anchor);
        if (!anchor_node) return invalid_id<table_row_tag>();

        table_id table = anchor_node->table;
        auto* tbl = doc_.get_node(table);
        if (!tbl) return invalid_id<table_row_tag>();

        table_row_id new_id = append_row(table, std::move(cells));
        if (!valid(new_id)) return new_id;

        // Remove auto-appended entry
        std::erase(tbl->rows, new_id);
        std::erase_if(tbl->ordered_items, [&](auto const& r) {
            return std::holds_alternative<table_row_id>(r.id)
                && std::get<table_row_id>(r.id) == new_id;
        });

        // Find anchor position
        auto it = std::ranges::find_if(tbl->ordered_items, [&](auto const& r) {
            return std::holds_alternative<table_row_id>(r.id)
                && std::get<table_row_id>(r.id) == anchor;
        });

        auto row_it = std::ranges::find(tbl->rows, anchor);

        // Insert based on direction
        if (dir == insert_direction::after)
        {
            if (row_it != tbl->rows.end()) ++row_it;
            if (it != tbl->ordered_items.end()) ++it;
        }

        tbl->rows.insert(row_it, new_id);
        tbl->ordered_items.insert(it, {new_id});

        return new_id;
    }

    template<typename Tag>
    table_row_id editor::insert_row_before(
        id<Tag> anchor,
        std::vector<value> cells)
    {
        return insert_row_impl(anchor, std::move(cells), insert_direction::before);
    }

    template<typename Tag>
    table_row_id editor::insert_row_after(
        id<Tag> anchor,
        std::vector<value> cells)
    {
        return insert_row_impl(anchor, std::move(cells), insert_direction::after);
    }
    
    bool editor::erase_column(column_id id)
    {
        auto* cn = doc_.get_node(id);
        if (!cn) return false;
        
        auto* tbl = doc_.get_node(cn->table);
        if (!tbl) return false;
        
        // Find column index
        auto col_it = std::ranges::find(tbl->columns, id);
        if (col_it == tbl->columns.end()) return false;
        
        size_t col_idx = std::distance(tbl->columns.begin(), col_it);
        
        // Remove cells from all rows at this index
        for (auto rid : tbl->rows)
        {
            auto* rn = doc_.get_node(rid);
            if (!rn || col_idx >= rn->cells.size()) continue;
            
            rn->cells.erase(rn->cells.begin() + col_idx);
            rn->is_edited = true;
            
            // Re-evaluate row contamination after cell removal
            bool has_invalid = false;
            for (auto& cell : rn->cells)
            {
                if (cell.semantic == semantic_state::invalid ||
                    cell.contamination == contamination_state::contaminated)
                {
                    has_invalid = true;
                    break;
                }
            }
            
            if (has_invalid)
            {
                rn->contamination = contamination_state::contaminated;
                doc_.mark_row_contaminated(rid);
            }
            else
            {
                rn->contamination = contamination_state::clean;
                doc_.request_clear_contamination(rid);
            }
        }
        
        // Remove column from table
        tbl->columns.erase(col_it);
        
        // Remove column node
        std::erase_if(doc_.columns_, [&](auto& c) { return c._id() == id; });
        
        return true;
    }

    inline column_id editor::append_column(
        table_id table_id,
        std::string_view name, 
        std::optional<value_type> declared_type
    )
    {
        auto* tbl = doc_.get_node(table_id);
        if (!tbl) return invalid_id<table_column_tag>();

        column_id cid = create_column_node_only(table_id, name, declared_type);
        tbl->columns.push_back(cid);

        for (auto rid : tbl->rows) 
        {
            auto* rn = doc_.get_node(rid);
            if (!rn) continue;
            
            typed_value empty_cell;
            empty_cell.val = value{};  // monostate
            empty_cell.type = declared_type.value_or(value_type::unresolved);
            empty_cell.origin = value_locus::table_cell;
            empty_cell.creation = creation_state::generated;
            empty_cell.is_edited = true;
            empty_cell.semantic = semantic_state::valid;
            empty_cell.contamination = contamination_state::contaminated; 
            
            rn->cells.push_back(std::move(empty_cell));

            // A monostate cell is invalid.
            doc_.mark_row_contaminated(rid);
        }

        return cid;
    }

    column_id editor::insert_column_before( 
        column_id anchor, 
        std::string_view name, 
        std::optional<value_type> declared_type )
    {
        auto* anchor_node = doc_.get_node(anchor);
        if (!anchor_node) return invalid_id<table_column_tag>();

        table_id owner = anchor_node->table;
        auto* tbl = doc_.get_node(owner);
        if (!tbl) return invalid_id<table_column_tag>();

        column_id cid = create_column_node_only(owner, name, declared_type);

        auto it = std::ranges::find(tbl->columns, anchor);
        auto ins_it = tbl->columns.insert(it, cid);
        auto dist = std::distance(tbl->columns.begin(), ins_it);

        for (auto rid : tbl->rows) 
        {
            auto* rn = doc_.get_node(rid);
            if (!rn) continue;
            
            typed_value empty_cell;
            empty_cell.val = value{};  // monostate
            empty_cell.type = declared_type.value_or(value_type::unresolved);
            empty_cell.origin = value_locus::table_cell;
            empty_cell.creation = creation_state::generated;
            empty_cell.is_edited = true;
            empty_cell.semantic = semantic_state::valid;
            empty_cell.contamination = contamination_state::contaminated; 
        
            auto pos = rn->cells.begin();
            std::advance( pos, dist );
            rn->cells.insert( pos, std::move(empty_cell) );
            
            // A monostate cell is invalid.
            doc_.mark_row_contaminated(rid);
        }

        return cid;
    }

    column_id editor::insert_column_after( 
        column_id anchor, 
        std::string_view name, 
        std::optional<value_type> declared_type )
    {
        auto* anchor_node = doc_.get_node(anchor);
        if (!anchor_node) return invalid_id<table_column_tag>();

        table_id owner = anchor_node->table;
        auto* tbl = doc_.get_node(owner);
        if (!tbl) return invalid_id<table_column_tag>();

        column_id cid = create_column_node_only(owner, name, declared_type);

        auto it = std::ranges::find(tbl->columns, anchor);
        if (it != tbl->columns.end()) ++it;
        auto ins_it = tbl->columns.insert(it, cid);
        auto dist = std::distance(tbl->columns.begin(), ins_it);

        for (auto rid : tbl->rows) 
        {
            auto* rn = doc_.get_node(rid);
            if (!rn) continue;
            
            typed_value empty_cell;
            empty_cell.val = value{};  // monostate
            empty_cell.type = declared_type.value_or(value_type::unresolved);
            empty_cell.origin = value_locus::table_cell;
            empty_cell.creation = creation_state::generated;
            empty_cell.is_edited = true;
            empty_cell.semantic = semantic_state::valid;
            empty_cell.contamination = contamination_state::contaminated; 
        
            auto pos = rn->cells.begin();
            std::advance( pos, dist );
            rn->cells.insert( pos, std::move(empty_cell) );
            
            // A monostate cell is invalid.
            doc_.mark_row_contaminated(rid);
        }

        return cid;
    }

    void editor::set_cell_value(
        table_row_id row,
        column_id col,
        value val)
    {
        auto* rn = doc_.get_node(row);
        if (!rn) return;

        auto* tbl = doc_.get_node(rn->table);
        if (!tbl) return;

        auto col_it = std::ranges::find(tbl->columns, col);
        if (col_it == tbl->columns.end()) return;

        size_t idx = std::distance(tbl->columns.begin(), col_it);
        if (idx >= rn->cells.size()) return;

        auto* cn = doc_.get_node(col);
        if (!cn) return;

        auto& cell = rn->cells[idx];

        cell.val       = std::move(val);
        cell.origin    = value_locus::table_cell;
        cell.creation  = creation_state::generated;
        cell.is_edited = true;

        value_type expected = cn->_type();
        value_type actual   = cell.held_type();

        if (expected != value_type::unresolved && actual != expected)
        {
            cell.type       = expected;
            cell.semantic   = semantic_state::invalid;
            cell.contamination = contamination_state::clean;

            doc_.mark_row_contaminated(row);
        }
        else
        {
            cell.type       = actual;
            cell.semantic   = semantic_state::valid;
            cell.contamination = contamination_state::clean;

            doc_.request_clear_contamination(row);
        }

        rn->is_edited = true;
    }

    void editor::set_cell_value(
        table_row_id row,
        column_id col,
        std::vector<value> arr)
    {
        auto* rn = doc_.get_node(row);
        if (!rn) return;

        auto* tbl = doc_.get_node(rn->table);
        if (!tbl) return;

        auto col_it = std::ranges::find(tbl->columns, col);
        if (col_it == tbl->columns.end()) return;

        size_t idx = std::distance(tbl->columns.begin(), col_it);
        if (idx >= rn->cells.size()) return;

        auto* cn = doc_.get_node(col);
        if (!cn) return;

        auto& cell = rn->cells[idx];

        value_type expected_array_type = cn->_type();

        // If column is not declared as array type → semantic violation
        if (!is_array_type(expected_array_type) &&
            expected_array_type != value_type::unresolved)
        {
            cell.semantic = semantic_state::invalid;
            doc_.mark_row_contaminated(row);
            rn->is_edited = true;
            return;
        }

        std::vector<typed_value> typed_arr;
        typed_arr.reserve(arr.size());

        bool has_invalid = false;

        for (auto& v : arr)
        {
            typed_value elem = make_array_element(
                std::move(v),
                expected_array_type,
                value_locus::table_cell
            );

            if (elem.semantic == semantic_state::invalid)
                has_invalid = true;

            typed_arr.push_back(std::move(elem));
        }

        cell.val         = std::move(typed_arr);
        cell.type        = expected_array_type;
        cell.origin      = value_locus::table_cell;
        cell.creation    = creation_state::generated;
        cell.is_edited   = true;
        cell.semantic    = semantic_state::valid;
        cell.contamination =
            has_invalid
                ? contamination_state::contaminated
                : contamination_state::clean;

        if (has_invalid)
            doc_.mark_row_contaminated(row);
        else
            doc_.request_clear_contamination(row);

        rn->is_edited = true;
    }

    bool editor::erase_row(table_row_id id)
    {
        auto* rn = doc_.get_node(id);
        if (!rn) return false;

        auto* tbl = doc_.get_node(rn->table);
        if (!tbl) return false;

        doc_.request_clear_contamination(id);

        std::erase_if(tbl->ordered_items, [&](auto const& r)
        {
            return std::holds_alternative<table_row_id>(r.id)
                && std::get<table_row_id>(r.id) == id;
        });

        std::erase_if(tbl->rows, [&](auto const& rid) {return rid == id;});

        auto& rows = doc_.rows_;
        rows.erase(
            std::ranges::find_if(rows, [&](auto const& r){return r.id == id;})
        );

        return true;
    }

    inline bool editor::erase_table(table_id id)
    {
        auto* tbl = doc_.get_node(id);
        if (!tbl) return false;

        auto* cat = doc_.get_node(tbl->owner);
        if (!cat) return false;

        // 1. Erase rows (they may be contamination sources)
        for (auto rid : tbl->rows)
        {
            doc_.request_clear_contamination(rid);

            std::erase_if(cat->ordered_items, [&](auto const& r)
            {
                return std::holds_alternative<table_row_id>(r.id)
                    && std::get<table_row_id>(r.id) == rid;
            });

            std::erase_if(doc_.rows_, [&](auto & r){return r.id == rid;});
        }

        // 2. Erase columns
        for (auto cid : tbl->columns)
            std::erase_if(doc_.columns_, [&](auto & c){return c._id() == cid;});

        // 3. Remove table from category
        std::erase(cat->tables, id);

        std::erase_if(cat->ordered_items, [&](auto const& r)
        {
            return std::holds_alternative<table_id>(r.id)
                && std::get<table_id>(r.id) == id;
        });

        // 4. Remove table storage
        std::erase_if(doc_.tables_, [&](auto & t){return t.id == id;});

        // 5. Remove contamination from owning category
        doc_.try_clear_category_contamination(cat->id);

        return true;
    }

    inline table_id editor::append_table(
        category_id where,
        std::vector<std::string> column_names)
    {
        std::vector<std::pair<std::string, std::optional<value_type>>> cols;

        for (auto& n : column_names)
            cols.emplace_back(n, std::nullopt);

        return append_table(where, std::move(cols));
    }

    inline table_id editor::append_table(
        category_id where,
        std::vector<std::pair<std::string, std::optional<value_type>>> columns)
    {
        auto* cat = doc_.get_node(where);
        if (!cat) return invalid_id<table_tag>();

        table_id tid = doc_.create_table_id();

        document::table_node tbl;
        tbl.id    = tid;
        tbl.owner = where;

        // Create columns
        for (auto& [name, opt_type] : columns)
        {
            column_id cid = doc_.create_column_id();

            document::column_node cn;
            cn.col.id   = cid;
            cn.col.name = std::move(name);
            cn.col.type = opt_type.value_or(value_type::unresolved);
            cn.col.semantic = semantic_state::valid;

            cn.table = tid;
            cn.owner = where;

            doc_.columns_.push_back(std::move(cn));
            tbl.columns.push_back(cid);
        }

        doc_.tables_.push_back(std::move(tbl));
        cat->tables.push_back(tid);
        cat->ordered_items.push_back({tid});

        return tid;
    }
/*
    template<typename Tag> 
    table_id editor::insert_table_before( id<Tag> anchor, std::vector<std::string> column_names ) 
    {
        std::vector<std::pair<std::string, std::optional<value_type>>> cols;

        for (auto& n : column_names)
            cols.emplace_back(n, std::nullopt);

        return insert_table_before(anchor, std::move(cols));
    }

    template<typename Tag>
    table_id editor::insert_table_before(
        id<Tag> anchor,
        std::vector<std::pair<std::string, std::optional<value_type>>> columns)
    {
        auto* ref = locate_anchor(anchor);
        if (!ref) return invalid_id<table_tag>();

        auto* anchor_node = doc_.get_node(anchor);
        if (!anchor_node) return invalid_id<table_tag>();

        category_id where = anchor_node->owner;

        table_id tid = append_table(where, std::move(columns));

        auto* cat = doc_.get_node(where);
        auto it = std::ranges::find(cat->ordered_items, *ref);

        if (it != cat->ordered_items.end())
        {
            std::erase_if(cat->ordered_items, [&](const document::source_item_ref& ref){return ref == document::source_item_ref{tid};});
            cat->ordered_items.insert(it, {tid});
        }

        return tid;
    }
*/
    template<typename Tag>
    comment_id insert_table_before(id<Tag> anchor, std::vector<std::pair<std::string, std::optional<value_type>>> columns)
    {
        return insert_category_item_before(anchor, &editor::append_table, columns);
    }

    template<typename Tag>
    comment_id insert_table_after(id<Tag> anchor, std::vector<std::pair<std::string, std::optional<value_type>>> columns)
    {
        return insert_category_item_after(anchor, &editor::append_table, columns);
    }

    template<typename Tag>
    comment_id insert_table_before(id<Tag> anchor, std::vector<std::string> column_names)
    {
        return insert_category_item_before(anchor, &editor::append_table, column_names);
    }

    template<typename Tag>
    comment_id insert_table_after(id<Tag> anchor, std::vector<std::string> column_names)
    {
        return insert_category_item_after(anchor, &editor::append_table, column_names);
    }

/*    
    template<typename Tag> 
    table_id editor::insert_table_after( id<Tag> anchor, std::vector<std::string> column_names ) 
    {
        std::vector<std::pair<std::string, std::optional<value_type>>> cols;

        for (auto& n : column_names)
            cols.emplace_back(n, std::nullopt);

        return insert_table_after(anchor, std::move(cols));
    }

    template<typename Tag>
    table_id editor::insert_table_after(
        id<Tag> anchor,
        std::vector<std::pair<std::string, std::optional<value_type>>> columns)
    {
        auto* ref = locate_anchor(anchor);
        if (!ref) return invalid_id<table_tag>();

        auto* anchor_node = doc_.get_node(anchor);
        if (!anchor_node) return invalid_id<table_tag>();

        category_id where = anchor_node->owner;

        table_id tid = append_table(where, std::move(columns));

        auto* cat = doc_.get_node(where);
        auto it = std::ranges::find(cat->ordered_items, *ref);

        if (it != cat->ordered_items.end())
        {
            ++it;
            std::erase(cat->ordered_items, document::source_item_ref{tid});
            cat->ordered_items.insert(it, {tid});
        }

        return tid;
    }
*/
//============================================================
// Comments
//============================================================

    inline comment_id editor::append_comment(
        category_id where,
        std::string_view text)
    {
        auto* cat = doc_.get_node(where);
        if (!cat) return invalid_id<comment_tag>();

        comment_id id = doc_.create_comment_id();

        document::comment_node cn;
        cn.id       = id;
        cn.text     = std::string(text);
        cn.owner    = where;
        cn.creation = creation_state::generated;

        doc_.comments_.push_back(std::move(cn));
        cat->ordered_items.push_back({id});

        return id;
    }

    inline void editor::set_comment(comment_id id, std::string_view text)
    {
        auto* cn = doc_.get_node(id);
        if (!cn) return;

        cn->text = std::string(text);
        cn->creation = creation_state::generated;
        cn->is_edited = true;
    }

    inline bool editor::erase_comment(comment_id id)
    {
        return erase_category_child(id, doc_.comments_);
    }

    template<typename K>
    comment_id insert_comment_before(id<K> anchor, std::string_view text)
    {
        return insert_category_child_before(anchor, &editor::append_comment, text);
    }

    template<typename K>
    comment_id insert_comment_after(id<K> anchor, std::string_view text)
    {
        return insert_category_child_after(anchor, &editor::append_comment, text);
    }

//============================================================
// Paragraphs (category scope only)
//============================================================

    inline paragraph_id editor::append_paragraph(
        category_id where,
        std::string_view text)
    {
        auto* cat = doc_.get_node(where);
        if (!cat) return invalid_id<paragraph_tag>();

        paragraph_id id = doc_.create_paragraph_id();

        document::paragraph_node pn;
        pn.id       = id;
        pn.text     = std::string(text);
        pn.owner    = where;
        pn.creation = creation_state::generated;

        doc_.paragraphs_.push_back(std::move(pn));
        cat->ordered_items.push_back({id});

        return id;
    }

    inline void editor::set_paragraph(paragraph_id id, std::string_view text)
    {
        auto* pn = doc_.get_node(id);
        if (!pn) return;

        pn->text = std::string(text);
        pn->creation = creation_state::generated;
        pn->is_edited = true;
    }

    inline bool editor::erase_paragraph(paragraph_id id)
    {
        return erase_category_child(id, doc_.paragraphs_);
    }

    template<typename K>
    comment_id insert_paragraph_before(id<K> anchor, std::string_view text)
    {
        return insert_category_child_before(anchor, &editor::append_paragraph, text);
    }

    template<typename K>
    comment_id insert_parageraph_after(id<K> anchor, std::string_view text)
    {
        return insert_category_child_after(anchor, &editor::append_paragraph, text);
    }

//============================================================
// Type control (explicit, opt-in)
//============================================================

    inline bool editor::set_key_type(
        key_id id,
        value_type type,
        type_ascription ascription)
    {
        auto* kn = doc_.get_node(id);
        if (!kn) return false;

        // Store old type for validation
        value_type old_type = kn->type;

        // Update key metadata
        kn->type        = type;
        kn->type_source = ascription;
        kn->is_edited   = true;

        auto& tv = kn->value;
        tv.type        = type;
        tv.type_source = ascription;
        tv.is_edited   = true;

        // Re-validate value against new type
        bool is_valid = true;

        if (type != value_type::unresolved)
        {
            // For arrays, check array type matches
            if (is_array(tv))
            {
                if (!is_array_type(type))
                {
                    // Trying to set non-array type on array value
                    is_valid = false;
                }
                else
                {
                    // Check all elements against new array element type
                    auto& arr = std::get<std::vector<typed_value>>(tv.val);
                    value_type expected_elem_type = detail::array_element_type(type);

                    for (auto& elem : arr)
                    {
                        if (elem.type != expected_elem_type)
                        {
                            elem.semantic = semantic_state::invalid;
                            is_valid = false;
                        }
                        else
                        {
                            elem.semantic = semantic_state::valid;
                        }
                    }
                }
            }
            else
            {
                // Scalar value - check type match
                if (tv.held_type() != type)
                {
                    is_valid = false;
                }
            }
        }

        // Update semantic state and contamination
        if (!is_valid)
        {
            tv.semantic       = semantic_state::invalid;
            tv.contamination  = contamination_state::contaminated;
            kn->semantic      = semantic_state::invalid;
            kn->contamination = contamination_state::contaminated;

            doc_.mark_key_contaminated(id);
        }
        else
        {
            tv.semantic       = semantic_state::valid;
            tv.contamination  = contamination_state::clean;
            kn->semantic      = semantic_state::valid;
            kn->contamination = contamination_state::clean;

            doc_.request_clear_contamination(id);
        }

        return is_valid;
    }

    inline bool editor::set_column_type(
        column_id id,
        value_type type,
        type_ascription ascription)
    {
        auto* cn = doc_.get_node(id);
        if (!cn) return false;

        auto* tbl = doc_.get_node(cn->table);
        if (!tbl) return false;

        auto col_it = std::ranges::find(tbl->columns, id);
        if (col_it == tbl->columns.end()) return false;
        
        size_t col_idx = std::distance(tbl->columns.begin(), col_it);

        // Update column metadata
        cn->col.type        = type;
        cn->col.type_source = ascription;
        cn->is_edited       = true;

        bool any_invalid = false;

        // PHASE 1: Update and validate each cell in this column
        for (auto rid : tbl->rows)
        {
            auto* rn = doc_.get_node(rid);
            if (!rn || col_idx >= rn->cells.size()) continue;

            auto& cell = rn->cells[col_idx];
            
            cell.type        = type;
            cell.type_source = ascription;
            cell.is_edited   = true;

            bool cell_valid = true;

            if (type != value_type::unresolved)
            {
                if (is_array(cell))
                {
                    if (!is_array_type(type))
                    {
                        cell_valid = false;
                    }
                    else
                    {
                        auto& arr = std::get<std::vector<typed_value>>(cell.val);
                        value_type expected_elem_type = detail::array_element_type(type);

                        for (auto& elem : arr)
                        {
                            if (elem.type != expected_elem_type)
                            {
                                elem.semantic = semantic_state::invalid;
                                cell_valid = false;
                            }
                            else
                            {
                                elem.semantic = semantic_state::valid;
                            }
                        }
                    }
                }
                else
                {
                    if (cell.held_type() != type)
                    {
                        cell_valid = false;
                    }
                }
            }

            // Update cell state (but don't touch row contamination yet)
            if (!cell_valid)
            {
                cell.semantic      = semantic_state::invalid;
                cell.contamination = contamination_state::clean;
                any_invalid = true;
            }
            else
            {
                cell.semantic      = semantic_state::valid;
                cell.contamination = contamination_state::clean;
            }

            rn->is_edited = true;
        }

        // PHASE 2: Re-evaluate entire row contamination states
        for (auto rid : tbl->rows)
        {
            auto* rn = doc_.get_node(rid);
            if (!rn) continue;
            
            // Check ALL cells in this row
            bool row_has_invalid = false;
            for (auto& cell : rn->cells)
            {
                if (cell.semantic == semantic_state::invalid ||
                    cell.contamination == contamination_state::contaminated)
                {
                    row_has_invalid = true;
                    break;
                }
            }
            
            if (row_has_invalid)
            {
                rn->contamination = contamination_state::contaminated;
                doc_.mark_row_contaminated(rid);
            }
            else
            {
                rn->contamination = contamination_state::clean;
                doc_.request_clear_contamination(rid);
            }
        }

        return !any_invalid;
    }
    
}

#endif
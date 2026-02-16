// arf_document.hpp - A Readable Format (Arf!) - Authoritative Document Model
// Version 0.3.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_DOCUMENT_HPP
#define ARF_DOCUMENT_HPP

#include "arf_parser.hpp"

#include <cassert>
#include <functional>
#include <iterator>
#include <iostream>
#include <memory>
#include <ranges>
#include <span>
#include <unordered_set>

namespace arf
{
    namespace 
    {
        template<typename T> struct node_to_view;
    }

    class document
    {
        friend struct materialiser;
        friend class serializer;
        friend class editor;   

    //------------------------------------------------------------------------
    // Node base class
    //------------------------------------------------------------------------

        struct empty_struct {};

        struct _source_event 
        { 
            std::optional<size_t> source_event_index; 
        };

        struct _semantic_state 
        {
            semantic_state      semantic      {semantic_state::valid};
            contamination_state contamination {contamination_state::clean};
        };

        // Conditional inheritance pattern:
        // - Nodes can selectively opt-in to source tracking and semantic state
        // - empty_struct is used for opt-out (zero overhead via EBO)
        // - category_node opts out of source_event_index (uses open/close instead)
        // - column_node opts out of semantic_state (uses col.semantic instead)        
        template<bool has_source_event = true, bool has_semantic_state = true>
        struct node 
            : public std::conditional_t<has_source_event, _source_event, empty_struct>
            , public std::conditional_t<has_semantic_state, _semantic_state, empty_struct>
        {
            // Authorship metadata
            creation_state creation  {creation_state::authored};
            bool           is_edited {false};
        };

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
        ~document() = default;  // unique_ptr handles cleanup
        
        // Non-copyable (parse_context is large)
        document(const document&) = delete;
        document& operator=(const document&) = delete;
        
        // Movable
        document(document&&) = default;
        document& operator=(document&&) = default;
        
    //------------------------------------------------------------------------
    // Category access
    //------------------------------------------------------------------------

        size_t category_count() const noexcept;
        std::optional<category_view> root() const noexcept;
        std::optional<category_view> category(std::string_view name) const noexcept;
        std::optional<category_view> category(category_id id) const noexcept;
        std::vector<category_view>   categories() const noexcept;

    //------------------------------------------------------------------------
    // Table access
    //------------------------------------------------------------------------

        size_t table_count() const noexcept;
        std::optional<table_view> table(table_id id) const noexcept;    
        std::vector<table_view>   tables() const noexcept;

    //------------------------------------------------------------------------
    // Column access
    //------------------------------------------------------------------------

        size_t column_count() const noexcept;
        std::optional<column_view> column(column_id id) const noexcept;
        std::vector<column_view>   columns() const noexcept;

    //------------------------------------------------------------------------
    // Row access
    //------------------------------------------------------------------------

        size_t row_count() const noexcept;
        std::optional<table_row_view> row(row_id id) const noexcept;
        std::vector<table_row_view>   rows() const noexcept;

    //------------------------------------------------------------------------
    // Key access
    //------------------------------------------------------------------------

        size_t key_count() const noexcept;
        std::optional<key_view> key(std::string_view name) const noexcept;
        std::optional<key_view> key(key_id id) const noexcept;
        std::vector<key_view>   keys() const noexcept;

        size_t comment_count() const noexcept;
        size_t paragraph_count() const noexcept;

        category_id create_root();

    //------------------------------------------------------------------------
    // Contamination management
    //------------------------------------------------------------------------

        void mark_key_contaminated(key_id id);
        void mark_row_contaminated(row_id id);

        // Used by request_clear_contamination
        using clearable_node = std::variant<key_id, row_id>;

        // A client is requesting the contamination of
        // a key or row to be cleared. 
        bool request_clear_contamination(clearable_node node);

        // Used by request_clear_contamination(node).
        // Set this value to control whether contamination is allowed
        // to be cleared. Defaults to always permissive. This is 
        // primarily intended as a feature for tooling.
        std::function<bool(clearable_node)> request_clear_fn 
            = [](clearable_node){return true;};

        bool has_contamination_sources() const
        {
            return !contaminated_source_keys_.empty() || !contaminated_source_rows_.empty();
        }

    //------------------------------------------------------------------------
    // ID creation (monotonic guarantee)
    //------------------------------------------------------------------------

        category_id   create_category_id()   noexcept  { return next_category_id_++; }
        key_id        create_key_id()        noexcept  { return next_key_id_++; }
        comment_id    create_comment_id()    noexcept  { return next_comment_id_++; }
        paragraph_id  create_paragraph_id()  noexcept  { return next_paragraph_id_++; }
        table_id      create_table_id()      noexcept  { return next_table_id_++; }
        row_id        create_row_id()        noexcept  { return next_row_id_++; }
        column_id     create_column_id()     noexcept  { return next_column_id_++; }

        enum class category_close_form
        {
            shorthand,   // "/"
            named        // "/category"
        };

    //------------------------------------------------------------------------
    // Types for internal storage nodes (fully normalised)
    //------------------------------------------------------------------------

        struct category_close_marker;
        struct category_node;
        struct table_node;
        struct column_node;
        struct row_node;
        struct key_node;
        struct comment_node;
        struct paragraph_node;  

    //------------------------------------------------------------------------
    // Types for source order tracking
    //------------------------------------------------------------------------

        // Entities participating in source order tracking 
        //----------------------------------------------------------
        using source_id = std::variant<
            key_id,
            category_id,            // category open
            category_close_marker,  // category close
            table_id,
            row_id,
            comment_id,
            paragraph_id
        >;

        // Used to track the authored order of document entities 
        //----------------------------------------------------------
        struct source_item_ref;

        // Translate tag (ID) to client view type
        //----------------------------------------------------------
        template<typename Tag> struct view_for;

        template<> struct view_for<key_tag>          { typedef key_view         type; };
        template<> struct view_for<table_tag>        { typedef table_view       type; };
        template<> struct view_for<category_tag>     { typedef category_view    type; };
        template<> struct view_for<row_tag>    { typedef table_row_view   type; };
        template<> struct view_for<column_tag> { typedef column_view      type; };
        //template<> struct view_for<comment_tag>      { typedef void type; };//comment_view type; };
        //template<> struct view_for<paragraph_tag>    { typedef void type; };//paragraph_view type; };

        template<typename Tag>
        using view_for_t = typename view_for<Tag>::type;

    private:

        // Used by materialiser and editor
        category_id create_category(std::string_view name, category_id parent);
        category_id create_category(category_id id, std::string_view name, category_id parent);
        comment_id create_comment(std::string text);
        paragraph_id create_paragraph(std::string text);

        // Entity IDs are guaranteed to be monotonic without reuse.
        // The document controls the creation of IDs.
        //----------------------------------------------------------
        category_id   next_category_id_   = category_id {1}; // ID 0 is reserved for root
        key_id        next_key_id_        = key_id {0};
        comment_id    next_comment_id_    = comment_id {0};
        paragraph_id  next_paragraph_id_  = paragraph_id {0};
        table_id      next_table_id_      = table_id {0};
        row_id  next_row_id_        = row_id {0};
        column_id     next_column_id_     = column_id {0};

        // Translate tag (ID) to internal node storage type
        //----------------------------------------------------------
        template<typename Tag> struct node_for;

        template<> struct node_for<category_tag>     { typedef category_node type; };
        template<> struct node_for<key_tag>          { typedef key_node type; };
        template<> struct node_for<table_tag>        { typedef table_node type; };
        template<> struct node_for<row_tag>    { typedef row_node type; };
        template<> struct node_for<column_tag> { typedef column_node type; };
        template<> struct node_for<comment_tag>      { typedef comment_node type; };
        template<> struct node_for<paragraph_tag>    { typedef paragraph_node type; };

        template<typename Tag>
        using node_for_t = typename node_for<Tag>::type;
        
        // Convenience getter of internal node storage for a entity ID
        //----------------------------------------------------------
        template<typename T>
        constexpr typename document::node_for<T>::type* get_node( ::arf::id<T> id_ ) noexcept
        {
            using NodeT = typename document::node_for<T>::type;

            auto find_id = [id_](std::vector<NodeT> & nodes) -> NodeT *
            {
                if (auto it = std::ranges::find_if(nodes, [id_](auto& item){return item._id() == id_;}); it != nodes.end())
                    return &*it;
                return nullptr;
            };

            if constexpr      (std::is_same_v<T, category_tag>)     { return find_id(categories_); }
            else if constexpr (std::is_same_v<T, key_tag>)          { return find_id(keys_); }
            else if constexpr (std::is_same_v<T, table_tag>)        { return find_id(tables_); }
            else if constexpr (std::is_same_v<T, row_tag>)    { return find_id(rows_); }
            else if constexpr (std::is_same_v<T, column_tag>) { return find_id(columns_); }
            else if constexpr (std::is_same_v<T, comment_tag>)      { return find_id(comments_); }
            else if constexpr (std::is_same_v<T, paragraph_tag>)    { return find_id(paragraphs_); }
            else static_assert(false, "Illegal ID");
        };        
        
        // The source CST document from the parser
        //----------------------------------------------------------
        std::unique_ptr<parse_context> source_context_;

        
        // The storage structures for the document data populated
        // by the materialiser or editor
        //----------------------------------------------------------
        std::vector<category_node>   categories_;
        std::vector<table_node>      tables_;
        std::vector<column_node>     columns_;
        std::vector<row_node>        rows_;
        std::vector<key_node>        keys_;
        std::vector<comment_node>    comments_;
        std::vector<paragraph_node>  paragraphs_;

        // These collect contamination sources. Only data positions 
        // (keys and rows) are sources of contamination. Categories
        // and tables will propagate contaminiation but will never
        // be sources. 
        //
        // Note: The sets represent root contamination sources only.
        //       Node flags represent derived contamination state.
        //
        // A document is clean if these containers are empty
        // contaminated if there is at lease one record in either. 
        std::unordered_set<size_t>  contaminated_source_keys_;
        std::unordered_set<size_t>  contaminated_source_rows_;

        // These imperatively set the clean state. Prefer
        // the request_clear_contamination method to allow 
        // the document or tooling to delegate the decision.
        void clear_key_contamination(key_id id);
        void clear_row_contamination(row_id id);        

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

        bool key_is_clean(const key_node& k) const;
        bool row_is_clean(const row_node& r) const;
        bool table_is_clean(const table_node& t) const;
        bool category_is_clean(const category_node& c) const;
        void propagate_contamination_up_category_chain(category_id id);
        void try_clear_category_contamination(category_id id);
    };


//========================================================================
// Nodes
//========================================================================

        struct document::category_close_marker
        {
            category_id         which;
            category_close_form form;

            bool operator==(category_close_marker const & rhs) const noexcept { return which == rhs.which; }
        };

        struct document::source_item_ref
        {
            source_id id;

            bool operator==(source_item_ref const & rhs) const noexcept 
            {
                if (id.index() != rhs.id.index())
                    return false;

                return std::visit([&rhs](auto const & lhs_val)
                {
                    using T = std::decay_t<decltype(lhs_val)>;
                    return lhs_val.operator==(std::get<T>(rhs.id));
                }, id);                
            }
        };

        struct document::category_node : document::node<false, true>
        {
            typedef category_id id_type;
            id_type _id() const noexcept { return id; }
            std::string_view _name() const noexcept { return name; }
            
            category_id                  id;
            std::string                  name;
            category_id                  parent;
            std::vector<category_id>     children;
            std::vector<table_id>        tables;
            std::vector<key_id>          keys;
            std::vector<source_item_ref> ordered_items;

            // Authorship metadata, instead of source_event_index:
            std::optional<size_t>        source_event_index_open;   // Category open event
            std::optional<size_t>        source_event_index_close;  // Category close event (if explicit)
        };

        struct document::table_node : document::node<>
        {
            typedef table_id id_type;
            id_type _id() const noexcept { return id; }
            std::string_view _name() const noexcept = delete;
            
            table_id                     id;
            category_id                  owner;
            std::vector<column_id>       columns;
            std::vector<row_id>    rows;          // semantic collection (all rows)
            std::vector<source_item_ref> ordered_items; // authored order (rows + comments + paragraphs + subcategories)
        };

        struct document::column_node : document::node<true, false>
        {
            typedef column_id id_type;
            id_type _id() const noexcept { return col.id; }
            std::string_view _name() const noexcept { return col.name; }
            value_type _type() const noexcept { return col.type; }
            
            struct column   col;
            table_id        table;
            category_id     owner;
        };

        struct document::row_node : document::node<>
        {
            typedef row_id id_type;
            id_type _id() const noexcept { return id; }
            std::string _name() const noexcept
            {
                if (cells.empty()) return "";
                auto const & cell = cells.front();
                return cell.value_to_string();
            }

            row_id             id;
            table_id                 table;
            category_id              owner;
            std::vector<typed_value> cells;
        };

        struct document::key_node : document::node<>
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
        };

        struct document::comment_node : document::node<>
        {
            typedef comment_id id_type;
            id_type _id() const noexcept { return id; }

            comment_id  id;
            std::string text;   // verbatim, may be multi-line, includes "//" and preserves leading whitespace and line breaks
            category_id owner {invalid_id<category_tag>()};
        };

        struct document::paragraph_node : document::node<>
        {
            typedef paragraph_id id_type;
            id_type _id() const noexcept { return id; }

            paragraph_id id;
            std::string  text;  // verbatim, may be multi-line, preserves leading whitespace and line breaks
            category_id  owner {invalid_id<category_tag>()} ;
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
        bool is_root() const noexcept { return node->parent == invalid_id<category_tag>(); }

        std::span<const category_id> children() const noexcept { return node->children; }
        std::span<const table_id> tables() const noexcept { return node->tables; }
        std::span<const key_id> keys() const noexcept { return node->keys;}

        std::optional<category_view> parent() const noexcept;
        std::optional<category_view> child(std::string_view name) const noexcept;
        std::optional<key_view> key(std::string_view name) const noexcept;

        size_t children_count() const noexcept { return node->children.size(); }
        size_t tables_count() const noexcept { return node->tables.size(); }
        size_t keys_count() const noexcept { return node->keys.size(); }

        bool is_locally_valid() const noexcept { return node->semantic == semantic_state::valid; }
        bool is_contaminated() const noexcept { return node->contamination == contamination_state::contaminated; }
    };

    struct document::table_view
    {
        const document* doc;
        const table_node* node;

        table_id id() const noexcept { return node->id; }
        category_view owner() const noexcept;

        std::span<const column_id> columns() const noexcept { return node->columns; }
        std::span<const row_id> rows() const noexcept { return node->rows; }

        size_t column_count() const noexcept { return node->columns.size(); }
        size_t row_count() const noexcept { return node->rows.size(); }

        std::optional<column_view> column( column_id id ) const noexcept;
        std::optional<column_view> column( std::string_view name ) const noexcept;

        std::optional<size_t> column_index(std::string_view name) const noexcept;        
        std::optional<size_t> column_index(column_id id) const noexcept;        

        std::optional<size_t> row_index(std::string_view name) const noexcept;        
        std::optional<size_t> row_index(row_id id) const noexcept;        

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

        row_id id() const noexcept { return node->id; }
        std::string name() const noexcept { return node->_name(); }
        std::span<const typed_value> cells() const noexcept { return node->cells; }

        table_view table() const noexcept;
        category_view owner() const noexcept;
        size_t index() const noexcept;

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
        
        bool is_array() const noexcept { auto v = node->value.type; return v == value_type::string_array || v == value_type::integer_array || v == value_type::floating_point_array; }
        size_t indices() const noexcept;

        bool is_locally_valid() const noexcept { return node->semantic == semantic_state::valid; }
        bool is_contaminated() const noexcept { return node->contamination == contamination_state::contaminated; }
    };


//========================================================================
// document member implementations
//========================================================================

    inline category_id document::create_root()
    {
        if (categories_.empty())
        {
            category_node root;
            root.id     = category_id{0};
            root.name   = detail::ROOT_CATEGORY_NAME.data();
            root.parent = invalid_id<category_tag>();

            categories_.push_back(std::move(root));
        }
        assert (categories_.front().id == category_id{0});        
        return category_id{0};
    }    

    inline category_id document::create_category( std::string_view name, category_id parent )
    {
        if (auto pcat = get_node(parent))
        {
            if (auto dup = category(name))
                if (dup->parent()->id() == parent)
                    return invalid_id<category_tag>();

            auto id = create_category_id();
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

        return invalid_id<category_tag>();
    }

    inline category_id document::create_category( category_id id, std::string_view name, category_id parent )
    {
        // Only root have the ID 0 or an invalid parent ID. Either of
        // these being explicitly set is interpreted as an attempt to
        // create the root category.
        if (id == category_id{0} || parent == invalid_id<category_tag>())
        {
            if (categories_.empty())
                return create_root();

            return invalid_id<category_tag>();
        }

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

    inline comment_id document::create_comment(std::string text)
    {
        comment_id cid{comments_.size()};
        comments_.push_back({.id = cid, .text = text});
        return cid;
    }

    inline paragraph_id document::create_paragraph(std::string text)
    {
        paragraph_id pid{paragraphs_.size()};
        paragraphs_.push_back({.id = pid, .text = text});
        return pid;
    }

    inline void document::mark_key_contaminated(key_id id)
    {
        if (contaminated_source_keys_.contains(static_cast<size_t>(id)))
            return;

        auto* kn = get_node(id);
        if (!kn) return;
        
        // Mark key and value as contaminated
        kn->contamination = contamination_state::contaminated;
        kn->value.contamination = contamination_state::contaminated;
        
        // Register as source
        contaminated_source_keys_.insert(static_cast<size_t>(id));
        
        // Propagate flags upward (but don't register containers)
        if (kn->owner != invalid_id<category_tag>())
        {
            auto* cat = get_node(kn->owner);
            if (cat)
            {
                cat->contamination = contamination_state::contaminated;
                propagate_contamination_up_category_chain(kn->owner);
            }
        }
    }

    inline void document::mark_row_contaminated(row_id id)
    {
        auto* rn = get_node(id);
        if (!rn) return;
        
        // Mark row as contaminated
        rn->contamination = contamination_state::contaminated;
        
        // Register as source
        contaminated_source_rows_.insert(static_cast<size_t>(id));
        
        // Propagate to table
        auto* tbl = get_node(rn->table);
        if (tbl)
        {
            tbl->contamination = contamination_state::contaminated;
            
            // Propagate to owning category
            auto* cat = get_node(tbl->owner);
            if (cat)
            {
                cat->contamination = contamination_state::contaminated;
                propagate_contamination_up_category_chain(tbl->owner);
            }
        }
    }

    inline void document::propagate_contamination_up_category_chain(category_id id)
    {
        auto* cat = get_node(id);
        if (!cat) return;

        auto parent_id = cat->parent;
        if (parent_id == invalid_id<category_tag>())
            return;

        auto* parent = get_node(parent_id);
        if (!parent) return;

        if (parent->contamination == contamination_state::contaminated)
            return;

        parent->contamination = contamination_state::contaminated;
        propagate_contamination_up_category_chain(parent_id);
    }

    inline bool document::key_is_clean(const key_node& k) const
    {
        if (k.semantic != semantic_state::valid)
            return false;

        if (k.value.contamination == contamination_state::contaminated)
            return false;
        
        if (k.value.semantic == semantic_state::invalid)
            return false;
        
        // Check array elements
        if (is_array(k.value))
        {
            auto& arr = std::get<std::vector<typed_value>>(k.value.val);
            for (auto& elem : arr)
            {
                if (elem.semantic == semantic_state::invalid ||
                    elem.contamination == contamination_state::contaminated)
                    return false;
            }
        }
        
        return true;
    }

    inline bool document::row_is_clean(const row_node& r) const
    {
        if (r.semantic != semantic_state::valid)
            return false;
        
        for (auto& cell : r.cells)
        {
            if (cell.semantic == semantic_state::invalid ||
                cell.contamination == contamination_state::contaminated)
                return false;
            
            // Check array cells
            if (is_array(cell))
            {
                auto& arr = std::get<std::vector<typed_value>>(cell.val);
                for (auto& elem : arr)
                {
                    if (elem.semantic == semantic_state::invalid ||
                        elem.contamination == contamination_state::contaminated)
                        return false;
                }
            }
        }
        
        return true;
    }

    inline bool document::table_is_clean(const table_node& t) const
    {
        if (t.semantic != semantic_state::valid)
            return false;
        
        // Check all columns
        for (auto col_id : t.columns)
        {
            auto it = find_node_by_id(columns_, col_id);
            if (it != columns_.end() && it->col.semantic != semantic_state::valid)
                return false;
        }
        
        // Check all rows
        for (auto row_id : t.rows)
        {
            auto it = find_node_by_id(rows_, row_id);
            if (it != rows_.end() && !row_is_clean(*it))
                return false;
        }
        
        return true;
    }

    inline bool document::category_is_clean(const category_node& c) const
    {
        if (c.semantic != semantic_state::valid)
            return false;
        
        // Check all keys
        for (auto kid : c.keys)
        {
            auto it = find_node_by_id(keys_, kid);
            if (it != keys_.end() && !key_is_clean(*it))
                return false;
        }
        
        // Check all tables
        for (auto tid : c.tables)
        {
            auto it = find_node_by_id(tables_, tid);
            if (it != tables_.end() && !table_is_clean(*it))
                return false;
        }
        
        // Check all subcategories
        for (auto cid : c.children)
        {
            auto it = find_node_by_id(categories_, cid);
            if (it != categories_.end() && 
                it->contamination == contamination_state::contaminated)
                return false;
        }
        
        return true;
    }

    inline bool document::request_clear_contamination(clearable_node node)
    {
        // Step 1: Validate node is actually clean
        bool is_clean = std::visit([this](auto id) -> bool 
        {
            using T = decltype(id);
            if constexpr (std::is_same_v<T, key_id>) 
            {
                auto* kn = get_node(id);
                return kn && key_is_clean(*kn);
            } 
            else 
            {
                auto* rn = get_node(id);
                return rn && row_is_clean(*rn);
            }
        }, node);
        
        if (!is_clean)
            return false;
        
        // Step 2: Ask permission
        if (!request_clear_fn(node))
            return false;
        
        // Step 3: Perform clearing
        std::visit([this](auto id) 
        {
            using T = decltype(id);
            if constexpr (std::is_same_v<T, key_id>)
                clear_key_contamination(id);
            else
                clear_row_contamination(id);
        }, node);
        
        return true;
    }

    inline void document::clear_key_contamination(key_id id)
    {
        auto* kn = get_node(id);
        if (!kn) return;
        
        // Only clear if actually clean
        if (!key_is_clean(*kn))
            return;
        
        // Clear flags
        kn->contamination = contamination_state::clean;
        kn->value.contamination = contamination_state::clean;
        
        // Unregister as source
        contaminated_source_keys_.erase(static_cast<size_t>(id));
        
        // Try to clear parent category
        if (kn->owner != invalid_id<category_tag>())
            try_clear_category_contamination(kn->owner);
    }

    inline void document::clear_row_contamination(row_id id)
    {
        auto* rn = get_node(id);
        if (!rn) return;
        
        if (!row_is_clean(*rn))
            return;
        
        rn->contamination = contamination_state::clean;
        contaminated_source_rows_.erase(static_cast<size_t>(id));
        
        auto* tbl = get_node(rn->table);
        if (!tbl) return;

        if (table_is_clean(*tbl))
        {
            tbl->contamination = contamination_state::clean;
            try_clear_category_contamination(tbl->owner);
        }
    }

    inline void document::try_clear_category_contamination(category_id id)
    {
        auto* cat = get_node(id);
        if (!cat) return;
        
        if (!category_is_clean(*cat))
            return;
        
        cat->contamination = contamination_state::clean;
        
        // Continue up to parent
        if (cat->parent != invalid_id<category_tag>())
            try_clear_category_contamination(cat->parent);
    }

    inline std::optional<document::category_view>
    document::root() const noexcept
    {
        if (categories_.empty())
            return std::nullopt;

        return category_view{ this, &categories_.front() };
    }

    size_t document::category_count() const noexcept { return categories_.size(); }
    size_t document::table_count() const noexcept { return tables_.size(); }
    size_t document::column_count() const noexcept { return columns_.size(); }
    size_t document::row_count() const noexcept { return rows_.size(); }
    size_t document::key_count() const noexcept { return keys_.size(); }
    size_t document::comment_count() const noexcept { return comments_.size(); }
    size_t document::paragraph_count() const noexcept { return paragraphs_.size(); }

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

    std::optional<size_t> document::table_view::row_index(std::string_view name) const noexcept
    {
        size_t i = 0;
        for (auto const & rid : rows())
        {
            auto r = doc->row(rid);
            if (r.has_value() && r->name() == name) return i;
            ++i;
        }
        return std::nullopt;
    }

    std::optional<size_t> document::table_view::row_index(row_id id) const noexcept
    {
        size_t i = 0;
        for (auto const & rid : rows())
        {
            if (rid == id) return i;
            ++i;
        }
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
    document::row(row_id id) const noexcept { return to_view(rows_, find_node_by_id(rows_, id)); }

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

    size_t document::key_view::indices() const noexcept 
    { 
        if (is_array())
            return std::get<std::vector<typed_value>>(node->value.val).size();
        return 0;
    }


    document::category_view document::table_view::owner()     const noexcept { return *doc->to_view(doc->categories_, doc->find_node_by_id(doc->categories_, node->owner)); }
    document::category_view document::column_view::owner()    const noexcept { return *doc->to_view(doc->categories_, doc->find_node_by_id(doc->categories_, node->owner)); }
    document::table_view    document::column_view::table()    const noexcept { return *doc->to_view(doc->tables_,     doc->find_node_by_id(doc->tables_,     node->table)); }
    document::category_view document::table_row_view::owner() const noexcept { return *doc->to_view(doc->categories_, doc->find_node_by_id(doc->categories_, node->owner)); }
    document::table_view    document::table_row_view::table() const noexcept { return *doc->to_view(doc->tables_,     doc->find_node_by_id(doc->tables_,     node->table)); }
    document::category_view document::key_view::owner()       const noexcept { return *doc->to_view(doc->categories_, doc->find_node_by_id(doc->categories_, node->owner)); }

    std::optional<document::column_view> document::table_view::column( column_id id ) const noexcept
    {
        for (auto col_id : columns())
            if (col_id == id)
                return doc->column(id);
        return std::nullopt;
    }
    std::optional<document::column_view> document::table_view::column( std::string_view name ) const noexcept
    {
        for (auto col_id : columns())
            if (auto col = doc->column(col_id); col->name() == name)
                return col;
        return std::nullopt;
    }

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

    size_t document::table_row_view::index() const noexcept
    {
        return *table().row_index(id());
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

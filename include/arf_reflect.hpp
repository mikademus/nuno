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

#include "arf_core.hpp"
#include "arf_document.hpp"

#include <array>
#include <utility>
#include <variant>
#include <vector>
#include <optional>
#include <string_view>

namespace arf::reflect
{

// ------------------------------------------------------------
// address step diagnostics
// ------------------------------------------------------------

    enum class step_state : uint8_t
    {
        ok,
        uninspected,
        unresolved,
        error
    };

    enum class step_error : uint8_t
    {
        none,

    // Missing context
        no_context_value,
        no_category_context,
        no_table_context,
        no_row_context,    

    // Malformed address
        top_category_after_category,
        structure_after_value,
        sub_category_under_root, 

    // Missing structure
        top_category_not_found,
        sub_category_not_found,
        key_not_found,
        table_not_found,
        row_not_found,
        column_not_found,

        row_not_owned,

    // Type error
        not_a_table,
        not_a_row,
        not_an_array,
        index_out_of_bounds,

        __LAST
    };

    constexpr std::array<std::string_view, static_cast<size_t>(step_error::__LAST)> 
    step_error_string =
    {
        "none",
        "no_context_value",
        "no_category_context",
        "no_table_context",
        "no_row_context", 
        "top_category_after_category",
        "structure_after_value",
        "sub_category_under_root",
        "top_category_not_found",
        "sub_category_not_found",
        "key_not_found",
        "table_not_found",
        "row_not_found",
        "column_not_found",
        "row_not_owned",
        "not_a_table",
        "not_a_row",
        "not_an_array",
        "index_out_of_bounds"
    };

    struct step_diagnostic
    {
        step_state state = step_state::ok;
        step_error error = step_error::none;
    };

// ------------------------------------------------------------
// address steps
// ------------------------------------------------------------

    struct top_category_step
    {
        explicit top_category_step(std::string_view n) : name(n)  
        {
        }
        std::string_view name;
    };

    struct sub_category_step
    {
        explicit sub_category_step(std::string_view n) : name(n)  
        {
        }
        std::string_view name;
    };

    struct key_step
    {
        explicit key_step(arf::key_id id) : id(id) {}
        explicit key_step(std::string_view n) : id(n) {}
        std::variant<key_id, std::string_view> id;
    };

    struct table_step
    {
        explicit table_step(table_id id) : id(id)  {}
        explicit table_step(size_t loc_idx) : id(loc_idx)  {}
        std::variant<table_id, size_t> id; // id or local ordinal
    };

    struct row_step
    {
        explicit row_step(row_id id) : id(id)  {}
        row_id id;
    };

    struct column_step
    {
        explicit column_step(column_id id) : id(id) {}
        explicit column_step(std::string_view name) : id(name) {}
        std::variant<column_id, std::string_view> id;
    };

    struct index_step
    {
        explicit index_step(size_t idx) : index(idx) {}
        size_t index;
    };

    using address_step =
        std::variant<
            key_step,
            top_category_step,
            sub_category_step,
            table_step,
            row_step,
            column_step,
            index_step
        >;

// ------------------------------------------------------------
// addressed_step
// ------------------------------------------------------------
// Diagnostic is written by inspect().
// Not thread-safe if the address is shared.
// ------------------------------------------------------------

        struct addressed_step
        {
            address_step     step;
            step_diagnostic  diagnostic {step_state::uninspected, step_error::none};

            template <typename T, typename... Args>
            addressed_step(std::in_place_type_t<T>, Args&&... args)
                : step(std::in_place_type<T>, std::forward<Args>(args)...)
            {}            
        };

// ------------------------------------------------------------
// address and address builder
// ------------------------------------------------------------
// Address represents a logical path.
// Diagnostics are written during inspection.
// ------------------------------------------------------------

    struct address
    {
        std::vector<addressed_step> steps;

        address& top(std::string_view name)
        {
            steps.emplace_back(std::in_place_type<top_category_step>, name);
            return *this;
        }

        address& sub(std::string_view name)
        {
            steps.emplace_back(std::in_place_type<sub_category_step>, name);
            return *this;
        }

        address& key(key_id id)
        {
            steps.emplace_back(std::in_place_type<key_step>, id);
            return *this;
        }

        address& key(std::string_view name)
        {
            steps.emplace_back(std::in_place_type<key_step>, name);
            return *this;
        }

        address& table(table_id id)
        {
            steps.emplace_back(std::in_place_type<table_step>, id);
            return *this;
        }

        address& local_table(size_t ordinal)
        {
            steps.emplace_back(std::in_place_type<table_step>, ordinal);
            return *this;
        }

        address& row(row_id id)
        {
            steps.emplace_back(std::in_place_type<row_step>, id);
            return *this;
        }

        address& column(column_id id)
        {
            steps.emplace_back(std::in_place_type<column_step>, id);
            return *this;
        }

        address& column(std::string_view name)
        {
            steps.emplace_back(std::in_place_type<column_step>, name);
            return *this;
        }

        address& index(size_t i)
        {
            steps.emplace_back(std::in_place_type<index_step>, i);
            return *this;
        }

        bool has_error() const noexcept
        {
            auto b = std::all_of(
                steps.begin(),
                steps.end(),
                [](const addressed_step& s)
                {
                    return s.diagnostic.state != step_state::error;
                }
            );
            return !b;
        }
    };

    inline std::string_view to_string(address_step const &step)
    {
        if (std::holds_alternative<key_step>(step)) return "key";
        if (std::holds_alternative<top_category_step>(step)) return "top_category_step";
        if (std::holds_alternative<sub_category_step>(step)) return "sub_category_step";
        if (std::holds_alternative<table_step>(step)) return "table_step";
        if (std::holds_alternative<row_step>(step)) return "row_step";
        if (std::holds_alternative<column_step>(step)) return "column_step";
        if (std::holds_alternative<index_step>(step)) return "index_step";
        return "unknown";
    }
    inline std::string to_string(address const & addr)    
    {
        std::string str;
        int i = 0;
        for (auto const & s : addr.steps)
        {
            str += to_string(s.step);
            if (++i < addr.steps.size())
                str += "->";
        }
        return str;
    }


    static_assert(std::is_copy_constructible_v<address>, "address must be copyable for const inspect() to work");

    inline address root()
    {
        return address{};
    }

// ------------------------------------------------------------
// inspect context
// ------------------------------------------------------------

    struct inspect_context
    {
        const document* doc = nullptr;

        std::optional<document::category_view>  category;
        std::optional<document::table_view>     table;
        std::optional<document::table_row_view> row;
        std::optional<document::column_view>    column;
        std::optional<document::key_view>       key;
        const typed_value*                      value = nullptr;
    };    

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------

    inline std::optional<table_id>
    resolve_table_ordinal(const document::category_view& cat, size_t ordinal)
    {
        auto tbls = cat.tables();
        if (ordinal >= tbls.size())
            return std::nullopt;

        return tbls[ordinal];
    }

// ------------------------------------------------------------
// inspection results
// ------------------------------------------------------------

     using inspected_item =
        std::variant<
            std::monostate,
            document::category_view,
            document::table_view,
            document::table_row_view,
            document::column_view,
            document::key_view,
            const typed_value*
        >;
    
    struct structural_child
    {
        enum class kind
        {
            top_category,
            sub_category,
            key,
            table,
            row,
            column,
            index
        };

        kind kind;
        std::string_view name;   // empty for anonymous (row, index)
        size_t           ordinal = 0; // IDs for tables and rows, index for arrays
    };

    inline std::string_view to_string(enum structural_child::kind kind)
    {
        switch (kind)
        {
            case structural_child::kind::top_category: return "top_category";
            case structural_child::kind::sub_category: return "sub_category";
            case structural_child::kind::key: return "key";
            case structural_child::kind::table: return "table";
            case structural_child::kind::row: return "row";
            case structural_child::kind::column: return "column";
            case structural_child::kind::index: return "index";
            default: return "unknown";
        }
    }

    struct prefix_match
    {
        structural_child child;
        address          extended;
    };    

    struct inspected
    {
        // const address*     addr {nullptr};
        std::optional<address> addr;
        inspected_item     item;
        const typed_value* value;
        size_t             steps_inspected;

        bool fully_inspected() const { return addr && steps_inspected == addr->steps.size(); }
        bool ok()              const { return fully_inspected() && !addr->has_error(); }       
        bool has_error()       const { return addr && addr->has_error(); }        

        bool is_category() const noexcept { return std::holds_alternative<document::category_view>(item); }
        bool is_value() const noexcept    { return value != nullptr; }//return std::holds_alternative<document::key_view>(item); }
        bool is_table() const noexcept    { return std::holds_alternative<document::table_view>(item); }
        bool is_row() const noexcept      { return std::holds_alternative<document::table_row_view>(item); }
        bool is_column() const noexcept   { return std::holds_alternative<document::column_view>(item); }
        //bool is_array() const noexcept    { return value && reflect::is_array(value->type); }

        size_t first_error_step() const
        {
            if (!addr) return steps_inspected;
            for (size_t i = 0; i < addr->steps.size(); ++i)
                if (addr->steps[i].diagnostic.state == step_state::error)
                    return i;
            return addr->steps.size();
        }
        
        // Returns the immediate structural children of the currently inspected item.
        //
        // Structural children describe what the document declares at this location,
        // independent of whether a full address resolved successfully.
        //
        // Children are yielded in the same order as authored in the source document.
        // This order is stable and must not be altered by name, kind, or ordinal sorting.
        //
        // Key properties:
        // - Operates on the last successfully resolved structural item, not on values
        // - Never fails, never mutates, and never performs further resolution
        // - Preserves authored document order
        //
        // Failure semantics:
        // - If inspection stopped early due to an error, children are derived from the
        //   last valid item before the failure
        // - If no structural item exists (item is std::monostate), the result is empty
        //
        // Typical use cases:
        // - Editor navigation and discovery
        // - Prefix-based address completion
        // - Structural introspection and tooling
        std::vector<structural_child>
        structural_children(const inspect_context& ctx) const;

        // Returns the structural children that are valid continuations of the current
        // address prefix(the last successfully inspected prefix of this address).
        //
        // If inspection failed partway through the address,
        // children are computed from the last valid structural
        // item (category, table, row, etc.), not from the full
        // unresolved address.
        //
        // This is the foundation for editor-style navigation,
        // suggestions, and address completion.
        //
        // This is a semantic alias of structural_children(), provided to make
        // intent explicit at call sites where an address is treated as a prefix
        // for navigation, discovery, or completion rather than as a full query.
        //
        // No additional resolution is performed.
        std::vector<structural_child>
        prefix_children(const inspect_context& ctx) const
        {
            return structural_children(ctx);
        }

        // Construct a new address by extending the inspected
        // address prefix with a structural child.
        //
        // The child must be valid in the context of this
        // inspection result. No validation or re-inspection
        // is performed here.     
        address extend_address(const structural_child& child) const;

        // Returns structural children whose names begin with
        // the supplied string prefix.
        //
        // Matching is literal and case-sensitive.
        // Ordering is preserved from the document.
        std::vector<prefix_match>
        prefix_children_matching(const inspect_context& ctx, std::string_view prefix) const;

        std::vector<address>
        suggest_next(inspect_context& ctx, std::string_view prefix) const;
    };    

// ------------------------------------------------------------
// inspect - mutable version
// ------------------------------------------------------------
// Mutable version: Writes diagnostics to addr.
// Not thread-safe if addr is shared across threads.
// Most efficient for single-threaded reuse.
// ------------------------------------------------------------
// Note: Addresses can be reused across multiple inspections.
// Each inspection resets and rewrites address diagnostic state.
// The address in the returned inpected object is a frozen copy.
// ------------------------------------------------------------

    inline inspected inspect(
        inspect_context& ctx,
        address& addr
    )
    {
        // reset context
        ctx.category = ctx.doc->root();
        ctx.table.reset();
        ctx.row.reset();
        ctx.column.reset();
        ctx.key.reset();
        ctx.value = nullptr;

        inspected out;
        out.item = *ctx.category;
        out.value = nullptr;
        out.steps_inspected = 0;

        inspected_item last_valid_item = *ctx.category;

        for (size_t i = 0; i < addr.steps.size(); ++i)
        {
            auto& astep = addr.steps[i];
            auto& diag  = astep.diagnostic;
            const auto& step = astep.step;

            diag = {};
            diag.state = step_state::ok;

            auto error = [&diag](step_error what)
            {
                diag.state = step_state::error;
                diag.error = what;
            };

            // ---------------- key
            if (auto s = std::get_if<key_step>(&step))
            {
                if (!ctx.category)
                    error(step_error::no_category_context);
                else
                {
                    auto k =
                        std::holds_alternative<key_id>(s->id)
                            ? ctx.doc->key(std::get<key_id>(s->id))
                            : ctx.category->key(std::get<std::string_view>(s->id));

                    if (!k)
                        error(step_error::key_not_found);
                    else
                    {
                        ctx.value = &k->value();
                        ctx.key = k;
                    }
                }
            }

            // ---------------- top category
            else if (auto s = std::get_if<top_category_step>(&step))
            {
                if (ctx.value)
                    error(step_error::structure_after_value);

                else if (ctx.category && ctx.category->id() != ctx.doc->root()->id())
                    error(step_error::top_category_after_category);

                else
                {
                    auto next = ctx.doc->root()->child(s->name);
                    if (!next)
                        error(step_error::top_category_not_found);
                    else
                    {
                        ctx.category = next;
                        ctx.table.reset();
                        ctx.row.reset();
                        ctx.column.reset();
                        ctx.value = nullptr;
                    }
                }
            }

            // ---------------- sub category
            else if (auto s = std::get_if<sub_category_step>(&step))
            {
                if (ctx.value)
                    error(step_error::structure_after_value);

                else if (!ctx.category)
                    error(step_error::no_category_context);

                else if (ctx.category->id() == ctx.doc->root()->id())
                    error(step_error::sub_category_under_root);

                else
                {
                    auto next = ctx.category->child(s->name);
                    if (!next)
                        error(step_error::sub_category_not_found);
                    else
                    {
                        ctx.category = next;
                        ctx.table.reset();
                        ctx.row.reset();
                        ctx.column.reset();
                        ctx.value = nullptr;
                    }
                }
            }

            // ---------------- table
            else if (auto s = std::get_if<table_step>(&step))
            {
                if (ctx.value)
                    error(step_error::structure_after_value);

                else if (!ctx.category)
                    error(step_error::no_category_context);

                else
                {
                    std::optional<table_id> tid =
                        std::holds_alternative<table_id>(s->id)
                            ? std::optional{ std::get<table_id>(s->id) }
                            : resolve_table_ordinal(*ctx.category, std::get<size_t>(s->id));

                    if (!tid)
                        error(step_error::table_not_found);

                    else
                    {
                        auto tbl = ctx.doc->table(*tid);
                        if (!tbl)
                            error(step_error::table_not_found);
                        else
                        {
                            ctx.table = tbl;
                            ctx.row.reset();
                            ctx.column.reset();
                            ctx.value = nullptr;
                        }
                    }
                }
            }

            // ---------------- row
            else if (auto s = std::get_if<row_step>(&step))
            {
                if (ctx.value)
                    error(step_error::structure_after_value);

                else if (!ctx.table)
                    error(step_error::no_table_context);

                else
                {
                    auto r = ctx.doc->row(s->id);
                    if (!r)
                        error(step_error::row_not_found);
                    else
                    {
                        bool owned = false;
                        for (auto rid : ctx.table->rows())
                        {
                            if (rid == s->id)
                            {
                                owned = true;
                                break;
                            }
                        }

                        if (!owned)
                            error( step_error::row_not_owned);

                        else
                        {
                            ctx.row = r;
                            ctx.column.reset();
                            ctx.value = nullptr;
                        }
                    }
                }
            }

            // ---------------- column
            else if (auto s = std::get_if<column_step>(&step))
            {
                if (ctx.value)
                    error(step_error::structure_after_value);

                else if (!ctx.table)
                    error(step_error::no_table_context);

                else if (!ctx.row)
                    error(step_error::no_row_context);

                else
                {
                    auto col =
                        std::holds_alternative<column_id>(s->id)
                            ? ctx.doc->column(std::get<column_id>(s->id))
                            : ctx.table->column(std::get<std::string_view>(s->id));

                    if (!col)
                        error(step_error::column_not_found);
                    else
                    {
                        ctx.column = col;
                        ctx.value = &ctx.row->cells()[col->index()];
                    }
                }
            }

            // ---------------- index
            else if (auto s = std::get_if<index_step>(&step))
            {
                if (!ctx.value)
                    error(step_error::no_context_value);

                else if (!is_array(*ctx.value))
                    error(step_error::not_an_array);

                else
                {
                    auto& arr = std::get<std::vector<typed_value>>(ctx.value->val);
                    if (s->index >= arr.size())
                        error(step_error::index_out_of_bounds);
                    else
                        ctx.value = &arr[s->index];
                }
            }

            ++out.steps_inspected;

            if (diag.state == step_state::error)
                break;
        
            if (diag.state == step_state::ok)
            {
                if (std::holds_alternative<index_step>(astep.step))
                {
                    last_valid_item = ctx.value;
                }
                else if (std::holds_alternative<column_step>(astep.step))
                {
                    if (ctx.value)
                        last_valid_item = ctx.value;
                    else if (ctx.column)
                        last_valid_item = *ctx.column;
                }
                else if (std::holds_alternative<key_step>(astep.step))
                {
                    if (ctx.key)
                    {
                        // If this is an array value and we might index it,
                        // store the value pointer instead of the key_view
                        const auto& val = ctx.key->value();
                        if (is_array(val))
                            last_valid_item = &val;
                        else
                            last_valid_item = *ctx.key;
                    }
                }
                else if (std::holds_alternative<row_step>(astep.step))
                {
                    last_valid_item = *ctx.row;
                }
                else if (std::holds_alternative<table_step>(astep.step))
                {
                    last_valid_item = *ctx.table;
                }
                else if (std::holds_alternative<top_category_step>(astep.step) ||
                        std::holds_alternative<sub_category_step>(astep.step))
                {
                    last_valid_item = *ctx.category;
                }
            }

        }

        out.item = last_valid_item;
        out.addr = addr;

        // Extract value from item
        if (auto pv = std::get_if<const typed_value*>(&out.item))
        {
            out.value = *pv;
        }
        else if (auto kv = std::get_if<document::key_view>(&out.item))
        {
            out.value = &kv->value();
        }
        else
        {
            out.value = nullptr;
        }    

        return out;
    }

// ------------------------------------------------------------
// inspect - immutable version
// ------------------------------------------------------------
// Const version: Copies addr internally, returns diagnostics in result.
// Thread-safe for shared addresses.
// Slightly less efficient (one copy + allocation).
// ------------------------------------------------------------
    inspected inspect(inspect_context& ctx, const address& addr)
    {
        address temp = addr;
        return inspect(ctx, temp);
    }

// ------------------------------------------------------------
// structural query
// ------------------------------------------------------------

    inline std::vector<structural_child>
    inspected::structural_children(const inspect_context& ctx) const
    {
        std::vector<structural_child> out;

        if (!ctx.doc)
            return out;

        struct structural_query_result {};

        auto query_category = [&](auto&& node) -> structural_query_result
        {
            // Special case: root category lists top-level categories
            if (node.is_root())
            {
                for (const auto& cat : ctx.doc->categories())
                {
                    if (!cat.is_root() && cat.parent()->is_root())
                    {
                        out.push_back({
                            structural_child::kind::top_category,
                            cat.name(),
                            0
                        });
                    }
                }
            }
            else
            {
                // Non-root: list subcategories normally
                for (auto sc : node.children())
                    if (auto cat = ctx.doc->category(sc); cat.has_value())
                    {
                        out.push_back({
                            structural_child::kind::sub_category,
                            cat->name(),
                            0
                        });
                    }
            }

            // keys
            for (auto k : node.keys())
                if (auto key = ctx.doc->key(k); key.has_value())
                {
                    out.push_back({
                        structural_child::kind::key,
                        key->name(),
                        0
                    });
                }

            // tables (ordinal only)
            size_t i = 0;
            for (auto tid : node.tables())
            {
                (void)tid;
                out.push_back({
                    structural_child::kind::table,
                    {},
                    static_cast<size_t>(tid)
                });
            }

            return structural_query_result{};
        };

        auto query_table = [&](auto&& node) -> structural_query_result
        {
            for (auto rid : node.rows())
            {
                out.push_back({
                    structural_child::kind::row,
                    {},
                    static_cast<size_t>(rid)
                });
            }
            return structural_query_result{};
        };

        auto query_table_row = [&](auto&& node) -> structural_query_result
        {
            for (auto c : node.table().columns())
                if (auto col = ctx.doc->column(c); col.has_value())
                {
                    out.push_back({
                        structural_child::kind::column,
                        col->name(),
                        col->index()
                    });
                }
            return structural_query_result{};
        };

        auto query_value = [&](auto&& node) -> structural_query_result
        {
            if (node && is_array(*node))
            {
                auto& arr = std::get<std::vector<typed_value>>(node->val);
                for (size_t i = 0; i < arr.size(); ++i)
                {
                    out.push_back({
                        structural_child::kind::index,
                        {},
                        i
                    });
                }
            }
            return structural_query_result{};
        };

        std::visit([&](auto&& node)
        {
            using T = std::decay_t<decltype(node)>;

            if constexpr (std::is_same_v<T, document::category_view>)
                query_category(node);

            else if constexpr (std::is_same_v<T, document::table_view>)
                query_table(node);

            else if constexpr (std::is_same_v<T, document::table_row_view>)
                query_table_row(node);

            else if constexpr (std::is_same_v<T, const typed_value*>)
                query_value(node);

            else if constexpr (std::is_same_v<T, document::key_view>)
                query_value(&node.value());
        }, item);

        return out;
    }    

    address inspected::extend_address(const structural_child& child) const
    {
        address next;

        // Determine extension point:
        // - if inspection fully succeeded → extend at end
        // - if inspection failed → extend at first failing step
        size_t prefix_len = ok() ? steps_inspected : first_error_step();

        next.steps.assign(
            addr->steps.begin(),
            addr->steps.begin() + prefix_len
        );

        switch (child.kind)
        {
            case structural_child::kind::top_category:
                next.top(child.name);
                break;

            case structural_child::kind::sub_category:
                next.sub(child.name);
                break;

            case structural_child::kind::key:
                next.key(child.name);
                break;

            case structural_child::kind::table:
                next.table(table_id{child.ordinal});
                break;

            case structural_child::kind::row:
                next.row(row_id{child.ordinal});
                break;

            case structural_child::kind::column:
                next.column(child.name);
                break;

            case structural_child::kind::index:
                next.index(child.ordinal);
                break;
        }

        return next;
    }

    inline std::vector<prefix_match>
    inspected::prefix_children_matching(
        const inspect_context& ctx,
        std::string_view prefix
    ) const
    {
        std::vector<prefix_match> out;

        for (const auto& c : structural_children(ctx))
        {
            if (!prefix.empty())
            {
                if (c.name.empty())
                    continue;

                if (!c.name.starts_with(prefix))
                    continue;
            }

            out.push_back({
                c,
                extend_address(c)
            });
        }

        return out;
    }

    inline std::vector<address>
    inspected::suggest_next(inspect_context& ctx, std::string_view token) const
    {
        std::vector<address> out;

        for (auto& m : prefix_children_matching(ctx, token))
            out.push_back(m.extended);

        return out;
    }

// ------------------------------------------------------------
// resolve
// ------------------------------------------------------------

    const typed_value* resolve(
        inspect_context& ctx,
        address& addr
    )
    {
        auto res = inspect(ctx, addr);
        return res.value;
    }

} // namespace arf::reflect

#endif // ARF_REFLECT_HPP

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
//#define SHOW_STEP_CTOR

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
        #ifdef SHOW_STEP_CTOR        
            std::cout << "Constructing top_category_step with name = " << name << std::endl;
        #endif        
        }
        std::string_view name;
    };

    struct sub_category_step
    {
        explicit sub_category_step(std::string_view n) : name(n)  
        {
        #ifdef SHOW_STEP_CTOR        
            std::cout << "Constructing sub_category_step with name = " << name << std::endl;
        #endif            
        }
        std::string_view name;
    };

    struct key_step
    {
        explicit key_step(arf::key_id id) : id(id) 
        {
        #ifdef SHOW_STEP_CTOR        
            std::cout << "Constructing key_step from key_id with ID = " << id << std::endl;
        #endif            
        }
        explicit key_step(std::string_view n) : id(n) {
        #ifdef SHOW_STEP_CTOR        
            std::cout << "Constructing key_step from string with name = " << n << std::endl;
        #endif            
        }
        std::variant<key_id, std::string_view> id;
    };

    struct table_step
    {
        explicit table_step(table_id id) : id(id) 
        {
        #ifdef SHOW_STEP_CTOR        
            std::cout << "Constructing table_step from table_id with ID = " << id << std::endl;
        #endif            
        }
        explicit table_step(size_t loc_idx) : id(loc_idx) 
        {
        #ifdef SHOW_STEP_CTOR        
            std::cout << "Constructing table_step from size_t with local index = " << loc_idx << std::endl;
        #endif            
        }
        std::variant<table_id, size_t> id; // id or local ordinal
    };

    struct row_step
    {
        explicit row_step(table_row_id id) : id(id) 
        {
        #ifdef SHOW_STEP_CTOR        
            std::cout << "Constructing row_step from table_row_id with ID = " << id << std::endl;
        #endif            
        }
        table_row_id id;
    };

    struct column_step
    {
        explicit column_step(column_id id) : id(id) 
        {
        #ifdef SHOW_STEP_CTOR        
            std::cout << "Constructing column_step from column_id with ID = " << id << std::endl;
        #endif            
        }
        explicit column_step(std::string_view name) : id(name) 
        {
        #ifdef SHOW_STEP_CTOR        
            std::cout << "Constructing column_step from string with name = " << name << std::endl;
        #endif            
        }
        std::variant<column_id, std::string_view> id;
    };

    struct index_step
    {
        explicit index_step(size_t idx) : index(idx) 
        {
        #ifdef SHOW_STEP_CTOR        
            std::cout << "Constructing index_step from size_t with index = " << index << std::endl;
        #endif            
        }
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
    // address builder
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

        address& row(table_row_id id)
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

        std::optional<document::category_view> category;
        std::optional<document::table_view>    table;
        std::optional<document::table_row_view>      row;
        std::optional<document::column_view>   column;
        const typed_value*                     value = nullptr;
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

    inline bool is_array(value_type v)
    {
        return v == value_type::string_array
            || v == value_type::int_array
            || v == value_type::float_array;
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
      
    struct inspected
    {
        const address*     addr {nullptr};
        inspected_item     item;
        const typed_value* value;
        size_t             steps_inspected;

        bool fully_inspected() const { return addr && steps_inspected == addr->steps.size(); }
        bool ok()              const { return fully_inspected() && !addr->has_error(); }
        bool has_error()       const { return !fully_inspected(); }        

        size_t first_error_step() const
        {
            if (!addr) return steps_inspected;
            for (size_t i = 0; i < addr->steps.size(); ++i)
                if (addr->steps[i].diagnostic.state == step_state::error)
                    return i;
            return addr->steps.size();
        }        
    };    

// ------------------------------------------------------------
// inspect
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
        ctx.value = nullptr;

        inspected out;
        out.addr = &addr;
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
                        ctx.value = &k->value();
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

                else if (!is_array(ctx.value->type))
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
                if (ctx.value)         last_valid_item = ctx.value;
                else if (ctx.column)   last_valid_item = *ctx.column;
                else if (ctx.row)      last_valid_item = *ctx.row;
                else if (ctx.table)    last_valid_item = *ctx.table;
                else if (ctx.category) last_valid_item = *ctx.category;
            }                
        }

        //if (out.steps_inspected > 0)
            out.item = last_valid_item;

        if (out.ok())
        {
            if (auto pv = std::get_if<const typed_value*>(&out.item))
                out.value = *pv;
            else
                out.value = nullptr;
        }
        else
        {
            out.value = nullptr;
        }   

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

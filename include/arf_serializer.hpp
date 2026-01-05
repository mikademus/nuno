// arf_serializer.hpp - A Readable Format (Arf!) - Serializer
// Version 0.2.0
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.

#ifndef ARF_SERIALIZER_HPP
#define ARF_SERIALIZER_HPP

#include "arf_reflect.hpp"
#include <ostream>

namespace arf
{

//========================================================================
//SERIALIZER_OPTIONS
//========================================================================

    struct serializer_options
    {
        enum class auth_policy
        {
            preserve,
            discard
        } authorship = auth_policy::preserve;

        enum class type_policy
        {
            preserve,
            force_tacit,
            force_explicit
        } types = type_policy::force_tacit;

        bool canonical_order = false; // placeholder
        bool relint          = false; // placeholder
    };

//========================================================================
// SERIALIZER
//========================================================================

    class serializer
    {
    public:
        serializer(std::ostream& out, serializer_options opts = {})
            : out_(out), opts_(opts)
        {
        }

        void write(const document& doc)
        {
            for (const auto& [name, cat] : doc.categories)
            {
                if (name == detail::ROOT_CATEGORY_NAME)
                    write_category_body(*cat);
                else
                    write_top_level_category(*cat);
            }
        }

    private:
        std::ostream&      out_;
        serializer_options opts_;

    private:
        // ------------------------------------------------------------

        void write_top_level_category(const category& cat)
        {
            out_ << cat.name << ":\n";
            write_category_body(cat);
        }

        void write_category_body(const category& cat)
        {
            bool table_header_emitted = false;

            for (const decl_ref& decl : cat.source_order)
            {
                switch (decl.kind)
                {
                    case decl_kind::key:
                        write_key_value(cat, decl.name);
                        break;

                    case decl_kind::subcategory:
                        write_subcategory(cat, decl.name);
                        break;

                    case decl_kind::table_row:
                    {
                        const table_row& row = cat.table_rows[decl.row_index];

                        // Only emit rows that actually belong to this category
                        if (row.source_category != &cat)
                            break;

                        if (!table_header_emitted)
                        {
                            write_table_header(cat);
                            table_header_emitted = true;
                        }
                        write_table_row(cat, decl.row_index);
                        break;
                    }
                }
            }
        }

        // ------------------------------------------------------------

        void write_key_value(const category& cat, const std::string& key)
        {
            const auto it = cat.key_values.find(key);
            if (it == cat.key_values.end())
                return;

            const typed_value& tv = it->second;
            value_ref v(tv);

            out_ << key;

            if (tv.type_source == type_ascription::declared &&
                opts_.types != serializer_options::type_policy::force_tacit)
            {
                out_ << ":" << detail::type_to_string(tv.type);
            }

            out_ << " = ";
            write_value(v);
            out_ << '\n';
        }

        // ------------------------------------------------------------

        void write_subcategory(const category& parent, const std::string& name)
        {
            auto it = parent.subcategories.find(name);
            if (it == parent.subcategories.end())
                return;

            out_ << ":" << name << "\n";
            write_category_body(*it->second);
            out_ << "/" << name << "\n";
        }

        // ------------------------------------------------------------

        void write_table_header(const category& cat)
        {
            if (cat.table_columns.empty())
                return;

            out_ << "# ";

            bool first = true;
            for (const column& col : cat.table_columns)
            {
                if (!first)
                    out_ << "  ";

                out_ << col.name;

                if (col.type_source == type_ascription::declared &&
                    opts_.types != serializer_options::type_policy::force_tacit)
                {
                    out_ << ":" << detail::type_to_string(col.type);
                }

                first = false;
            }

            out_ << '\n';
        }

        // ------------------------------------------------------------

        void write_table_row(const category& cat, size_t row_index)
        {
            if (row_index >= cat.table_rows.size())
                return;

            const table_row& row = cat.table_rows[row_index];

            bool first = true;
            for (const typed_value& tv : row.cells)
            {
                if (!first)
                    out_ << "  ";

                write_value(value_ref(tv));
                first = false;
            }

            out_ << '\n';
        }

        // ------------------------------------------------------------

        void write_value(const value_ref& v)
        {
            if (auto src = v.source_str())
            {
                out_ << *src;
                return;
            }

            // Fallback: semantic emission only
            if (auto s = v.as_string())
            {
                out_ << *s;
                return;
            }
        }
    };

} // namespace arf

#endif

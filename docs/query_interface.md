# Querying Data in an Arf! Document

This document describes the **public query API** for accessing parsed Arf! data.
It covers scalar values, arrays, reflection, and structured table access.

Arf! deliberately separates **path-based queries** (for scalar data) from **structure-based APIs** (for arrays and tables). This prevents ambiguity and keeps queries deterministic.

---

## Parsing an Arf! Document

```cpp
#include "arf_parser.hpp"
```

Parsing operates on string data and produces a document tree:

```cpp
std::string example_config = obtain_arf_data_from_somewhere();

auto result = arf::parse(example_config);

if (result.has_value())
{
    arf::document& doc = *result.doc;
}

if (result.has_errors())
{
    // result.errors contains parse diagnostics
}
```

Parsing is non-throwing. All failures are reported explicitly.

---

## Note on Data Types

Arf! supports the following value types:

| Arf! name | Internal representation   |
| --------- | ------------------------- |
| `str`     | `std::string`             |
| `int`     | `int64_t`                 |
| `float`   | `double`                  |
| `bool`    | `bool`                    |
| `date`    | `std::string`             |
| ---       | ---                       |
| `str[]`   |                           |
| `int[]`   |                           |
| `float[]` |                           |

Array types are stored as `std::vector<T>` internally and exposed via **non-owning views**.

The names `int` and `float` are convenience annotations; no implicit narrowing occurs.

---

## Paths and Structured Queries

Dot-separated paths identify locations within the document hierarchy.
All query APIs take a path to locate the target node.

* Scalar accessors (`get_int`, `get_string`, etc.) only succeed if the path resolves to a scalar value.
* The `get_array` and `get_table` structural accessors succeed on array and table nodes respectively.

Attempting to access a non-scalar node with a scalar accessor or vice versa safely returns `std::nullopt`.

---

## Querying Key/Value Data

```cpp
#include "arf_query.hpp"
```

Key/value data is queried via **typed accessors**:

```cpp
auto volume = arf::get_float(doc, "game_settings.audio.master_volume");
```

Available accessors:

* `arf::get_string`
* `arf::get_int`
* `arf::get_float`
* `arf::get_bool`
( `arf::get_date` not available in 0.2.0)

Each returns `std::optional<T>`.

If the value exists but has a different type, a **safe conversion** is attempted.
If conversion fails, `std::nullopt` is returned.

Array accessors returns `std::optional<std::span<const T>>`:

| Array accessor            | span value type                |
| ------------------------- | ------------------------------ |
| `arf::get_string_array`   | `std::span<const std::string>` |
| `arf::get_int_array`      | `std::span<const int64_t>`     |
| `arf::get_float_array`    | `std::span<const double>`      |

---

## Reflection with `value_ref`

For tooling, inspection, or generic logic, Arf! provides a reflective interface:

```cpp
std::optional<value_ref> arf::get(const document& doc, const std::string& path);
```

`value_ref` is a lightweight, non-owning view onto a value in the document.

### Type inspection

```cpp
bool is_scalar() const
bool is_array() const

bool is_string() const
bool is_int() const
bool is_float() const
bool is_bool() const

bool is_string_array() const
bool is_int_array() const
bool is_float_array() const
```

### Value access

```cpp
std::optional<std::string> as_string() const
std::optional<int64_t>     as_int() const
std::optional<double>      as_float() const
std::optional<bool>        as_bool() const

template<typename T>
std::optional<std::span<const T>> as_array() const

const value& raw() const
```

Arrays are returned as `std::span<const T>` to avoid copying and preserve document ownership.

**Application code should prefer typed accessors** (`get_int`, `get_string`, …) whenever possible.

---

## Querying Arrays

Arrays are located by dot-paths but must be accessed using array-specific APIs (reflection or typed helpers).

### From key/value data

```cpp
auto tags = arf::get_array<std::string>(doc, "player.tags");

if (tags)
{
    for (const auto& tag : *tags)
        std::cout << tag << "\n";
}
```

`get_array<T>` returns:

```cpp
std::optional<std::span<const T>>
```

The span remains valid as long as the document exists.

### From reflection

```cpp
if (auto ref = arf::get(doc, "player.tags"))
{
    if (auto tags = ref->as_array<std::string>())
    {
        // use *tags
    }
}
```

---

## Querying Tables

Tables are located by dot-paths and accessed through the `table_view` API.

### Obtaining a table

```cpp
auto table = arf::get_table(doc, "creatures.monsters");
```

Returns:

```cpp
std::optional<table_view>
```

`table_view` is a lightweight, non-owning handle.

---

### Iterating rows

Tables can be divided into sections by specifying subcategories. The basic `rows()` method provides access to all rows in the current category only:

```cpp
for (size_t i = 0; i < table->rows().size(); ++i)
{
    auto row = table->row(i);
}
```

Each row is represented by a `row_view`.

---

Table rows can be exhaustingly iterated over either in document order as rows occur in the document as authored, or in canonical order in a breadth-first search recursion:

```cpp
for (auto it : table->rows_document())
    row_view row = *it;    

for (auto it : table->rows_recursive())
    row_view row = *it;    
```

---

### Accessing row values

Row access is **name-based** and type-safe:

```cpp
auto region  = row.get_string("region");
auto port    = row.get_int("port");
auto active  = row.get_bool("active");
```

Each accessor returns `std::optional<T>`.

If the column does not exist, is out of range, or has the wrong type, the result is `std::nullopt`.

---

### Arrays in table rows

Array-valued columns can be accessed as spans:

```cpp
auto skills = row.get_array<std::string>("skills");

if (skills)
{
    for (const auto& s : *skills)
        std::cout << s << "\n";
}
```

No copying occurs.

---

### Subcategories in tables

Table subcategories participate in the same table structure.

The table view presents rows in **document order**, including rows originating from subcategories.

Clients do not need to manually traverse subcategories.

---

## Design Notes

* Arrays are exposed as spans, never copied by default
* All query APIs are non-throwing
* Document ownership is preserved at all times

Arf!’s query API is designed to be predictable, explicit, and difficult to misuse by accident.

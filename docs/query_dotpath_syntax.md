# Dot‑path syntax (Arf! query 0.3.0)

Dot‑paths are *addresses*, not a second query language. They navigate document structure in document order and may explicitly switch into table axes (tables, rows, columns) using specialised selectors.

---

## 1. Structural navigation

Dot (`.`) separates path segments and descends structurally.

```
foo.bar.baz
```

Selects `baz` under `bar` under `foo`, in document order.

---

## 2. Array indexing

Array elements are selected explicitly using a bracketed index segment. 

```
foo.bar.[2]
```
**Note** that the bracets are a segment and must be preceded by a dot to reinforce the narrowing-step invariant (e.g. correct: foo.[2]; incorrect: foo[2]).

The brackets distinguish positional access from name-based selection,
preventing ambiguity with keys that have numeric names.

Selects the third element of the array value at `foo.bar`.

The index is **0-based**.

Array indexing is only valid when the current value is an array. Using an index segment on a non-array value is an error.

This explicit form avoids ambiguity with other pluralities and keeps positional access type-specific.

**Arf! arrays are single-dimensional only.** Chaining index operations
(e.g., `arr.[0].[1]`) is never meaningful and will fail at the second
index attempt.

---

## 3. Table selection

Tables are selected explicitly using `#` segments.

### Ordinal table selection

```
foo.#2
```

Selects the third table under `foo`.

### Table wildcard

```
foo.#
```

Selects **all** tables under `foo`.

The order is always document order.

---

## 4. Axis selectors (rows and columns)

Rows and columns are separate axes inside a table and are selected using *symmetrically enclosed* segments.

Axis switches are explicit and never implicit.

### Row selector

```
-row-
```

Selects the row named `row`.

### Column selector

```
|col|
```

Selects the column named `col`.

### Trimming rule

Exactly **one** enclosing character on each side is stripped to obtain the semantic name.

This allows names that themselves contain axis glyphs:

```
--orcs--   -> row name "-orcs-"
||hp||     -> column name "|hp|"
```

A segment is a row or column selector **iff** it begins and ends with the same axis glyph (`-` for rows, `|` for columns).

### Edge Cases

Valid but unusual:
- `---` selects row named "-"
- `||||` selects column named "||"
Valid but meaningless:
- `--` tries to select a row with empty name (empty cells never exists)

While syntactically valid, using delimiters is discouraged for readability.
---

## 5. Cell addressing

Row and column selectors may be chained to address cells.

```
#.-row-.|col|
```

Meaning:

* `#`      : all tables in scope
* `-row-`  : select row `row`
* `|col|`  : select column `col`

Row and column selectors may appear in any order. Their meaning is determined by axis, not by position, and no implicit precedence or ordering rules apply.

---

## 6. General rules

* Dot-paths are deterministic and document-ordered.
* Dot (`.`) always represents a semantic narrowing step.
* Positional access is always explicit and type-specific:

  * `#n` for tables
  * `[n]` for arrays
* Axis switches must be explicit; dots never change axis.
* No implicit row, column, or array ordering exists.

Anything involving filtering, predicates, or computation belongs to the flowing query API, not the dot-path.

## Scope: What Belongs in Dot-Paths

Dot-paths are for **structural and positional navigation only**.

**In scope:**
- Named categories: `world.countries`
- Named keys: `settings.volume`
- Table ordinals: `data.#2`
- Row/column names: `#0.-france-.|population|`
- Array indices: `equipment.[1]`

**Out of scope** (use fluent API):
- Filtering: `.where(eq("hp", 100))`
- Predicates: `.where(gt("level", 5))`
- Projections: `.project("name", "hp")`
- Aggregations: `.count()`, `.sum()`

This separation keeps dot-paths **deterministic and structural** while
enabling flexible computation through methods.

## Quick Reference

| Syntax | Meaning | Example |
|--------|---------|---------|
| `.` | Structural descent | `world.settings.video` |
| `#n` | Nth table (0-based) | `items.#2` |
| `-name-` | Row by name | `#0.-orcs-` |
| `|name|` | Column by name | `#0.|hp|` |
| `[n]` | Array element (0-based) | `equipment.[2]` |
| `-name-.|col|` | Cell (row + column) | `#0.-npc1-.|hp|` |

**Rules:**
- Dot (`.`) always represents semantic narrowing
- Axis switches must be explicit (no implicit row/column ordering)
- Array indexing requires brackets to distinguish from name-based selection
- No implicit coercion or type inference
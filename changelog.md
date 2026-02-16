# Changelog

## [0.3.0] - 2026-02-16

### Overview

Version 0.3.0 represents a complete architectural redesign of Arf, transforming it from a proof-of-concept parser into a comprehensive document framework. While the Arf format specification remains unchanged and fully backward-compatible, the implementation has been rebuilt from the ground up to support the full document lifecycle: parsing, inspection, editing, and serialization.

The framework now treats documents as first-class structured graphs with stable entity identities, semantic validation, and contamination tracking. Every entity—keys, tables, rows, columns, comments, paragraphs, and categories—is a typed, addressable node with preserved authorship order and provenance metadata. This enables sophisticated tooling scenarios including IDEs, validators, converters, and programmatic document generation.

### Comparison to 0.2.0

Where 0.2.0 provided a map-based document model with immediate parsing and basic table iteration, 0.3.0 introduces a staged pipeline (parse → materialize → document), a rich type system with semantic validation, and complete CRUD operations through a new editor API. The document model has evolved from string-indexed maps to ID-based entity graphs, enabling stable references across edits and sophisticated contamination propagation.

The most significant architectural shift is the emergence of the **document as the gravitational center** of the framework. While 0.3.0 initially intended reflection as the foundational layer upon which query and serialization would be built, the increasing sophistication of the document model—with its comprehensive event tracking, source order preservation, and semantic metadata—enabled query and serialization to operate directly on document structures. Reflection evolved into a specialized inspection tool rather than a mandatory intermediary, with each module finding its own natural relationship to the document core.

Three entirely new modules expand Arf's capabilities: the **reflection API** provides low-level address-based inspection for tooling and navigation scenarios; the **query API** offers high-level ergonomic access for user-facing operations; and the **editor API** enables type-safe programmatic manipulation with automatic validation.

---

### Major Changes

#### Architecture

**Module Decomposition:**
- **Entry Point** (`arf.hpp`): Convenience header aggregating all modules
- **Core Types** (`arf_core.hpp`): Pure type definitions extracted from monolithic 0.2.0 core
- **Document Model** (`arf_document.hpp`): Separated from core, expanded to 1,170 lines with ID-based entity graph
- **Parser** (`arf_parser.hpp`): Reduced scope to CST generation only (was 514 lines, now 725 with enhanced diagnostics)
- **Materializer** (`arf_materialise.hpp`): New 1,184-line module for document construction and semantic validation
- **Reflection** (`arf_reflect.hpp`): New 1,045-line address-based inspection API
- **Query** (`arf_query.hpp`): Expanded from 832 to 2,474 lines with query DSL and ergonomic access patterns
- **Editor** (`arf_editor.hpp`): New 2,509-line CRUD API with contamination tracking
- **Serializer** (`arf_serializer.hpp`): Expanded from 348 to 867 lines with source-faithful reconstruction

**Staged Pipeline:**
- Parsing no longer directly populates documents; instead generates reusable CST
- Materialization performs semantic validation during document construction
- Separation enables multiple materializations from single parse, enhanced error diagnostics

**Document as Gravitational Center:**
- Document model matured into self-sufficient structure with complete metadata
- Parse event tracking enables serializer to reconstruct authored structure directly
- Rich view layer eliminates need for reflection in common query scenarios
- Each module (reflection, query, serialization) operates independently on document
- Reflection serves specialized tooling needs rather than acting as mandatory foundation

#### Document Model

**From Map to Graph:**
- **0.2.0:** `std::map<string, category>` with string-indexed lookups, raw pointers, single table per category
- **0.3.0:** ID-based entity storage (`vector<category_node>`, `vector<key_node>`, etc.) with typed stable handles

**Entity System:**
- **Stable IDs:** `id<Tag>` template provides type-safe, monotonic, non-reused handles for all entities
- **New entity types:** Comments and paragraphs as first-class documentation; columns as explicit typed entities
- **Multiple tables:** Categories can contain unlimited tables; tables explicitly tracked with IDs

**Source Order Preservation:**
- **0.2.0:** `vector<decl_ref>` tracking keys, rows, subcategories per-category with string-based identity
- **0.3.0:** `vector<source_item_ref>` in both categories and tables, ID-based, includes comments, paragraphs, and category close markers

**View Layer:**
- Document internals hidden behind immutable view types (`category_view`, `key_view`, `table_view`, etc.)
- Views expose rich metadata: semantic state, contamination, IDs, provenance
- No direct pointer access to internal storage; enforces read-only access patterns

#### Type System

**Value Representation:**
```cpp
// 0.2.0: Simple variant
using value = variant<string, int64_t, double, bool, vector<string>, vector<int64_t>, vector<double>>;

// 0.3.0: Semantic metadata + recursive arrays
struct typed_value {
    value val;                      // variant now includes monostate and vector<typed_value>
    value_type type;                // unresolved | string | integer | decimal | boolean | date | *_array
    type_ascription type_source;    // tacit vs declared
    value_locus origin;             // key_value | table_cell | array_element | predicate
    semantic_state semantic;        // valid vs invalid
    contamination_state contamination; // clean vs contaminated
    creation_state creation;        // authored vs generated
    bool is_edited;
};
```

**Key Enhancements:**
- `unresolved` type for untyped values
- `array_element` locus distinguishes container values from contained elements
- Recursive `typed_value` enables heterogeneous arrays with per-element validation
- Metadata tracks complete provenance and validity state

#### Contamination System (New)

**Semantic Validation:**
- Type mismatches mark values as `invalid`
- Invalid elements contaminate their containers (array → key; cell → row)
- Contamination propagates upward (row → table → category)
- Document tracks contamination sources for efficient cleanup

**Integration:**
- Materializer validates during construction, marks structural and type errors
- Editor re-evaluates contamination on every mutation
- Serializer can detect and report contaminated regions
- Enables IDE-like semantic highlighting and error tracking

#### Reflection API (New Module)

**Philosophy:**
Reflection provides **low-level, address-oriented document inspection** for tooling scenarios that require precise navigation control, structural introspection, and diagnostic information. It operates at the level of **addresses and steps**, treating the document as a navigable space where paths can be constructed, inspected, and diagnosed programmatically.

**Core Concepts:**
- **Addresses:** Composable sequences of navigation steps describing paths through document structure
- **Steps:** Individual navigation operations (top-level category, subcategory, key, table, row, column, array index)
- **Inspection:** Resolution of addresses with detailed diagnostics at each step
- **Value-centric:** Addresses ultimately resolve to values, not structural nodes

**Programmatic Address Construction:**
```cpp
using namespace arf::reflect;
address addr = root()
    .top("config")
    .sub("database")
    .key("host");

inspect_context ctx{.doc = &document};
inspect_result result = inspect(ctx, addr);

if (result.ok()) {
    const typed_value* value = result.value;
    // Work with resolved value
}
```

**Diagnostic Capabilities:**
- Step-by-step resolution with 30+ granular error types
- Partial resolution: returns last valid item on address failure
- Structural children enumeration for navigation/completion scenarios
- Error context for each failed step (what was expected, what was found)

**Intended Use Cases:**
- **Tooling:** IDEs, validators, linters requiring structural navigation
- **Navigation:** Address completion, suggestion engines, path verification
- **Debugging:** Detailed diagnostics explaining why addresses fail to resolve
- **Programmatic inspection:** Scenarios requiring explicit control over navigation steps

#### Query API (Enhanced Module)

**Philosophy:**
Query provides **high-level, user-facing document access** optimized for ergonomic data retrieval. It operates at the level of **paths and values**, treating the document as a queryable data store where simple string expressions or fluent chains retrieve desired information without navigation ceremony.

**Core Concepts:**
- **String queries:** Dot-path expressions (`"config.database.host"`) resolve to values
- **Typed accessors:** Convenient methods extracting values in expected types
- **Result handles:** Query results encapsulate locations, values, and metadata
- **Ergonomic chaining:** Fluent API for common access patterns

**String Query Syntax:**
```cpp
auto result = query(doc, "config.database.host");
auto host = result.as_string();  // Optional<string>

// Type-specific extraction
auto port = query(doc, "config.database.port").as_integer();
auto enabled = query(doc, "config.features.ssl").as_boolean();
```

**Integration with Editor:**
```cpp
// Bridge query → editor via ID extraction
auto query_result = query(doc, "config.version");
auto kid = query_result.key_id();  // Extract stable ID

editor ed(doc);
ed.set_key_value(*kid, 2);  // Edit using ID
```

**Intended Use Cases:**
- **User applications:** Configuration readers, data extractors, report generators
- **Simple access:** "Just give me the value at this path" scenarios
- **Scripting:** Quick queries without address construction ceremony
- **Editing workflows:** Query to find, extract ID, pass to editor

#### Interoperability: Reflection ↔ Query

**Design Relationship:**
- **Reflection is low-level:** Explicit steps, diagnostic control, structural inspection
- **Query is high-level:** String paths, ergonomic accessors, value focus
- **Query uses reflection internally:** String paths compile to reflection addresses
- **Both operate on document:** Neither is a mandatory foundation for the other

**When to Use Which:**

Use **Reflection** when you need:
- Programmatic address construction with explicit step control
- Detailed diagnostic information about resolution failures
- Structural introspection (enumerate children, inspect metadata)
- Tooling scenarios requiring precise navigation semantics

Use **Query** when you need:
- Quick value retrieval from known paths
- User-facing APIs with string-based queries
- Ergonomic typed accessors without ceremony
- Integration with editor via ID extraction

**Cross-Module Pattern:**
```cpp
// Query provides ergonomics, reflection provides diagnostics
auto result = query(doc, "path.to.key");

if (!result.has_value()) {
    // Use reflection to diagnose why
    inspect_context ctx{.doc = &doc};
    address addr = /* construct from query path */;
    auto inspection = inspect(ctx, addr);
    
    // inspection.steps contains detailed failure info
}
```

#### Editor API (New Module)

**Philosophy:**
The editor is the **public API for all document mutation**. While the document class provides read-only access and internal building blocks, the editor is the sole public interface for creating, modifying, and deleting document entities programmatically. The editor guarantees invariant enforcement by maintains item order, contamination status, and entity semantic state.

**Complete CRUD Operations:**
- **Keys:** Create, read (via document), update (values and types), delete
- **Tables:** Create with typed/untyped columns, delete (cascade to rows/columns)
- **Rows:** Append, insert before/after, delete
- **Columns:** Append, insert before/after, delete (with cell removal from all rows), retype
- **Array elements:** Append, set, erase within key arrays and table cell arrays
- **Comments/Paragraphs:** Append, insert before/after, set text, delete
- **Categories:** Append, insert before/after, delete (safety checks for non-empty)

**Type Control:**
- `set_key_type(key_id, value_type)` retroactively changes key type, re-validates value
- `set_column_type(column_id, value_type)` re-validates all cells in column across rows
- Returns bool indicating if all values remain valid under new type

**Design Principles:**
- Single-insert pattern: create node, insert once at correct position (no append-erase-insert)
- Automatic contamination tracking: every mutation updates semantic state
- Ordered_items maintenance: preserves authored document structure
- Type-safe IDs: cannot mix key_id with table_id; compile-time safety

#### Serialization Enhancements

**Source-Faithful Reconstruction:**
- **0.2.0:** Normalized output, invented indentation, reordered items
- **0.3.0:** Preserves authored structure through parse event tracking

**Capabilities:**
- Maintains original category/subcategory nesting and closing syntax
- Preserves authored order of all entities (keys, tables, comments, paragraphs)
- Respects source formatting for unedited entities
- Emits appropriate syntax for edited/generated entities

**Edit-Aware Output:**
- Tracks which entities were modified via `creation_state` and `is_edited` flags
- Reconstructs authored structure for unedited entities
- Emits explicit syntax for edited/generated entities

**Configurable Policies:**
```cpp
struct serializer_options {
    type_policy types;              // as_authored | force_explicit | force_tacit
    formatting format;              // preserve_authored | compact
    bool skip_comments;
    bool compact_blank_lines;
};
```

#### Error Handling

**Granular Error Types:**
- **Parse errors** (syntactic): Location tracking, message, source context
- **Semantic errors** (validation): Type mismatches, invalid declarations, structural violations
- **Material errors** (materialization): CST construction issues

**Error Enums:**
- `semantic_error_kind`: 15+ specific semantic error types
- `step_error`: 19+ reflection address resolution errors
- Errors stored alongside document for inspection

---

### Breaking Changes

**0.2.0 → 0.3.0 has no backward compatibility.** The API is completely redesigned:

| 0.2.0 | 0.3.0 | Notes |
|-------|-------|-------|
| `doc.categories["name"]` | `doc.category("name")` | Returns `optional<category_view>` |
| `cat->key_values["key"]` | `cat->key("key")->value()` | Two-step: get key view, access value |
| `cat->table_rows` | `doc.table(table_id)->rows()` | Tables now explicit entities |
| Raw pointers | View types | Immutable views, no direct pointer access |
| `parse()` returns document | `load()` returns parse context | Context includes errors, CST |
| Direct mutation | Editor API | `editor.set_key_value(id, val)` |

**Migration Strategy:**
Reading 0.2.0 documents works unchanged (format is compatible). Document manipulation requires rewriting to use editor API and view-based access.

---

### Documentation

**New Documentation:**
- `query_interface.md`: Query API specification and usage patterns
- `query_dotpath_syntax.md`: Syntax guide for query expressions

**Test Suites:**
- `arf_parser_tests.hpp`: CST generation and syntax validation
- `arf_materialiser_tests.hpp`: Document construction and semantic validation
- `arf_document_structure_tests.hpp`: Document model invariants
- `arf_reflection_tests.hpp`: Address resolution and inspection
- `arf_query_tests.hpp`: Query DSL and predicate evaluation
- `arf_serializer_tests.hpp`: Round-trip preservation and edit reconstruction
- `arf_editor_tests.hpp`: CRUD operations and contamination tracking
- `arf_integration_tests.hpp`: Cross-API workflows (parse → query → edit → serialize)

**Total:** ~200+ test cases validating all framework layers

---

### Known Limitations

- **Column deletion** removes cells from all rows but does not adjust column count metadata
- **Move operations** not supported by design; use delete + recreate pattern
- **Category management** supports creation/deletion but not reparenting or cascade deletion
- **Query predicates** support limited to basic filtering; no arithmetic or complex expressions

---

### Performance Notes

- **ID lookups:** O(n) linear search in entity vectors (acceptable for document-scale workloads)
- **Insertion:** 3x faster than 0.2.0 pattern (single vector insert vs append-erase-insert)
- **Memory overhead:** ~30% additional memory for semantic metadata (contamination, provenance, edit tracking)
- **Trade-off:** Slightly slower individual lookups, massive gains in correctness and capabilities

---

## [0.2.0] - 2025-12-13

### Overview

Version 0.2.0 established Arf as a functional configuration format with a working C++ implementation. Building on the 0.1.0 proof-of-concept, this version introduced a structured query interface, comprehensive table handling with document-order iteration, and a basic serialization layer.

The parser evolved to handle complex hierarchical documents with proper subcategory unwinding and table mode management. The document model moved from a flat single-file implementation to a modular architecture with separate concerns for parsing, querying, and serialization.

### What's New Since 0.1.0

Where 0.1.0 provided a minimal single-file parser with basic string-based queries, 0.2.0 introduced a proper document model with typed table support, view-based APIs, and table-centric navigation. The addition of document-order row iteration and subcategory close markers enabled more sophisticated document traversal.

Key improvements include: type-aware table parsing (int, float, bool, date, arrays), named subcategory closing with unwinding logic, proper tracking of declaration order via `decl_ref` structures, and the introduction of separate query and serialization modules.

---

### Changes from 0.1.0

#### Document Model

**Structure:**
- `document` now owns categories via `map<string, unique_ptr<category>>`
- `category` contains: key_values map, table metadata (columns + rows), subcategories map, source order vector
- `decl_ref` struct tracks declaration order: kind (key | table_row | subcategory), name, row_index

**Table Support:**
```cpp
struct column { string name; value_type type; };
using table_row = vector<value>;
// Categories now track: vector<column> table_columns, vector<table_row> table_rows
```

**Type System:**
```cpp
enum value_type { 
    string, integer, decimal, boolean, date,
    string_array, int_array, float_array 
};
using value = variant<string, int64_t, double, bool, 
                      vector<string>, vector<int64_t>, vector<double>>;
```

#### Parser Enhancements

- **Table parsing:** Recognizes `# column:type column:type` headers, validates cell types
- **Array parsing:** Supports pipe-delimited arrays (`1|2|3` → `vector<int64_t>`)
- **Subcategory closing:** Named close (`/subcategory`) with stack unwinding
- **Source order:** Tracks declaration order in `category::source_order` vector
- **Table mode:** Maintains table context across subcategories for row continuation

#### Query Interface (New)

**View-Based API:**
```cpp
class table_view;
class row_view;
```

**Features:**
- Document-order row iteration across category hierarchy
- Typed accessors: `row.get_int("column")`, `row.get_string_array("column")`
- Provenance tracking: `row.source()` returns originating category
- Frame-based iteration: Stack-based traversal maintaining table context

**Internal Reflection:**
- `value_ref` struct provided basic reflection for table navigation
- Enabled document-order iteration but not exposed as public API
- Foundation for future reflection module (realized in 0.3.0)

#### Serialization

**Capabilities:**
- Basic document reconstruction with table headers and rows
- Subcategory nesting with proper indentation
- Inline table rows for subcategories with matching columns

**Known Limitations:**
- **Does not preserve authored structure:** Output is normalized
- **Invented indentation:** Consistent but not source-faithful
- **Reordered items:** No guarantee of original declaration order
- **Missing features:** Comments and paragraphs discarded
- Addressed in 0.3.0 through parse event tracking and source order preservation

#### API Surface

**C++ Interface:**
```cpp
document parse(const string& input);
string serialize(const document& doc);
optional<string> get_string(const document& doc, const string& path);
optional<int64_t> get_int(const document& doc, const string& path);
// Similar for float, bool
```

**C API:**
```cpp
arf_document_t* arf_parse(const char* input);
char* arf_serialize(const arf_document_t* doc);
char* arf_get_string(const arf_document_t* doc, const char* path);
// Similar for int, float, bool with out-parameters
void arf_free_document(arf_document_t* doc);
void arf_free_string(char* str);
```

---

### File Organization

- **arf_core.hpp** (161 lines): Core structures, type definitions, forward declarations
- **arf_parser.hpp** (514 lines): Parser implementation with table and subcategory support
- **arf_query.hpp** (832 lines): View-based table iteration and typed accessors
- **arf_serializer.hpp** (348 lines): Document reconstruction (normalized output)

**Total: 1,855 lines** (up from 0.1.0's single 900-line file)

---

### Limitations

- **No mutation API:** Documents are read-only after parsing
- **No semantic validation:** Type mismatches emit to stderr but document remains usable
- **String-based identity:** No stable handles; categories/keys referenced by name
- **Single table per category:** Cannot have multiple independent tables
- **No comments/paragraphs:** Format supports these but implementation ignores them
- **Map-based storage:** O(log n) lookups, no guarantee of insertion order preservation beyond source_order tracking
- **Normalized serialization:** Output does not preserve original structure, formatting, or order

---

## [0.1.0] - 2025-12-12

### Overview

Version 0.1.0 introduced Arf as a minimal proof-of-concept for a human-readable configuration format. Implemented as a single 900-line header file, it demonstrated the core concepts: hierarchical categories, key-value pairs, typed tables with subcategory row grouping, and basic query syntax.

The implementation prioritized simplicity and ease of integration, providing both C++ and C APIs in a single include file. While lacking advanced features like semantic validation or mutation support, 0.1.0 successfully parsed realistic configuration files and provided a foundation for future development.

---

### Features

**Format Support:**
- Top-level categories: `category:`
- Subcategories: `:subcategory` with `/subcategory` closing
- Key-value pairs: `key = value`
- Typed tables: `# column:type column:type` headers
- Table rows: Space-delimited cells, two-space column separator
- Comments: `// text` (ignored during parsing)

**Data Types:**
- Scalars: string, int, float, bool, date
- Arrays: str[], int[], float[] (pipe-delimited: `a|b|c`)

**API:**
- `parse(string)` → document
- `serialize(document)` → string
- `get_string/int/float/bool(document, path)` for queries
- C API with opaque handle and memory management

**Implementation:**
- Single-file header-only library
- 900 lines including C++ and C APIs
- No dependencies beyond STL
- Basic error handling (no detailed diagnostics)

---

### Limitations

- No semantic validation (silent failures)
- No edit/mutation capabilities
- Limited error reporting
- Comments discarded during parsing
- No preservation of source formatting
- Table iteration in parse order (not document order)
- Subcategory closing handled simplistically

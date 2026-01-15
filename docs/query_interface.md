# Query API
Version 0.3.0

## Query interface principles

The Arf! query API is a human-facing, read-oriented interface for retrieving values from a document. It is intentionally distinct from reflection.

* The query API is optimised for human ergonomics and clarity. 
* Tooling, mutation, and structural introspection belong to reflection.

### Core principles:
1. **Queries observe, never mutate.**
   Querying does not alter document structure or values.
2. **Queries are selections, not navigation.**
   Path expressions and fluent operations progressively narrow a result set.
   They do not encode absolute addresses or structural identity.
3. **Paths primarily express structure**; value-based selection is explicit except for constrained identifier shorthands.
   Identifier-like value selection is permitted in constrained collection contexts.
4. **Document order is preserved** unless explicitly refined.
   The relative authored order is preserved even in query results.
5. **Plurality is the default.**
   Queries may yield zero, one, or many results.
6. **Ambiguity is reported, not fatal.**
   Ambiguous queries return all matching results and diagnostics.
7. **Values may act as identifiers** at query time,
   but never become structural names.
8. **Evaluation is implicit.**
   A query is executed when results are accessed.
   There is no explicit "eval" or execution step.
9. **Query results are transparent and present.**
   Current results (the working set) can always be examined.
   (Again, there is no explicit execution step.)

## Details

### Document order is always preserved

All query results preserve the authored order from the source document.
This is an invariant across all operations.
```cpp
    auto results = query(doc, "races").strings();
    // results[0] is the first "races" element in the document
    // results[1] is the second "races" element in the document
```       
This guarantee holds for:
  - Multi-result queries
  - Predicate filtering (where clauses)
  - Structural selection (select clauses)
  - Ambiguous matches

Order is NEVER altered by:
  - Query evaluation strategy
  - Internal optimization
  - Result caching

The author's intent is preserved.

### Query usage model

A query is a progressive selection pipeline over a document.
Each link in the chain narrows or refines a working result set.

#### 1. Dot-paths (structural and contextual selection)

Dot-paths describe structural descent and contextual selection.
Any dot-path accepted by query() is also valid in select().

```cpp
     query(doc, "world.races");
     query(doc).select("world.races");
     query(doc, "world").select("races");
```     
These forms are equivalent.

#### 1.1. The working set

Internally, a query operates over a working set.
Conceptually, the working set is a set of value locations, not values themselves.

Each element in the working set refers to:
 - a concrete value in the document
 - together with its structural context (category, table, row, key, or array position)

This allows queries to:
 - preserve authored order
 - report ambiguity without collapsing results
 - extract values in multiple representations (singular or plural)

The working set is not exposed directly, but all result extraction methods (`as_*`,  `*_s`) operate over it.

#### 1.2. Dot-path semantics: Progressive filtering, not navigation

A dot-path is NOT a navigation through nodes or addresses.
It is a PROGRESSIVE FILTER that narrows a working result set.

Each segment applies a selection predicate:
```cpp
    "world.races.orcs"
```

Conceptual execution:
    1. Start with entire document as working set
       **Note** that the initial working set *conceptually* represents “all observable values in the document”, not a concrete collection. Each selection step progressively constrains what can be observed.
    2. Filter to elements matching "world" → working set = {world values or value-bearing structures}
    3. Filter to elements matching "races" → working set = {race values or value-bearing structures}
    4. Filter to elements matching "orcs"  → working set = {orc values or value-bearing structures}

This is analogous to:
  - SQL: Successive WHERE clauses on derived tables
  - Functional: Composition of filter/map operations
  - Optics: Progressive lens focusing

It is NOT analogous to:
  - Filesystem paths: /world/races/orcs
  - Object navigation: doc.world.races.orcs
  - Address traversal: root().top("world").sub("races")

The distinction matters:
  - Navigation implies unique paths and structural identity
  - Filtering implies set operations and plurality

Query embraces plurality and set-based selection.
Reflection provides precise structural addressing.

#### 2. Flowing selection chains

Queries are built as a flowing chain of selection links:
```cpp
    query(doc, "world.races")
        .where("race", "orcs")
        .select("poise");
```        
Each link refines the current result set.
There is no implicit backtracking or guessing.

#### 3. Mixed selection styles

Structural selection (dot-paths) and predicate selection can be mixed:
```cpp
    query(doc)
        .select("world")
        .select("races")
        .where("race", "orcs")
        .select("population");
```        
Dot-paths may appear both at query construction and mid-chain.

#### 4. Value-based selection in dot-paths

In collection contexts (such as tables), dot-paths may include
value-based selectors as a shorthand for identifier lookup.
```cpp
    "world.races.orcs"
    "world.races.orcs.poise"
```
Here, "orcs" acts as a query-time identifier selecting rows whose
leading identifier column matches "orcs".

Value-based selectors:
* are only valid in table contexts
* only applies to leading identifier columns
* is equivalent to ```where(<leading_column>, eq(<value>))```

**This does not promote values to structural names.**
It is equivalent to an equality predicate, a constrained, contextual
shorthand for equality selection.

Consider the common use case of tables with leading columns in string format
(as in the fantasy races query, above). It is desirable and useful to be able 
to select directly on the leading column as an identifier. 

If a value-based selector is used outside a valid table context, or if the leading column does not exist or is not comparable, the selector:
 - yields no matches
 - produces a diagnostic
 - does not invalidate the query

#### 5. Multiple access paths to the same data

The same value can often be reached in more than one way:
```cpp
     query(doc, "world.races.orcs.poise");

     query(doc, "world.races")
         .where("race", "orcs")
         .select("poise");

     get_string(doc, "world.races.orcs.poise");
```
All are valid and equivalent in intent.
The API optimises for human convenience and readability.

#### 6. Plurality is always possible

Queries may yield zero, one, or many results.
Plurality is the default and never an error.
```cpp
     auto res = query(doc, "world.races.orcs");
```
Document order is preserved unless explicitly refined.

#### 7. Predicate model

// Predicates are explicit value constraints.

```cpp
     where("population", gt("1000"))
     where("name", ne("orc"))
```

Equality has a shorthand:
```cpp
     where("race", "orcs")
```
Predicates refine the current result set and are fully chainable.

#### 8. Implicit execution

Queries execute implicitly when results are accessed. There is no explicit evaluation step.

Calls to `query()` and all chained selection operations construct a lazy query handle.  
No traversal or filtering occurs until a result extraction method is invoked.

```cpp
auto res = query(doc, "world.foo");
auto v   = res.as_integer(); // triggers evaluation
```

Evaluation is cached per handle:
 - Repeated extractions do not re-evaluate the query.
 - Different extraction forms (as_string, strings, etc.) operate on the same cached working set.
 - Querying observes the document; it never mutates it.

### Query syntax

Basic syntax:
```cpp
     query(document, initial-dot-path).flowing_predicates()... ;
```     
Dot-paths and flowing predicates perform the same narrowing operations.

```cpp
     auto ctx = load(arf_string);
     auto res = query(ctx.document, "servers.europe.poland.ip");
     auto opt_int = res.as_integer();
```
Any query is a sequence of narrowing filters on the document. Each element of the narrowing operation is a predicate that narrows the document either by narrowing the lens to a part of the script's physical structure or to a subset of data. 

A note on the singular convenience extraction methods (as_string, as_integer, etc.).

Singular extraction methods:
 - return std::nullopt if the working set is empty
 - return std::nullopt if the working set contains more than one element
 - produce diagnostics in the case of ambiguity or type mismatch

They never throw and never select arbitrarily.

#### 1. Entry points

The primary entry points to the query API are:

```cpp
query(const document& doc)
query(const document& doc, std::string_view dot_path)
```
The first form creates a query handle over the entire document.
The second form applies an initial dot-path selection as the first refinement step.

Both forms are equivalent to beginning a query chain with `select(dot_path)`.

#### 1. Dot-path syntax

Basic dot-path syntax:

```cpp
     "foo.bar.baz"
```
Dot-paths are split on the `.` character.
Tables are selected through the `#` character.

Rules:
    1. `.` and `#` are reserved characters in dot-paths. Identifiers containing periods or hashes cannot be selected via dot-paths and must instead be accessed using flowing predicates.  
    2. Tables are nameless and selected by ordinal position in their nearest category context. Dot-paths support the shorthand `#n`, meaning the "nth table" (zero-based).
```cpp
     "settings.#2.users"
```       
    3. Array indices can be accessed by their ordinal number when following an array value.
```cpp
     "equipment.potions.2"
``` 
    4. Ordinal selection is contextual:
`#n` and array indices are only valid where a table or array selection is meaningful.
If no such table or array exists, the step yields an empty refinement and a diagnostic.

Dot-path steps never throw and never abort query evaluation.
A dot-path step that does not match any elements:
 - produces no matches
 - records a diagnostic
 - does not abort evaluation

Subsequent steps operate on an empty working set.

#### 2. Flowing predicates

Flowing predicates are explicit refinement steps applied to the current working set.

They take the form:

```cpp
.where(column_name, predicate)
```
or, for equality:

```cpp
.where(column_name, value)
```

Flowing predicates:
 - Always refine the current working set
 - Never introduce new elements
 - Preserve document order
 - Are fully chainable

Example:

```cpp
query(doc, "world.races")
    .where("population", gt("1000"))
    .where("alignment", ne("chaotic"))
    .select("poise");
```
Predicates that cannot be applied in the current context produce diagnostics and yield no matches, but do not invalidate the query handle.

#### 2.1. Predicates are contextual:

 - In table contexts, predicates apply to table rows via columns.
 - In key/value contexts, predicates apply directly to values.

Equality shorthand
Equality has a shorthand form:

```cpp
where("race", "orcs")
```
This is equivalent to:

```cpp
where("race", eq("orcs"))
```

#### 2.2. Type interpretation

Predicate values are interpreted lazily and contextually:
 - No global type coercion is performed at query construction time.
 - Comparison semantics depend on the declared or inferred type of the target value.

If a predicate cannot be meaningfully applied (e.g. numeric comparison on a string), the predicate is considered invalid and produces a diagnostic.

Invalid predicates do not abort query evaluation; they yield an empty refinement and report diagnostics.
This aligns with recoverability and avoids magic coercion.

### Diagnostics and ambiguity

Queries may produce diagnostics without failing.

Typical cases include:
- No matches found
- Ambiguous matches
- Invalid predicates
- Type mismatches

Diagnostics are accumulated on the query handle and can be inspected after evaluation.

Ambiguity is not an error:
- All matching results are returned
- Diagnostics indicate that the result was ambiguous

The absence of diagnostics does not imply semantic correctness — only that the query was well-formed and applicable.
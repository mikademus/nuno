// arf_query.hpp - A Readable Format (Arf!) - Query Interface
// Version 0.3.0
//
// Copyright 2025 Mikael Ueno A
// Licenced as-is under the MIT licence.
//
// -----------------------------------------------------------------------------
// Query interface principles
// -----------------------------------------------------------------------------
//
// The Arf! query API is a human-facing, read-oriented interface for retrieving
// values from a document. It is intentionally distinct from reflection.
//
// Core principles:
//
// 1. Queries observe, never mutate.
//    Querying does not alter document structure or values.
//
// 2. Queries are selections, not navigation.
//    Path expressions and fluent operations progressively narrow a result set.
//    They do not encode absolute addresses or structural identity.
//
// 3. Paths primarily express structure and value-based selection is explicit.
//    Identifier-like value selection is permitted in constrained collection contexts.
//
// 4. Document order is preserved unless explicitly refined.
//    The relative authored order is preserved even in query results.
//
// 5. Plurality is the default.
//    Queries may yield zero, one, or many results.
//
// 6. Ambiguity is reported, not fatal.
//    Ambiguous queries return all matching results and diagnostics.
//
// 7. Values may act as identifiers at query time,
//    but never become structural names.
//
// 8. Evaluation is implicit.
//    A query is executed when results are accessed.
//    There is no explicit "eval" or execution step.
//
// 9. Query results are transparent and present.
//    Current results (the working set) can always be examined.
//    (Again, there is not explicit execution step.)
//
// The query API is optimised for human ergonomics and clarity.
// Tooling, mutation, and structural introspection belong to reflection.
//
// -----------------------------------------------------------------------------
// Document order is always preserved
// -----------------------------------------------------------------------------
//
// All query results preserve the authored order from the source document.
// This is an invariant across all operations.
//
//     auto results = query(doc, "races").strings();
//       --> results[0] is the first "races" element in the document
//       --> results[1] is the second "races" element in the document
//
// This guarantee holds for:
//   - Multi-result queries
//   - Predicate filtering (where clauses)
//   - Structural selection (select clauses)
//   - Ambiguous matches
//
// Order is NEVER altered by:
//   - Query evaluation strategy
//   - Internal optimization
//   - Result caching
//
// The author's intent is preserved.
//
// -----------------------------------------------------------------------------
// Query syntax and usage model
// -----------------------------------------------------------------------------
//
// A query is a progressive selection pipeline over a document.
// Each link in the chain narrows or refines a working result set.
//
// -----------------------------------------------------------------------------
// 1. Dot-paths (structural and contextual selection)
// -----------------------------------------------------------------------------
//
// Dot-paths describe structural descent and contextual selection.
// Any dot-path accepted by query() is also valid in select().
//
//     query(doc, "world.races");
//     query(doc).select("world.races");
//     query(doc, "world").select("races");
//
// These forms are equivalent.
//
// -----------------------------------------------------------------------------
// 1.1. Dot-path semantics: Progressive filtering, not navigation
// -----------------------------------------------------------------------------
//
// A dot-path is NOT a navigation through nodes or addresses.
// It is a PROGRESSIVE FILTER that narrows a working result set.
//
// Each segment applies a selection predicate:
//
//     "world.races.orcs"
//
// Conceptual execution:
//
//     1. Start with entire document as working set
//     2. Filter to elements matching "world" → working set = {world entities}
//     3. Filter to elements matching "races" → working set = {race entities}
//     4. Filter to elements matching "orcs"  → working set = {orc entities}
//
// This is analogous to:
//   - SQL: Successive WHERE clauses on derived tables
//   - Functional: Composition of filter/map operations
//   - Optics: Progressive lens focusing
//
// It is NOT analogous to:
//   - Filesystem paths: /world/races/orcs
//   - Object navigation: doc.world.races.orcs
//   - Address traversal: root().top("world").sub("races")
//
// The distinction matters:
//   - Navigation implies unique paths and structural identity
//   - Filtering implies set operations and plurality
//
// Query embraces plurality and set-based selection.
// Reflection provides precise structural addressing.
//
// -----------------------------------------------------------------------------
// 2. Flowing selection chains
// -----------------------------------------------------------------------------
//
// Queries are built as a flowing chain of selection links:
//
//     query(doc, "world.races")
//         .where("race", "orcs")
//         .select("poise");
//
// Each link refines the current result set.
// There is no implicit backtracking or guessing.
//
// -----------------------------------------------------------------------------
// 3. Mixed selection styles
// -----------------------------------------------------------------------------
//
// Structural selection (dot-paths) and predicate selection can be mixed:
//
//     query(doc)
//         .select("world")
//         .select("races")
//         .where("race", "orcs")
//         .select("population");
//
// Dot-paths may appear both at query construction and mid-chain.
//
// -----------------------------------------------------------------------------
// 4. Value-based selection in dot-paths
// -----------------------------------------------------------------------------
//
// In collection contexts (such as tables), dot-paths may include
// value-based selectors as a shorthand for identifier lookup.
//
//     world.races.orcs
//     world.races.orcs.poise
//
// Here, "orcs" acts as a query-time identifier selecting rows whose
// leading identifier column matches "orcs".
//
// This does not promote values to structural names.
// It is equivalent to an equality predicate, a constrained, contextual
// shorthand for equality selection.
//
// Consider the common use case of tables with leading columns in string format
// (as in the fantasy races query, above). It is desirable and useful to be able 
// to select directly on the leading column as an identifier. 
//
// -----------------------------------------------------------------------------
// 5. Multiple access paths to the same data
// -----------------------------------------------------------------------------
//
// The same value can often be reached in more than one way:
//
//     query(doc, "world.races.orcs.poise");
//
//     query(doc, "world.races")
//         .where("race", "orcs")
//         .select("poise");
//
//     get_string(doc, "world.races.orcs.poise");
//
// All are valid and equivalent in intent.
// The API optimises for human convenience and readability.
//
// -----------------------------------------------------------------------------
// 6. Plurality is always possible
// -----------------------------------------------------------------------------
//
// Queries may yield zero, one, or many results.
// Plurality is the default and never an error.
//
//     auto res = query(doc, "world.races.orcs");
//
// Document order is preserved unless explicitly refined.
//
// -----------------------------------------------------------------------------
// 7. Predicate model
// -----------------------------------------------------------------------------
//
// Predicates are explicit value constraints.
//
//     where("population", gt("1000"))
//     where("name", ne("orc"))
//
// Equality has a shorthand:
//
//     where("race", "orcs")
//
// Predicates refine the current result set and are fully chainable.
//
// -----------------------------------------------------------------------------
// 8. Implicit execution
// -----------------------------------------------------------------------------
//
// Queries execute implicitly when results are accessed.
// There is no explicit evaluation step.
//
//     auto res = query(doc, "world.foo");
//     auto v   = res.as_integer();
//
// Querying observes the document; it never mutates it.
//
// -----------------------------------------------------------------------------

#ifndef ARF_QUERY_HPP
#define ARF_QUERY_HPP

#pragma once

#include <string_view>
#include <vector>
#include <optional>
#include <cstddef>
#include <cstdint>

namespace arf
{
    struct document;

// ---------------------------------------------------------------------
// Query diagnostics
// ---------------------------------------------------------------------

    enum class query_issue_kind
    {
        none,
        not_found,
        ambiguous,
        type_mismatch,
        invalid_predicate,
        structural_violation
    };

    struct query_issue
    {
        query_issue_kind kind;
        std::string_view message;
    };

// ---------------------------------------------------------------------
// Predicate model
// ---------------------------------------------------------------------

    struct predicate
    {
        enum class kind
        {
            eq,
            ne,
            lt,
            le,
            gt,
            ge
        };

        std::string_view column;
        kind              op;

        // Stored as string_view intentionally:
        // value interpretation is deferred and contextual.
        std::string_view value;
    };

    // Predicate constructors (value-side helpers, not execution)
    predicate is (std::string_view value) noexcept;
    predicate ne (std::string_view value) noexcept;
    predicate lt (std::string_view value) noexcept;
    predicate le (std::string_view value) noexcept;
    predicate gt (std::string_view value) noexcept;
    predicate ge (std::string_view value) noexcept;

// ---------------------------------------------------------------------
// Query result
// ---------------------------------------------------------------------

    class query_result
    {
        public:
            query_result() noexcept;

            bool empty() const noexcept;
            bool ambiguous() const noexcept;

            const std::vector<query_issue>& issues() const noexcept;

            // Singular extraction
            std::optional<std::string_view> as_string() const noexcept;
            std::optional<std::int64_t>      as_integer() const noexcept;
            std::optional<double>            as_real() const noexcept;

            // Plural extraction
            std::vector<std::string_view> strings() const;
            std::vector<std::int64_t>      integers() const;
            std::vector<double>            reals() const;

        private:
            // Opaque on purpose: implementation owns storage and identity.
            struct impl;
            impl* pimpl;
    };

// ---------------------------------------------------------------------
// Query builder
// ---------------------------------------------------------------------

    class query
    {
        public:
            explicit query(const document& doc) noexcept;
            query(const document& doc, std::string_view path) noexcept;

            // Structural narrowing (dot-path semantics)
            query& select(std::string_view path) noexcept;

            // Ordinal table selection
            query& table(std::size_t ordinal) noexcept;

            // Predicate refinement
            query& where(std::string_view column, const predicate& pred) noexcept;

            // Shorthand: equality predicate
            query& where(std::string_view column, std::string_view value) noexcept;

            // Explicit enumeration (optional; plurality is default)
            query& all() noexcept;

            // Implicit evaluation on access
            query_result result() const noexcept;

        private:
            const document* doc_;
    };

// ---------------------------------------------------------------------
// Primary entry point
// ---------------------------------------------------------------------

    inline query query(const document& doc) noexcept
    {
        return query{ doc };
    }

    inline query query(const document& doc, std::string_view path) noexcept
    {
        return query{ doc, path };
    }

// ---------------------------------------------------------------------
// Ergonomic getters (human API)
// ---------------------------------------------------------------------

    std::optional<std::string_view>
    get_string(const document& doc, std::string_view path) noexcept;

    std::optional<std::int64_t>
    get_integer(const document& doc, std::string_view path) noexcept;

    std::optional<double>
    get_real(const document& doc, std::string_view path) noexcept;
}

#endif // ARF_QUERY_HPP

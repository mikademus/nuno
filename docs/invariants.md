# Contamination invariants
Contamination represents *semantic breakage that propagates*, not mere editing.

## 1. Sources of contamination
 * Only keys and table rows can be root contamination sources.
 * Categories, tables, and columns are never contamination sources.
 * Root sources are tracked explicitly in:
   * `contaminated_source_keys_`
   * `contaminated_source_rows_`

## 2. Local vs derived state
 * Root contamination sources are authoritative.
 * Node-level `contamination_state` flags on categories and tables are derived.
 * Keys and rows are authoritative sources.
 * A document is contaminated iff at least one root source exists.

## 3. What causes contamination
A key or row becomes contaminated when:
 * Its value(s) are semantically invalid (e.g. type mismatch against a declared type).
 * Semantic invalidity is introduced by editing or generation.
 * Contamination may be introduced even if the node was previously clean.

## 4. Propagation rules
 * Contamination propagates upwards:
   * key → owning category → ancestor categories
   * row → owning table → owning category → ancestor categories
 * Tables and categories reflect contamination if any descendant source is contaminated.
 * Propagation is monotonic until explicitly re-evaluated (no silent clearing).

## 5. Clearing contamination
 * Clearing is never implicit.
 * A client must request clearing via `request_clear_contamination`.
 * Clearing succeeds only if:
   * The requested node is semantically valid now.
   * `request_clear_fn permits` it.
 * Clearing a root source may trigger upward re-evaluation and clearing of derived contamination.

## 6. Semantic validity vs contamination
 * `semantic_state::invalid` is a local fact.
 * `contamination_state::contaminated` is a structural consequence.
 * A node can be:
   * semantically invalid but not a contamination source (e.g. column metadata),
   * or semantically valid but still contaminated (due to another descendant).

## 7. Editing is orthogonal
 * `is_edited` does not imply contamination.
 * Contamination implies semantic concern; editing implies provenance change.
 * Generated data follows the same rules as edited data.

## 8. Invariants the system must maintain
 * Root contamination containers and node flags must never disagree.
 * Clearing the last root source guarantees a clean document.
 * No category or table may be clean if any descendant root source is contaminated.

## 9. Idempotency
 * Calling mark_*_contaminated() multiple times is safe (idempotent).
 * Calling clear_*_contamination() multiple times is safe (idempotent).
 * Propagation terminates at already-contaminated ancestors.
 * Clearing terminates at first non-clean ancestor.
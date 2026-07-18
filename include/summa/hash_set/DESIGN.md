# hash_set / hash_map: scaling beyond linear scan

## Status quo

`SummaHashSet` and `SummaHashMap` (`include/summa/hash_map/hash_map.h`, which mirrors this
file's structure) are **not** hash tables in the algorithmic sense. Each is a pair of
parallel dynamic arrays:

- `keys`: a flat, growable array of precomputed `SummaHashCode` values.
- `values`: a flat, growable array of `void*` payloads, index-aligned with `keys`.

`contains`/`add`/`put`/`get`/`remove` all compute `summa_hash(element, size)` and then call
`summa_array_contains` / `summa_array_index_of` / `summa_array_remove_at`, which **linearly
scan every stored hash code** until they find a match or exhaust the array
(`include/summa/array/array.h`).

Hashing still buys something here: comparing two 8-byte `unsigned long` hash codes is
cheaper than `memcmp`-ing arbitrarily large keys/elements directly. But the scan is still
`O(n)` in the number of entries — the hash doesn't *select* a small candidate set the way a
bucketed table does. Push is amortized `O(1)` (capacity doubles on overflow), but every
`add`/`put` still pays for an `O(n)` dedup scan before it can push, and `summa_hash_set_make`
/ a hypothetical bulk `hash_map_make` grows from the default capacity (8) via repeated
doublings instead of reserving the known final size up front.

Net: fine for small sets (a few dozen entries — linear scan over packed 8-byte ints is fast
and cache-friendly), but it degrades the same way an unsorted array would once entry counts
reach the hundreds or thousands.

## Why this matters for the Scheme interpreter

Two places in a Scheme implementation are classic hot paths for this kind of structure:

- **The global environment.** Every top-level `define` and every builtin procedure lives in
  one table, easily hundreds of entries in a non-trivial program or once the standard
  library is populated. Every unresolved variable lookup that isn't shadowed locally walks
  out to this table — and it happens on *every* variable reference during evaluation,
  including inside loops implemented via recursion.
- **Symbol interning**, if the reader/parser interns identifiers into canonical `Symbol`
  objects (so `eq?` on symbols is pointer comparison) — consulted once per identifier token
  during parsing, so its cost scales with program size rather than execution count.

Local/lexical scopes are usually *not* the concern: a function's parameter list or a `let`
binding set is typically a handful of entries, where a linear scan (or even a plain alist)
is competitive with — sometimes faster than — a real hash table, because there's no bucket
array or hashing overhead to pay for. Most Scheme implementations only bother with a real
hash table for the global/top-level environment for exactly this reason.

**Conclusion**: yes, scale will matter, but narrowly — the global environment and symbol
table are where `O(n)` will actually be felt, not every scope. It's reasonable to keep the
current linear-scan implementation while the interpreter is still correctness-focused, but
treat the change below as a prerequisite before running anything beyond toy scripts, and
watch the global environment/symbol table specifically as the first place it'll bite.

## What needs to change for real `O(1)` average-case behavior

### 1. Bucketed storage

Replace "flat array of hash codes, scanned linearly" with a fixed-size (resizable) bucket
array indexed by `code % bucket_count` (or `code & (bucket_count - 1)` if `bucket_count` is
kept a power of two — a mask is cheaper than a modulo). Lookup, insert, and remove all start
by jumping straight to the entry's bucket instead of touching every entry in the structure.

### 2. Collision resolution strategy

Two entries can land in the same bucket. Pick one:

- **Open addressing** (linear or quadratic probing, or Robin Hood hashing): all entries live
  in one contiguous table; a collision probes forward to the next slot(s). Better cache
  locality, no per-entry allocation, fits this codebase's existing "everything is a flat
  `SummaArray`" style. Deletion needs care — either tombstone markers (slots marked
  "deleted, keep probing past me") or backward-shift deletion (Robin Hood-style, avoids
  tombstone buildup but is fiddlier to implement correctly).
- **Separate chaining**: each bucket holds a small list/array of entries that hashed there.
  Simpler to reason about and deletion is trivial, but adds per-node allocation/pointer
  overhead and is a worse fit for this project's flat-array aesthetic.

**Recommendation**: open addressing with tombstones as a first cut — simplest to implement
correctly on top of the existing `SummaArray` primitives, and consistent with how the rest
of this codebase avoids linked structures.

### 3. Load factor and resizing

Track `count / bucket_count` and rehash into a larger table (typically 2x) once it crosses a
threshold (commonly ~0.7). This is what actually keeps operations `O(1)` *amortized* — without
it, a bucketed table under high load degrades toward the same linear-scan behavior we have
today, just with extra indirection on top. There is currently no load-factor concept
anywhere in `array.h`/`hash_set.h`; growth is purely "did we run out of slots," not "are we
dense enough to start colliding a lot."

### 4. Real equality on hash match — no longer optional

Today, two colliding hash codes are treated as "the same key" outright (this was flagged
and deliberately deferred during the initial hash_set implementation). That's a tolerable
simplification when collisions are rare accidents in a linear scan, but once entries are
routed into shared buckets by design, collisions between **genuinely different** keys become
routine, not exceptional. Any bucketed rewrite must add a `memcmp` (over `key_size`/
`element_size` bytes) on hash match before declaring equality — otherwise unrelated keys
that happen to collide will silently overwrite or shadow each other.

### 5. Hash function quality

`djb2` (the current hash, `hash_set.h`) is a reasonable general-purpose choice and doesn't
need to change for this rewrite. It matters more once buckets are in play, since poor
distribution shows up as bucket pile-up rather than just a longer flat scan — worth
revisiting only if profiling shows clustering, not a prerequisite for the rewrite itself.

## What does *not* need to change

The public API (`summa_hash_set_make_empty` / `contains` / `add` / `remove` / `clear` /
`copy` / `free`, and the equivalent `summa_hash_map_*` functions) does not need new
signatures for any of the above — bucketing, load factor, and collision resolution are all
internal representation changes behind the existing opaque `SummaHashSet` / `SummaHashMap`
pointer types. Callers (including the Scheme interpreter, whenever it starts depending on
this) shouldn't need to change when this lands.

## Explicitly out of scope here

- **Iteration / enumeration** over a set or map's contents. There's currently no way for a
  consumer outside this header's own implementation TU to walk the entries (noted
  previously, deliberately deferred). Worth reconsidering once the interpreter needs to,
  e.g., enumerate global bindings for a REPL or debugger — but it's an additive API surface
  question, orthogonal to the storage rewrite above.
- **Bulk-reserve on construction** (`summa_hash_set_make`/an eventual bulk `hash_map_make`
  sizing capacity to the known input length up front instead of growing via doubling) — a
  smaller, independent optimization that's worth doing opportunistically but isn't blocking.

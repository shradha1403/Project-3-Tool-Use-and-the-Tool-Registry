# ECE 309 Project 3 Design Log

**P2 status:** i used my own P2 `Conversation` and `SentinelScanner` (no reference build).
**Tools used:** i used Claude (an AI assistant) to help draft the `ToolRegistry`, `Workspace::resolve`, and `calc_evaluate` code and the first version of the tests. i went through each piece myself and can walk through it. i ran the sanitizer builds and the measurements below myself.

## Hash table

i went with **separate chaining**, where each bucket is a `std::vector<std::unique_ptr<Tool>>`. i picked it over linear probing because there's no clustering to worry about, a bad hash only hurts one bucket instead of snowballing, and the load factor can sit at 1.0 safely since a chain can always take one more entry. with open addressing i'd have had to stay under 1 or `find` on a missing name could loop forever.

The hash is **64-bit FNV-1a**. it xors in every byte and then multiplies by the FNV prime, so it uses every character and is order sensitive (`"ab"` and `"ba"` differ). `hash()` returns the full value and the table reduces it with `hash & (bucket_count - 1)`, which works because the bucket count is always a power of two (starts at 16).

**Resize policy:** before inserting, if `size + 1 > bucket_count` (load factor would go over 1.0), i double the bucket count and re-insert every entry using the new count. checking before the insert means `load_factor()` never goes above 1.0.

**Measurements for `tool_0` to `tool_9999`:**

| | Value |
|---|---|
| final bucket count | 16384 |
| load factor | 0.610 |
| longest chain, FNV-1a (real) | 6 |
| distinct hash values | 10000 / 10000 |
| longest chain, weak hash `s[0] + s.size()` at 16384 buckets | 9000 |

the weak hash only lands in 4 buckets because every name starts with `t` and the length is only 6 to 9 chars. so one chain would have around 9000 entries and lookup turns into a linear scan. FNV-1a keeps it at 6.

**Why `add()` is amortized O(1):** a resize from n to 2n buckets costs O(n), but it only happens after about n inserts since the last one (the table doubles each time). so the O(n) rehash cost gets spread over about n cheap inserts, which is O(1) extra per insert. the cheap inserts themselves are O(1) expected because chains stay short.

## Pointer stability

the buckets hold `unique_ptr<Tool>`, and the `Tool` objects live on the heap. when i rehash, i only move the `unique_ptr`s into the new bucket array. the `Tool` itself never moves, so the raw `Tool*` that `find()` gave out earlier still points at the same object. the scale test checks this by grabbing the pointer for `tool_0` after the first insert and comparing it after the 10,000th.

## Calculator trace: `2 * (3 + 4) - 5`

- `2`: push onto values. values `[2]`, ops `[]`
- `*`: ops is empty, push. ops `[*]`
- `(`: push. ops `[* (]`
- `3`: values `[2 3]`
- `+`: top of ops is `(`, which isn't an operator, so just push. ops `[* ( +]`
- `4`: values `[2 3 4]`
- `)`: apply `+`. pop 4 (right), then 3 (left), push 7. values `[2 7]`. top is `(` now, so pop it. ops `[*]`
- `-`: top is `*` with precedence 2, which is >= 1, so apply it: 7 is right, 2 is left, push 14. values `[14]`. ops empty so push `-`
- `5`: values `[14 5]`
- end: apply `-`: 5 right, 14 left, push 9. values `[9]`

the answer is 9. using `>=` instead of `>` is what makes `10 - 4 - 3` come out as 3 and not 9.

## Errors as values

in an agent loop a tool failing is a normal thing that happens (file missing, bad path, divide by zero), and the model is supposed to read the error and try something else. if those were exceptions, every caller up the chain would need to catch them just to turn them back into a message for the model, and one missed catch would crash the harness. returning a `ToolResult::failure(...)` or `false` plus a message keeps the failure path as ordinary data flow. exceptions are for real bugs in the C++ (like calling `as_string()` on a number), where crashing loudly or hitting the dispatcher's last-resort catch makes sense.

## What i'd do differently

i'd make the registry's bucket type a small struct with the cached hash value next to the pointer. right now rehash recomputes `hash(t->name())`, which calls the virtual `name()` and hashes the string again for every entry. caching it would make resizes cheaper and also let `find` skip the string compare on most mismatches.

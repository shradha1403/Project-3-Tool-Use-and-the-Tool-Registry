# ECE 309 Project 3: Tool Use and the Tool Registry

This is my Project 3 for ECE 309. It extends the P2 chat harness so the model can actually do things: read a file, write a file, list a folder, or do arithmetic. The harness finds the tool call in the model's reply, runs it, puts the result back in the conversation, and asks the model again without waiting for the user. Control only goes back to the user when the model replies with plain text.

Full assignment is in `P3_spec.md`. My design write-up is in `docs/design-log-p3.md`.

## What i implemented

| Piece | File | What it does |
|---|---|---|
| `ToolRegistry` | `include/tools/tool_registry.h`, `src/tools/tool_registry.cpp` | hash table from tool name to tool. separate chaining, 64-bit FNV-1a, doubles at load factor 1.0 |
| `Workspace::resolve` | `src/tools/workspace.cpp` | turns a model-supplied path into a path inside the workspace folder, or rejects it (pure text, never touches the filesystem) |
| `calc_evaluate` | `src/tools/calculator_eval.cpp` | two-stack arithmetic evaluator, loops only so it handles very long and deeply nested expressions |

Everything else (JSON, tokenizer, file tools, dispatcher, harness, `main.cpp`) came with the starter. P2's `Conversation` and `SentinelScanner` are my own, copied into `include/core` and `src/core`.

## Build and test

Needs Linux or WSL2 (ASan and UBSan are on by default).

```bash
cmake -S . -B build -DMH_WERROR=ON
cmake --build build -j
cd build && ctest --output-on-failure
```

Or run the pieces on their own:

```bash
./build/test_p2      # P2 regression
./build/test_loop    # provided tool-loop tests
./build/test_p3      # my tests (try: ./build/test_p3 reg_)
```

## Try it

```bash
./build/miniharness --tools
./build/miniharness --script scripts/tools_demo.script
./build/miniharness --script scripts/escape_attempt.script
```

`tools_demo` writes `workspace/notes/summary.txt`. Delete that file to reset the demo.

## Results

All four ctest suites pass (P2 regression, p3, tool loop, e2e goldens) with `-Werror` and sanitizers on.

For the 10,000 tools `tool_0` to `tool_9999`:

| | value |
|---|---|
| buckets | 16384 |
| load factor | 0.610 |
| longest chain (FNV-1a) | 6 |
| longest chain with the weak hash `s[0] + s.size()` | 9000 |

## Layout

```
include/   headers (core, json, tools, harness, model, util)
src/       implementation (core, json, tools, harness, model, main.cpp)
tests/     p2 regression, p3 tool loop, my p3 suite, e2e golden sessions
scripts/   demo sessions
docs/      design-log-p3.md
reference/ instructor P2 reference (not used, my own P2 builds fine)
```

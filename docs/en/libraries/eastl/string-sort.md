---
title: EASTL String, Span, and Sort
topic: libraries
feature: eastl-string-sort
standard: N/A
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# EASTL String, Span, and Sort

> Source path: `references/impl/EASTL/include/EASTL/string.h`, `span.h`, `sort.h`

## eastl::string

EASTL's `basic_string` is similar to the standard `std::string` but with key differences:

- No exceptions — `at()` returns error codes instead of throwing `out_of_range`
- Allocator parameter is an instance (not a template parameter)
- Built-in debug naming (`set_name()`/`get_name()`)
- SSO capacity is implementation-dependent

## eastl::span

EASTL's `span<T>` provides a non-owning view of contiguous memory, similar to `std::span`.

## eastl::sort

EASTL's sorting algorithms are optimized for game scenarios:

- Uses insertion sort for small arrays (≤ 20 elements)
- Detects and exploits existing order in sorted data
- Supports user-defined comparators
- No exceptions — behavior is undefined if the comparator throws

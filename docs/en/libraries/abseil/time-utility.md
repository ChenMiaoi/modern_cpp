---
title: Abseil Time and Basic Utilities
topic: libraries
feature: time-utility
standard: N/A
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# Abseil Time and Basic Utilities

> Source path: `references/impl/abseil-cpp/absl/time/`, `absl/types/`, `absl/container/`

## absl::Time and absl::Duration

Abseil's time library is the foundation of Google Calendar, Spanner, and other products. Core types:

```cpp
// Duration: nanosecond-precision time interval
class Duration {
  int64_t rep_hi_;  // High bits of seconds
  uint32_t rep_lo_;  // Low bits of nanoseconds (0-999999999)
};

// Time: absolute time point since Unix epoch
class Time {
  Duration d_;  // Relative to Unix epoch
};

// TimeZone: wrapper around IANA timezone database
class TimeZone {
  // Internally references the cctz library (civil_time), Google's own timezone library
  // Supports DST transitions, historical timezone rules
};
```

**Why not `std::chrono`?** `std::chrono` in the C++11 era lacked timezone support (only gained `std::chrono::time_zone` in C++20). Abseil's time library has been in use at Google since 2011, nearly a decade before the standard. Its API is more intuitive:

```cpp
absl::Time now = absl::Now();
absl::Time tomorrow = now + absl::Hours(24);

// Timezone conversion
absl::TimeZone tz = absl::time_internal::LoadTimeZone("America/New_York");
absl::CivilMinute cm = absl::ToCivilMinute(now, tz);
```

## absl::Span

`absl::Span<T>` is the predecessor of `std::span<T>`—a non-owning view of contiguous memory:

```cpp
template <typename T>
class Span {
  T* data_;
  size_t size_;
};

// Usage
void Process(absl::Span<const int> data) {
  for (int x : data) { ... }
}

Process({1, 2, 3});                      // From initializer_list
Process(std::vector<int>{1, 2, 3});      // From vector
Process(absl::MakeConstSpan(arr, 3));    // From array
```

## absl::optional

`absl::optional<T>` is the predecessor of `std::optional<T>`. Implementation strategy:

```cpp
template <typename T>
class optional {
  union {
    char dummy_;
    T value_;
  };
  bool engaged_;
};
```

When `engaged_ = true`, `value_` contains a valid value; when `engaged_ = false`, `value_` is in an undestroyed state (the union's `dummy_` member is active).

## absl::flat_hash_set / flat_hash_map

Beyond the underlying SwissTable mechanism (see [SwissTable chapter](/libraries/abseil/swisstable)), Abseil provides a series of container specializations:

| Container | Feature |
|-----------|---------|
| `flat_hash_set/map` | Inline storage, cache-friendly, iterator unstable |
| `node_hash_set/map` | Node allocation, reference/iterator stable |
| `btree_set/map` | B-tree ordered container, faster than `std::set/map` |
| `btree_map` | Cache-friendly ordered map, larger nodes (reduces tree height) |

`btree_set/map` is Abseil's replacement for `std::set/map` (red-black tree)—B-tree stores multiple elements per node (typically filling a cache line), reducing tree height and pointer chasing. For ordered container scenarios, `absl::btree_map` is typically 2-5x faster than `std::map`.

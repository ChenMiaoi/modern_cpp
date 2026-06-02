---
title: Abseil Strings and Hash Utilities
topic: libraries
feature: strings-hash
standard: N/A
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# Abseil Strings and Hash Utilities

> Source path: `references/impl/abseil-cpp/absl/strings/`, `absl/hash/`

## absl::string_view

`absl::string_view` is the predecessor of `std::string_view`—Google used it extensively internally before C++17 standardization. Core implementation: a `(pointer, length)` pair pointing to external string data, not owning memory.

```cpp
class string_view {
  const char* data_;
  size_t size_;
  // sizeof = 16 bytes (two pointer-sized members)
};
```

**Note**: `absl::string_view` has an API nearly identical to `std::string_view`, but Abseil's version is Google-internal compatible—it does not provide certain constexpr methods found in some standard versions, to maintain compatibility with older compilers. In new projects, use `std::string_view` directly.

## absl::StrCat and Formatted Concatenation

`StrCat` is the predecessor of `std::format`, designed specifically for efficient string concatenation:

```cpp
// Traditional approach — 3 temporary allocations
std::string result = "Hello, " + name + "! You have " + std::to_string(count) + " messages.";

// StrCat — precomputes total length, single allocation
std::string result = absl::StrCat("Hello, ", name, "! You have ", count, " messages.");
```

`StrCat`'s implementation strategy:

1. **Precompute total length**: iterate all arguments, accumulate each argument's string length
2. **Single allocation**: allocate an output buffer of `total_length` bytes
3. **Direct copy**: memcpy each argument directly to the correct position in the output buffer

This avoids the multiple allocations and copies of `std::string::operator+`.

## absl::Hash

Abseil provides its own hashing framework, safer and more uniform than `std::hash`:

```cpp
// std::hash problem: many types have poor hash functions
// absl::Hash guarantees high-quality distribution, supports all basic types and containers
template <typename T>
size_t hash = absl::Hash<T>{}(value);
```

`absl::Hash` design:

1. **Compositional hashing**: container type hashes are composed from element hashes, not simple XOR
2. **Domain separation**: different types use different hash seeds, avoiding collisions between `int(42)` and `double(42.0)`
3. **SALSA20 stream cipher**: internally uses SALSA20's mixing function to ensure high-quality diffusion

```cpp
// absl::Hash combine function (simplified)
void H1::Combine(size_t seed, size_t value) {
  // Mixing function based on SALSA20 quarter-round
  seed += value * 0x9e3779b97f4a7c15;  // Golden ratio multiplier
  seed = absl::rotl(seed, 11);
  seed *= 0x243f6a8885a308d3;  // Fractional part of π
}
```

**Comparison with std::hash**:

| Dimension | `std::hash` | `absl::Hash` |
|-----------|-----------|-------------|
| Quality | Not guaranteed (many implementations use identity) | Guaranteed high-quality distribution |
| Security | No collision attack resistance | **SipHash/SALSA20 mixing** |
| Composition | No standard method | `H1::Combine` |
| Container support | None | Automatic support for vector/pair/tuple |
| Compile-time | Limited | constexpr-friendly |

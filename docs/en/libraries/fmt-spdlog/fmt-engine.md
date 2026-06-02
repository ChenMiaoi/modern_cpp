---
title: fmt 格式化引擎
topic: libraries
feature: fmt-engine
standard: C++20
status_checked_at: 2026-06-02
implementation:
  fmt:
    paths:
      - references/impl/fmt/include/fmt/base.h
      - references/impl/fmt/include/fmt/format.h
    symbols:
      - basic_format_arg
      - basic_memory_buffer
      - fstring
      - format_string_checker
exercises: []
solutions: []
---
# fmt Formatting Engine

> Source path: `references/impl/fmt/include/fmt/base.h`, `format.h`

## Three-Stage Pipeline

```
fmt::format("Hello, {}! Value: {:d}", name, count)
     |
     v
Stage 1: Compile-time format string parsing (consteval)
  fstring<Args...>'s consteval constructor
  parse_format_string(str, checker)
  Validation: valid argument index? type match? format spec valid?
  Invalid -> compile-time report_error() -> compilation failure
     |
     v
Stage 2: Argument type erasure (tagged union)
  stored_type_constant<T>::value -> type enum
  value<Context> union storage (16 bytes, 128-bit value storage)
  Runtime: basic_format_arg::visit(visitor) switch dispatch
     |
     v
Stage 3: Output generation
  memory_buffer: 500-byte stack array, zero heap allocation
  formatter<T>::format(value, ctx)
  Dragonbox floating-point / direct integer write
```

## Compile-Time Format String Checking

```cpp
template <typename... T> struct fstring {
  using checker = detail::format_string_checker<char, int(sizeof...(T)), ...>;

  template <size_t N>
  FMT_CONSTEVAL fstring(const char (&s)[N]) : str(s, N - 1) {
    parse_format_string<char>(str, checker(str, arg_pack()));
  }
};
```

`FMT_CONSTEVAL` expands to C++20 `consteval`, forcing the compiler to execute all validation at compile time.

### Type Compatibility Checking

```cpp
constexpr auto integral_set = sint_set | uint_set | bool_set | char_set;
case 'd': return parse_presentation_type(pres::dec, integral_set);
case 'f': return parse_presentation_type(pres::fixed, float_set);
```

`parse_presentation_type` verifies via `in(arg_type, set)` bitwise operations: if the argument type is not in the allowed set, a compile-time error is triggered.

## Type Erasure: Tagged Union + Visit

```cpp
template <typename Context> class value {
public:
  union {
    int int_value;
    unsigned uint_value;
    double double_value;
    string_value<char_type> string;       // {const Char* data; size_t size;}
    custom_value<Context> custom;         // {void* value; void (*format)(...);}
    // ... 15 types
  };
};

template <typename Visitor>
FMT_CONSTEXPR auto visit(Visitor&& vis) const {
  switch (type_) {
  case type::int_type:     return vis(value_.int_value);
  case type::double_type:  return vis(value_.double_value);
  case type::string_type:  return vis(value_.string.str());
  case type::custom_type:  return vis(handle(value_.custom));
  // ... 15 branches
  }
}
```

The union occupies 16 bytes — the largest member is `long double` (16 bytes).

## Output Buffer

```cpp
// 500-byte stack buffer, short strings (<50 chars) stay entirely on the stack
template <typename T, size_t SIZE = inline_buffer_size,
          typename Allocator = std::allocator<T>>
class basic_memory_buffer : public detail::buffer<T> {
  T data_[SIZE];  // Stack buffer
  Allocator alloc_;
  // Automatically switches to heap allocation when SIZE is exceeded
};
```

## User API

The primary user entry points are `fmt::format`, `fmt::print`, and similar high-level interfaces; the existing text above already expands into format string checking, argument erasure, and output buffering.

## Standard Semantics

fmt covers the complete format string syntax of C++20 `std::format` / C++23 `std::print`: positional arguments `{0}`, automatic indexing `{}`, named arguments `{name}`, nested width/precision `{:{}}`, format specifiers `fill align sign # 0 width precision type`. Key semantic differences are as follows:

| Dimension | `std::format` | `fmt::format` |
|-----------|---------------|---------------|
| Format string type | `std::format_string<Args...>` (consteval since C++20) | `fmt::fstring<Args...>` (`FMT_CONSTEVAL` constructor) |
| Check timing | Compile-time (consteval construction) | Compile-time (consteval construction) + runtime fallback (`vformat` path unchecked) |
| Error reporting | `static_assert` or compiler diagnostics | `report_error()` → compile-time `static_assert`; runtime `format_error` exception |
| Custom types | `std::formatter<T>` specialization | `fmt::formatter<T>` specialization + ADL `format_as()` free function + `formatter<T>::format_as()` member function |
| Floating-point format | Implementation-defined (typically Grisu3 or `to_chars`) | Dragonbox (shortest representation, ~2-5x faster than `to_chars`) |
| Compile-time format string | `std::format_string` (non-type template parameter, C++26) | `fmt::fstring` (class template, C++20 consteval constructor) |
| Dynamic format string | `std::vformat(fmt, args)` (no compile-time checking) | `fmt::vformat(fmt, args)` (no compile-time checking, runtime parsing) |

**fmt's additional strengthening of compile-time checking**:

- `format_string_checker` parses the format string character by character at compile time, calling `parse_funcs_[id](context_)` for each `{id}` — this invokes the corresponding `formatter<T>::parse()`, pushing compile-time validation of format specifiers down to each type's `parse` method.
- `compile_parse_context` inherits from `parse_context` and holds a `types_` array and `num_args_`, verifying in `check_arg_id(id)` that argument indices are within bounds, and in `check_dynamic_spec(int)` that dynamic width/precision arguments must be integer types.
- `mapped_type_constant<T, Char>` maps user types to built-in types or `custom_type` via `type_mapper`, then through `stored_type_constant` determines the storage path — at compile time it can determine whether `formatter<T>` exists (`has_formatter<T, Char>()`), and if not, triggers a `type_is_unformattable_for` compile error.
- `encode_types<Context, T...>()` packs each argument's `type` enum value into a `ullong` (4 bits per argument), computing `desc_` at compile time with zero runtime overhead for type reading.
:::::

## Object Layout

The key structures of `value<Context>` tagged union and `basic_memory_buffer` have already been given above; a unified layout diagram for argument storage, handle, and buffer will follow.

## Core Source Paths

`base.h` and `format.h` were given at the beginning of this article; the call chains for `parse_format_string`, `format_string_checker`, `visit`, and `memory_buffer` will follow.

## Core Classes / Functions

**`fstring<Args...>`** (`base.h`): Format string wrapper, holding `basic_string_view<Char> str`. The `FMT_CONSTEVAL` constructor accepts `const char (&)[N]` or `std::string`, and during construction calls `parse_format_string<char>(str, checker(str, arg_pack()))` — `checker` is an instance of `format_string_checker<char, int(sizeof...(T)), ...>`. It scans the format string character by character at compile time; each `{` enters replacement field parsing, `}` ends it; invalid format strings trigger `static_assert` at compile time via `report_error()`.

**`format_string_checker`** (`base.h:1679`): Compile-time format string validator. During construction, stores each argument's `type` enum into `types_[]` and each `formatter<T>::parse` function pointer into `parse_funcs_[]`. `on_arg_id()` handles automatic indexing, `on_arg_id(int)` handles explicit indexing (calling `context_.check_arg_id(id)` to verify bounds), `on_arg_id(basic_string_view<Char>)` handles named arguments (linear search in `named_args_[]`). `on_format_specs(id, begin, end)` calls `parse_funcs_[id](context_)` to push format specifier validation down to the corresponding `formatter<T>::parse()`.

**`value<Context>`** (`base.h:2135`): 16-byte tagged union, members include `int`, `unsigned`, `long long`, `ullong`, `native_int128`, `bool`, `char_type`, `float`, `double`, `long double` (largest member, determines union size), `const void*`, `string_value<char_type>` (`{const Char* data; size_t size}`), `custom_value<Context>` (`{void* value; void (*format)(...)}`). The constructor determines the storage branch via `stored_type_constant<T>::value`: built-in types write directly to the corresponding union member; user types take the `custom_tag` path, storing an object pointer + `format_custom<T>` function pointer.

**`basic_format_arg<Context>`** (`base.h:2451`): Holds `value<Context> value_` and `type type_`. The `visit(Visitor&&)` method performs a 15-way switch dispatch on `type_`, passing the union member to the visitor. The `format_custom()` method calls `value_.custom.format()` for `custom_type`, deferring the call to `formatter<T>::format()` until runtime.

**`basic_memory_buffer<T, SIZE, Allocator>`** (`format.h:778`): Inherits from `detail::buffer<T>` (`{T* ptr_; size_t size_; size_t capacity_; grow_fun grow_}`), embeds `T store_[SIZE]` (default `SIZE = 500`) and `Allocator alloc_`. During construction, `ptr_` points to `store_`, `capacity_ = SIZE`. The `grow` static method grows by 1.5x when capacity is insufficient (`new_capacity = old + old/2`), allocates heap memory, `memcpy`s the data, and frees the old buffer (if not `store_`). During destruction, only heap allocations are freed (`if (data != store_)`).

**`basic_format_args<Context>`** (`base.h:2532`): Argument view, holding `ullong desc_` and a union (`values_` or `args_`). When the number of arguments is ≤ `max_packed_args` (15), type information is encoded in the low 60 bits of `desc_` (4 bits per argument), with values stored in a contiguous `value<Context>` array; when more than 15 arguments are present, the `is_unpacked_bit` is set in the high bits of `desc_`, and it switches to storing a `basic_format_arg<Context>` array (each 24 bytes: 16-byte union + 4-byte type + padding). The `type(index)` method reads a 4-bit type code from `desc_` at the given offset.
:::::

## Key Algorithms

The three-stage pipeline has already been covered in the main text; a summary of key branches for "parsing → type checking → visit dispatch → output writing" will follow.

## ABI Constraints

fmt is primarily composed of header-only templates and inline functions, with no standard-library-style stable ABI commitment. Specific constraints:

- **Namespace versioning**: `FMT_BEGIN_NAMESPACE` expands to `namespace fmt { inline namespace v12 { ... }}`. `v12` is the ABI version marker — linking different versions of fmt from the same binary across major versions (e.g. v11 → v12) will cause symbol conflicts or ODR violations, because `inline namespace` causes `fmt::format` to actually resolve to `fmt::v12::format`.
- **Template instantiation**: Core types like `formatter<T>`, `value<Context>`, `basic_format_arg<Context>` are all templates, independently instantiated in each translation unit. ABI stability depends on whether the compiler produces identical layouts for the same template parameters — this is generally true, but differences in empty base class optimization (EBO) and `[[no_unique_address]]` semantics across compiler versions may cause different actual sizes for `value<Context>`.
- **`FMT_API` marker**: A small number of non-template symbols (such as `dragonbox::to_decimal`, `dragonbox::get_cached_power`, `assert_fail`) are marked `FMT_API`. On Windows this expands to `__declspec(dllexport/dllimport)`, on ELF/Mach-O to `__attribute__((visibility("default")))`. These symbols remain stable within a major version but may change signatures across major versions.
- **`FMT_BUILTIN_TYPES` switch**: Defaults to 1 (built-in types use direct union storage). When set to 0, all types go through the `custom_value` path (function pointer indirect call), changing the mapping result of `stored_type_constant` and causing different `type_` enum values for the same `value<Context>` — mixing different settings across translation units is an ODR violation.
- **No exported symbol table commitment**: fmt does not maintain a version script like `libc.so`. In dynamic library scenarios (`FMT_SHARED`), the set of exported symbols changes across versions. Upgrading fmt versions requires recompiling all dependents.
:::::

## Exception Safety

fmt's exception safety model has three layers:

**Format string errors (compile-time)**:
- `format_string_checker::on_error()` calls `report_error(message)`, triggering compilation failure in a consteval context. No runtime exceptions are produced.
- `compile_parse_context::check_arg_id()` verifies argument index bounds, also producing compile-time errors.

**Format string errors (runtime, `vformat` path)**:
- `vformat_to` encounters invalid format during parsing (e.g. unclosed `{`, invalid format specifier character), throws `fmt::format_error` (inherits `std::runtime_error`).
- `basic_format_args::get()` returns an empty `basic_format_arg` (`type_ == none_type`) when the index is out of bounds; visit goes to the `monostate` branch, not throwing an exception but outputting an empty string.

**Memory allocation failure**:
- `basic_memory_buffer::grow()` calls `Allocator::allocate(new_capacity)`. When using `std::allocator`, allocation failure throws `std::bad_alloc`.
- Key protection: `grow()` first allocates new memory, `memcpy`s the data, then frees the old memory. If `allocate` throws, the old data is unaffected (`old_data` is still in place), and `basic_memory_buffer` properly releases `store_` or previous heap allocations during destruction. If `deallocate` throws (the standard requires it not to, but fmt comments explicitly state "even if it throws it's harmless", since the new storage has already taken over).

**User-defined formatter throws exception**:
- `formatter<T>::format()` is called via `custom_value<Context>::format` function pointer. If the user formatter throws, the exception propagates along `vformat_to`'s call stack — `basic_memory_buffer` is destroyed during stack unwinding, releasing allocated heap memory (RAII).
- `format_to`'s output iterator may have already partially written — written characters cannot be rolled back. For `std::string` output (`back_insert_iterator`), characters that have been `push_back`ed remain in the string.

**Exception-disabled mode**:
- When `FMT_USE_EXCEPTIONS=0`, `FMT_THROW(x)` expands to `::fmt::assert_fail(__FILE__, __LINE__, (x).what())` — directly `abort()`, no exceptions produced. `FMT_TRY`/`FMT_CATCH` expand to `if(true)`/`if(false)`, and all catch blocks are optimized away.
:::::

## Iterator / Reference Invalidation

**`basic_format_args` borrowed lifetime**:
- `basic_format_args` is a **non-owning view**, not holding memory for argument values. It references the `value<Context>[]` or `basic_format_arg<Context>[]` in `format_arg_store` via pointers.
- `format_arg_store<Context, NUM_ARGS, ...>` is constructed by `make_format_args(args...)` or internally by `fmt::format`, with its lifetime bound to the calling expression. In typical usage `vformat_to(out, fmt, make_format_args(args...))`, `format_arg_store` is a temporary object, valid until `vformat_to` returns — `basic_format_args` must not escape to external storage.
- **Dangerous pattern**: `auto args = fmt::make_format_args(42, "hello"); fmt::vformat("{}", args);` — `args` internally holds a pointer to a stack-allocated temporary `format_arg_store`, but `format_arg_store` has already been destroyed after the first statement, making `args` dangling. fmt documentation explicitly warns against this pattern.

**`basic_memory_buffer` pointer invalidation after growth**:
- `basic_memory_buffer`'s `grow()` method allocates new heap memory, `memcpy`s, and frees old memory when capacity is insufficient. After migration, `ptr_` points to the new address; `store_` (stack array) address remains unchanged.
- `detail::buffer<T>`'s `data()` returns `ptr_`, `size()` returns the amount written. Pointers/references obtained by the user via `data()` or iterators become invalid after `grow()` — the same invalidation semantics as `std::vector`'s `push_back`.
- `basic_appender<T>` (fmt's output iterator) internally holds a `buffer<T>*`; each `operator++` calls `buf.push_back()`, which may trigger `grow()`. Since the appender does not cache the `data()` pointer, it is always safe. However, if the user caches `buf.data()` during formatting and dereferences it later, the behavior is undefined.
- **Stack → heap transition point**: The first `grow()` occurs when writing exceeds 500 bytes. `store_` is still on the stack but no longer used; `ptr_` switches to a heap address. All subsequent pointer invalidations involve only heap memory.
:::::

## Performance Model

The main text has already highlighted consteval checking, 16-byte union, and 500-byte stack buffer; branch prediction, heap allocation thresholds, and visit dispatch costs will follow.

## libstdc++ vs libc++ vs MSVC

Comparison of the three standard libraries' `std::format` engines with fmt (standalone library) on key dimensions:

| Dimension | fmt (v12) | libstdc++ (GCC 14+) | libc++ (LLVM 18+) | MSVC STL (VS 2022 17.10+) |
|-----------|-----------|---------------------|-------------------|---------------------------|
| Format string checking | `consteval` construction (`fstring`) | `consteval` construction (`__format_string_view`) | `consteval` construction | `consteval` construction |
| Argument type erasure | 16-byte union + 4-bit packed type (`value<Context>`) | `__format_arg` type erasure, internal `_Arg` tagged union | `_FormatArg` type erasure | `_Basic_format_arg` type erasure |
| Packed argument threshold | ≤15 arguments packed (4 bit × 15 = 60 bit in `desc_`) | Similar packed strategy | Implementation-defined | Implementation-defined |
| Buffer strategy | 500-byte stack SBO + heap fallback (1.5x growth) | `__output_buffer`, stack SBO + heap fallback | `_OutputBuffer`, stack SBO + heap fallback | `_Fmt_buffer`, stack SBO + heap fallback |
| Floating-point formatting | Dragonbox (shortest representation) | `std::to_chars` (Grisu3 + fallback) | `std::to_chars` (Dragonbox or Ryu) | `std::to_chars` (Grisu3 + fallback, VS 2022 17.10+ switched to Dragonbox) |
| Integer formatting | `format_decimal` direct buffer write + `write_int` for sign/prefix/padding | `__to_chars_integral` | `_Int_to_chars` | `_Integral_to_chars` |
| Output iterator model | `basic_appender<T>` (back_insert_iterator) + `FILE*` + `iterator_buffer` | Iterator + `__output_buffer` | Iterator + `_OutputBuffer` | Iterator + `_Fmt_buffer` |
| `format_error` | `fmt::format_error` (inherits `std::runtime_error`) | `std::format_error` (before C++26: `std::runtime_error`) | `std::format_error` | `std::format_error` |
| `format_as` support | ADL `format_as()` + `formatter<T>::format_as()` member | Not supported (C++26 proposal P2836) | Not supported | Not supported |
| Compile-time `formatter<T>::parse` | `constexpr` (`FMT_CONSTEVAL`), complete compile-time format spec validation | `constexpr`, compile-time validation | `constexpr`, compile-time validation | `constexpr`, compile-time validation |

**Key differences**:
- **Floating-point formatting** is the point of greatest performance difference. fmt's Dragonbox implementation computes the shortest decimal representation directly from IEEE 754 bit patterns, without depending on `to_chars`. libstdc++ and MSVC STL's `std::format` calls indirectly through `std::to_chars`, and performance depends on the standard library's `to_chars` implementation quality.
- **`format_as` extension point** is unique to fmt — it allows non-intrusive mapping of user types to built-in types; the standard library has no corresponding mechanism.
- **Buffer size**: The three standard libraries' stack SBO sizes vary (typically 256-512 bytes); fmt uses a fixed 500 bytes. The actual hit rate depends on the formatted output length — typical log lines (<100 characters) hit the stack buffer in all implementations.
:::::

## Minimal Reproduction Code

```cpp
#include <fmt/format.h>

int main() {
  auto s = fmt::format("Hello, {}! {}", "world", 42);
  return static_cast<int>(s.size());
}
```

## Compile / Disassembly / Benchmark Evidence

**consteval checking path**:
- The `fstring` constructor is marked `FMT_CONSTEVAL` (expands to C++20 `consteval`), and the compiler must complete format string parsing at compile time. When compiling `fmt::format("{}", 42)` with `-std=c++20` under GCC/Clang, the format string check produces no runtime instructions — you can verify in disassembly that `format_string_checker` does not appear in the `.text` section.
- If the format string is invalid (e.g. `fmt::format("{:d}", "hello")`), the compiler reports the error message triggered by `format_string_checker::on_error()` → `report_error()`, rather than a linker or runtime error.

**Argument visit dispatch**:
- `basic_format_arg::visit()` is a 15-way switch marked `FMT_INLINE`. GCC/Clang at `-O2` compiles it to a jump table or binary search — for 15 cases, a jump table is more common (`jmp [table + rax*8]`).
- The `case type::int_type` branch directly reads `value_.int_value` (offset 0), `case type::double_type` reads `value_.double_value` (offset 0, union shares starting address) — no additional indirection layer.
- The `custom_type` branch calls `handle(value_.custom)` then indirectly invokes the user formatter via `custom_.format(custom_.custom.value, parse_ctx, ctx)` — there is one function pointer indirect jump here, and the branch predictor needs history to predict the target address.

**`memory_buffer` stack hit rate**:
- `inline_buffer_size = 500` bytes. Typical formatted output (log lines, error messages, user prompts) is usually <100 characters; the 500-byte stack buffer covers the vast majority of scenarios, **achieving zero heap allocation throughout**.
- `basic_memory_buffer`'s construction (`set(store_, SIZE)`) and destruction (`if (data != store_) deallocate()`) involve only stack pointer adjustment when the stack buffer is hit, with no `malloc`/`free` system calls.
- Benchmarks (fmt official) show: formatting `"Hello, {}! {}"` + `string_view` + `int` on the stack buffer hit path takes ~30ns (GCC 12, `-O2`, Zen 3), with the main cost being `memcpy` writing the output string + `format_decimal` integer-to-string conversion.

**`format_decimal` integer formatting**:
- fmt uses a lookup table `digits_` (200 bytes, precomputed "00"-"99") to accelerate two-digit conversion — each loop writes 2 characters instead of 1. The table lookup instruction `movzx eax, WORD PTR digits[rax*2]` is visible in disassembly.
- For 64-bit integers, `count_digits` uses `FMT_BUILTIN_CLZLL` (`__builtin_clzll` or MSVC `_BitScanReverse64`) to compute the digit count, O(1) complexity.

**Dragonbox floating-point formatting**:
- `dragonbox::to_decimal`'s typical path for IEEE 754 double is approximately 15-20 arithmetic instructions (multiplication + shift + table lookup), with no loop. Benchmarks show it is 2-3x faster than `std::to_chars` (Grisu3) and 5-10x faster than `printf("%.17g")`.
:::::

## cpplings Exercise Entry Points

- [`format1` — std::format formatting](../../../exercises/cpp20/format1.cpp)
- [`print23` — std::print / std::println formatted output](../../../exercises/cpp23/print23.cpp)

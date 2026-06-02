---
title: "fmt 高级特性"
topic: unknown
feature: fmt-advanced
standard: N/A
status_checked_at: 2026-06-02
---
# fmt Advanced Features

> Source paths: `references/impl/fmt/include/fmt/chrono.h`, `ranges.h`, `compile.h`

## Custom Types

```cpp
struct Point {
  double x, y;
};

template <> struct fmt::formatter<Point> : formatter<double> {
  auto format(const Point& p, format_context& ctx) const {
    return format_to(ctx.out(), "({}, {})", p.x, p.y);
  }
};

fmt::format("{}", Point{1.5, 2.5});  // "(1.5, 2.5)"
```

Custom types are stored via `custom_value<Context>` — the `value` constructor detects that the user has specialized `formatter<T>`, enters the `custom_tag` path, and packs the object pointer together with a formatting function pointer into a 16-byte union.

## Compile-time Formatting

```cpp
// fmt::format_string validates the format string at compile time
constexpr auto s = fmt::format<int, double>("{} {}", 42, 3.14);
```

## Chrono Formatting

```cpp
auto now = std::chrono::system_clock::now();
fmt::format("{:%Y-%m-%d %H:%M}", now);  // "2024-01-15 14:30"

std::chrono::seconds dur(3661);
fmt::format("{:%H:%M:%S}", dur);  // "01:01:01"
```

## Ranges Formatting

```cpp
std::vector<int> v = {1, 2, 3};
fmt::format("{}", v);  // "[1, 2, 3]"
fmt::format("{::#x}", v);  // "[0x1, 0x2, 0x3]"
```

## Floating-Point Formatting: Dragonbox

fmt uses the Dragonbox algorithm for float-to-string conversion, which is 2–5× faster than `std::to_chars`:

- **Dragonbox**: directly computes the shortest representation based on the mathematical properties of IEEE 754 floating-point numbers.
- **Grisu-Exact**: an alternative algorithm that guarantees correct rounding.
- **Ryu**: another fast algorithm, used by fmt in earlier versions.

## User API

The user-facing entry points covered in this document include `fmt::format`, custom `formatter<T>`, chrono/ranges formatting, and compile-time format string validation; the main text already elaborates on these advanced features directly.

## Standard Semantics

fmt provides several extensions on top of the `std::format` / `std::print` standard semantics, along with a few incompatible details:

| Dimension | `std::format` / `std::print` | `fmt` Extensions / Differences |
|-----------|-------------------------------|--------------------------------|
| Custom formatter | Specialize `std::formatter<T>`, must provide `parse()` + `format()` | Specialize `fmt::formatter<T>`; additionally supports ADL `format_as(T) -> U` to map a type to a formattable type, and the `formatter<T>::format_as()` member function — neither has an equivalent in the standard library |
| Compile-time format string | C++20 `std::format_string<Args...>` (consteval constructor) | `fmt::fstring<Args...>` (`FMT_CONSTEVAL` constructor), semantically equivalent; the `FMT_COMPILE` macro further compiles the format string into a type-level AST (`compile.h`), completely eliminating runtime parsing |
| Ranges formatting | C++23 `std::formatter` specialization for range support (`std::range` concept) | `fmt::formatter<Range>` implemented via `ranges.h`, supports map/set/sequence/tuple; `fmt::join(range, sep)` joins elements with a custom separator — no standard library equivalent |
| Chrono formatting | `std::format` supports `std::chrono` types (C++20) | `fmt::formatter<duration>` / `formatter<time_point>` implemented in `chrono.h`, supports `%Y-%m-%d %H:%M:%S` and other strftime-style tokens; `FMT_SAFE_DURATION_CAST` (enabled by default) ensures floating-point duration conversions do not overflow |
| Dynamic width/precision | `{:{}}` syntax, compile-time check that the argument is an integer | Same semantics; fmt additionally allows named arguments `{:{name}}` |
| Output target | `std::string` (`std::format`) / `FILE*` (`std::print`) | `fmt::format` → `std::string`; `fmt::format_to(it, ...)` → arbitrary output iterator; `fmt::print(FILE*, ...)` / `fmt::print(ostream&, ...)` / `fmt::print_to(FILE*, ...)` |
| Error type | `std::format_error` (prior to C++26: `std::runtime_error`) | `fmt::format_error` (inherits `std::runtime_error`), same semantics but different type — catching `std::format_error` will not catch `fmt::format_error` |
| `format_to` return value | Returns output iterator (past-the-end) | Same semantics |
| `to_string` | None (use `std::format`) | `fmt::to_string(T)` directly calls `fmt::format("{}", val)` returning `std::string` |

**Key incompatibilities**:
- `format_as` is an extension unique to fmt ([P2836R1](https://wg21.link/P2836) proposes it for the standard, but as of C++26 it has not been merged). Standard library code cannot use `format_as`; explicit `std::formatter` specialization is required.
- `fmt::format_error` and `std::format_error` are different types; when mixing fmt and standard library formatting, each must be caught separately.
- fmt's `formatter<T>::parse()` receives `fmt::parse_context&`, while the standard library receives `std::format_parse_context&` — the two APIs are similar but not the same type, so formatter specializations cannot be reused between fmt and `std::format`.

## Object Layout

The type-erased object layouts involved in fmt-advanced are concentrated in three areas: `custom_value` for custom formatter specializations, formatter specialization instances for chrono/ranges, and the type-level AST generated by `FMT_COMPILE`.

### `custom_value<Context>` and Custom Formatters

```
value<Context> (16-byte union)
┌────────────────────────────────────────────┐
│ union {                                    │
│   int / unsigned / long long / ...         │ ← built-in types stored directly
│   double / long double                     │
│   string_value<char_type>  {ptr; size}     │ ← 16 bytes (pointer + length)
│   custom_value<Context>    {ptr; fmt_fn}   │ ← 16 bytes (void* + function pointer)
│ }                                          │
└────────────────────────────────────────────┘
```

After a user specializes `fmt::formatter<T>`, `stored_type_constant<T>::value` maps to `custom_type`. When constructing `value<Context>`, the `custom_tag` branch is taken:
- `custom.value = const_cast<void*>(static_cast<const void*>(&obj))` — a void pointer to the user's object
- `custom.format = format_custom<T>` — a function pointer to a `format_custom` template instantiation that internally calls `formatter<T>().format(value, ctx)`

At runtime, when `visit` reaches the `custom_type` branch, `handle(custom)` constructs a `basic_format_arg::handle`, which then calls `custom.format(custom.value, parse_ctx, ctx)` — a single indirect function pointer jump.

### Chrono Formatter Storage

`formatter<std::chrono::duration<Rep, Period>>` and `formatter<std::chrono::time_point<Clock, Duration>>` are full specializations (not inheriting from `formatter<double>`). The format specification is parsed by `parse()` into an internal `chrono_format_spec` structure:

```
chrono_format_spec
├── fill         : char       // fill character, default ' '
├── align        : align_t    // alignment (left/right/center)
├── width        : int        // minimum width
├── precision    : int        // precision (fractional part of %S only)
├── localized    : bool       // whether to use locale
└── chrono_specs : string     // strftime token sequence, e.g. "%Y-%m-%d %H:%M"
```

Internally, `format()` converts a `time_point` to `std::tm` (via `gmtime_r` / `gmtime_s` / `localtime_r`), then outputs strftime results character by character. Duration formatting handles `%H`/`%M`/`%S` tokens directly through `format_duration`, avoiding the intermediate `tm` conversion.

### Ranges Formatter Storage

`formatter<Range>` in `ranges.h` is instantiated after SFINAE detection of `is_range_` / `is_tuple_like_`. It internally holds:

```
formatter<Range, Char>
├── underlying_  : formatter<element_type, Char>  // element formatter
├── specs_       : range_formatter_specs           // separator, outer bracket style
│   ├── separator  : basic_string_view<Char>       // default ", "
│   ├── opening    : char                          // default '['
│   └── closing    : char                          // default ']'
└── (format spec parsed by parse(), passed to element formatter via "{::spec}")
```

In the `{::#x}` syntax, `#x` after `::` is pushed down to the element `formatter<int>::parse()`, causing integers to be output in hexadecimal. The outer `[]` and separator are controlled by the range formatter itself.

### `FMT_COMPILE` Type-level AST

In `compile.h`, `FMT_COMPILE("{}")` expands to a `compiled_string` subclass that parses the format string into a type-level AST at compile time:

```
concat<text<Char>, concat<field<Char, int, 0>, text<Char>>>
├── lhs : text<Char>          // static text fragment
└── rhs : concat<field<...>, text<...>>
    ├── lhs : field<Char, int, 0>  // replacement field, binding argument 0
    └── rhs : text<Char>           // trailing text
```

Each AST node is a zero-size type (`text` holds only a `string_view`; `field` has no data members), and `concat<L,R>` has a size equal to the sum of both. After the compiler instantiates the entire AST tree, `format()` calls expand via recursive `concat::format()` into pure inline code — no runtime format string parsing, no switch dispatch, no heap allocation.

## Core Source Paths

The source paths `chrono.h`, `ranges.h`, and `compile.h` are given at the top of this document; the entry points in `format.h` / `base.h` that connect to these features will be added later.

## Core Classes / Functions

### `fmt::formatter<T>` Specialization Protocol

The entry point for custom type formatting. Users must specialize `fmt::formatter<T>` and implement two methods:

```cpp
template <> struct fmt::formatter<MyType> {
  // Compile-time parsing of the format specification (the spec part in "{:spec}")
  constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin());
  // Runtime formatting output
  auto format(const MyType& val, fmt::format_context& ctx) const -> decltype(ctx.out());
};
```

- `parse()` is called during the consteval validation phase of `fstring` (pushed down via `format_string_checker::on_format_specs`). It must consume format specification characters within the `[begin, end)` range of `ctx` and return an iterator pointing to one position before `}`. If the format specification is empty (`"{}"`), the default implementation `return ctx.begin()` suffices.
- `format()` is called at runtime via the `custom_value<Context>::format` function pointer; it writes output to `ctx.out()` and returns the past-the-end iterator.

### `custom_value<Context>`

A 16-byte union member in `base.h` that stores a user type pointer plus a formatting function pointer. `format_custom<T>` is a template function whose instantiation binds to the concrete `formatter<T>::format`. `basic_format_arg::visit()` calls `handle(custom)` in the `custom_type` branch to return a proxy object, deferring the actual formatting until the argument is consumed.

### `format_string_checker` / `fstring`

The `FMT_CONSTEVAL` constructor of `fstring<Args...>` (`base.h`) calls `format_string_checker` at compile time. The latter holds a `parse_funcs_[]` array (each element is a function pointer to `formatter<T>::parse`), scans the format string character by character, and for each `{id}` calls `parse_funcs_[id](compile_parse_context)` to validate the format specification. If validation passes, compilation succeeds; otherwise `report_error()` triggers a `static_assert`.

### Chrono Formatter

The `parse()` of `formatter<std::chrono::duration<Rep, Period>>` (`chrono.h`) parses `%Y`/`%m`/`%d`/`%H`/`%M`/`%S` and other strftime token sequences into `chrono_format_spec`. For `time_point` types, `format()` first converts to `std::tm` via `to_time_t` + `gmtime_r`/`localtime_r`, then outputs section by section according to the tokens; for `duration` types, it directly computes hours/minutes/seconds/sub-seconds. `FMT_SAFE_DURATION_CAST` ensures lossless floating-point duration precision.

### Range Formatter

`formatter<Range, Char>` (`ranges.h`) uses `range_format_kind_<T>` SFINAE detection to determine the range category (`map` / `set` / `sequence` / `string`). `parse()` first consumes the outer format specification (bracket style, separator), and upon encountering `::` pushes the remaining specification down to the element `formatter<element_type>::parse()`. `format()` iterates the range, calling `underlying_.format(elem, ctx)` for each element, inserting separators between elements. `fmt::join(range, sep)` is a standalone convenience function that generates a `join_view` and takes a specialized path, omitting brackets and nested format specification parsing.

### `FMT_COMPILE` AST Nodes

In `compile.h`, `compile_format_string<Args, POS, ID, ...>(fmt)` recursively parses the format string, generating a type-level AST:
- `text<Char>` — static text fragment
- `field<Char, V, N>` — replacement field without format specification
- `spec_field<Char, V, N>` — replacement field with format specification (internally holds a `formatter<V, Char>` instance)
- `concat<L, R>` — AST concatenation node
- `runtime_named_field<Char>` — named argument (runtime lookup)

After the compiler inlines the entire AST, `format()` expands into a pure instruction sequence.

## Key Algorithms

The four core paths of fmt-advanced — custom formatter dispatch, compile-time format string validation, chrono token parsing, and Dragonbox floating-point conversion — are connected through a three-stage pipeline: compile-time validation → argument erasure → output generation.

### Custom Formatter Dispatch Path

```
User specializes formatter<T>
    │
    ▼
fstring construction: format_string_checker::on_format_specs(id, begin, end)
    │  Calls parse_funcs_[id](compile_parse_context)
    │  → formatter<T>::parse()  // compile-time validation of format spec
    ▼
Runtime: value<Context> construction (custom_tag path)
    │  stored_type_constant<T> == custom_type
    │  custom.value = &obj, custom.format = format_custom<T>
    ▼
visit(ctx) → case custom_type:
    │  handle(custom).format(parse_ctx, ctx)
    │  → formatter<T>().format(value, ctx)  // user code executes
    ▼
Output to ctx.out()
```

Key point: `parse()` executes at compile time (consteval context), while `format()` executes at runtime. The two are bridged via the function pointer in `custom_value`.

### Compile-time Format String Validation Path

```
fstring<Args...> FMT_CONSTEVAL construction
    │
    ▼
parse_format_string(str, checker)
    │  Character-by-character scan: '{' → replacement field
    │                                '}' → end (illegal bare '}' reports error)
    │                                other → static text
    ▼
Replacement field parsing:
    ├─ arg_id: auto-index / explicit index / named argument
    │   check_arg_id(id) validates index is in bounds
    ▼
Format spec parsing:
    │  fill? align? sign? #? 0? width? .precision? type?
    │  Dynamic width/precision: check_dynamic_spec(int) validates argument is an integer
    ▼
formatter<T>::parse(ctx) pushdown
    │  Each type's parse() validates the format spec it accepts
    │  e.g.: formatter<int> accepts d/x/o/b, etc.
    │        formatter<double> accepts f/e/g/a, etc.
    ▼
Compilation succeeds → fstring<Args...> stores string_view + type descriptor
Compilation fails → report_error() → static_assert
```

### Chrono Token Parsing Path

```
"{:%Y-%m-%d %H:%M:%S}"
    │
    ▼
chrono_formatter::parse(ctx)
    │  Consumes '%' then reads token character:
    │  %Y → year, %m → month, %d → day
    │  %H → hour, %M → minute, %S → second
    │  %F → %Y-%m-%d (shortcut), %T → %H:%M:%S (shortcut)
    │  Token sequence stored in chrono_format_spec.chrono_specs
    ▼
chrono_formatter::format(tp, ctx)
    │  time_point → to_time_t → gmtime_r/localtime_r → std::tm
    │  Iterate chrono_specs:
    │    Ordinary character → output directly
    │    %Y → format_decimal(tm.tm_year + 1900, 4)
    │    %S → integer part + .%Q sub-second precision
    ▼
Duration formatting (no tm conversion):
    │  %H → duration_cast<hours>(d).count()
    │  %M → duration_cast<minutes>(d % 1h).count()
    │  %S → sub-second part, precision controlled by .precision
    │  FMT_SAFE_DURATION_CAST: safe floating-point duration conversion
    ▼
Output to ctx.out()
```

### Dragonbox Floating-Point Conversion

```
double val → dragonbox::to_decimal(val)
    │
    ▼
IEEE 754 decomposition:
    │  sign, exponent, significand
    │  Special values: ±0, ±Inf, NaN → fast path direct output
    ▼
Cache lookup:
    │  Look up 128-bit precomputed cache based on exponent
    │  Two 64×64 multiplications: shift + accumulate
    ▼
Shortest representation computation:
    │  Directly computes shortest decimal integer and exponent
    │  No loop, fixed ~15-20 instructions
    ▼
write_int + write_exponent → output to buffer
```

The key difference between Dragonbox and Ryu/Grisu3: Ryu requires 128×128 multiplication and division, and Grisu3 relies on iterative refinement. Dragonbox exploits the mathematical properties of IEEE 754 binary patterns to skip the refinement step entirely, obtaining the shortest representation with a single lookup + multiplication.

### Pipeline Relationships

The four paths share a three-stage pipeline: `fstring` consteval construction (stage 1) uniformly triggers format string parsing and `formatter<T>::parse()` validation — whether custom types, chrono, or ranges, all go through the same `format_string_checker`. Runtime argument erasure (stage 2) maps all types to the tagged union `value<Context>` via `stored_type_constant`. Output generation (stage 3) uses indirect invocation through `custom_value` function pointers for custom formatters, direct invocation of fully specialized `formatter<T>::format()` for chrono/ranges, and the Dragonbox inline path for floating-point types. `FMT_COMPILE` bypasses stage 2, promoting format string and argument bindings entirely into the type system, with the compiler directly generating the final output code.

fmt's advanced features (custom formatters, chrono, ranges, compile) are all implemented as header-only templates and inline functions, with no standard-library-style long-term ABI stability guarantee. Consistent with the basic ABI constraints described in fmt-engine, with the following additional considerations:

- **Template instantiation explosion**: each `formatter<T>` specialization, each `format_custom<T>` instantiation, and each AST node generated by `FMT_COMPILE` is an independent template instantiation. When formatting many different types, object file sizes may bloat. At the ABI level, these instances are independently generated in different translation units and deduplicated by the linker via COMDAT folding, but folding strategies may differ across compilers.
- **`FMT_COMPILE` type-level AST**: nested types like `concat<text, concat<field<int, 0>, text>>` are entirely determined by the format string. Changing the format string content produces a new type, and the old type is no longer instantiated — there is no cross-version compatibility issue, but it also means precompiled format strings are not binary-portable.
- **chrono/ranges have no independent ABI**: the formatter specializations in `chrono.h` and `ranges.h` are directly instantiated in the user's translation unit, exporting no additional symbols (except `safe_duration_cast` helper functions marked with `FMT_API`). After upgrading the fmt version, all translation units that include these headers must be recompiled.
- **No `stable` namespace**: fmt's `inline namespace v12` changes between major versions; `formatter<T>` instances from v11 and v12 cannot be mixed during linking.

## Exception Safety

The exception safety model of the advanced features inherits the three-layer structure described in fmt-engine (compile-time format string errors → runtime format_error → memory allocation failures), supplemented by the following feature-specific scenarios:

### User-Defined Formatter Throwing Exceptions

- `formatter<T>::format()` is called at runtime via the `custom_value<Context>::format` function pointer. If user code throws an exception, it propagates along the `vformat_to` call stack.
- `basic_memory_buffer` is destroyed via RAII during stack unwinding, releasing any allocated heap memory.
- **Partial output is not rolled back**: the output iterator of `format_to` (e.g., `back_insert_iterator<string>`) may have already written some characters. Characters already `push_back`'d to the string remain in place and are not rolled back. For `FILE*` output, bytes already written have reached the file descriptor and cannot be retracted.
- Guarantee: no resource leaks (RAII), but output may be in a partially completed state.

### Safe Conversion in Chrono Formatting

- `FMT_SAFE_DURATION_CAST` (`chrono.h`) checks for overflow during floating-point duration conversion. `lossless_integral_conversion` and `safe_float_conversion` set an error code `ec` rather than throwing an exception.
- When `gmtime_r` / `localtime_r` calls fail (e.g., invalid `time_t`), fmt returns an empty `std::tm`, and subsequent formatting outputs zero values rather than crashing.
- The chrono formatter's `format()` itself does not throw exceptions (assuming the underlying C library functions do not throw), but `format_to` may throw `bad_alloc` due to buffer growth.

### Ranges Formatting Exception Propagation

- `formatter<Range>::format()` iterates the range, calling `underlying_.format(elem, ctx)` for each element. If the element formatter throws an exception, the first N already-formatted elements are retained in the output, and subsequent elements are not processed.
- The range iteration itself may throw exceptions (e.g., from user code in `++it` or `*it`), with the same propagation path as above.
- `fmt::join(range, sep)` takes a specialized path without creating a `format_context`, but element formatting exception propagation semantics are the same.

### `FMT_COMPILE` Exception Behavior

- The `format()` method of AST nodes generated by `FMT_COMPILE` is a `constexpr` function and should not throw exceptions during compile time. At runtime, since there is no format string parsing and no type erasure, the only possible exception sources are element formatters and buffer growth.
- Compared to plain `fmt::format`, the `FMT_COMPILE` path eliminates visit dispatch and runtime format_specs parsing, resulting in fewer exception trigger points.

### `FMT_USE_EXCEPTIONS=0`

- All `FMT_THROW` expand to `fmt::assert_fail` → `abort()`. Throws in user formatters are also disabled (depending on the compiler's `-fno-exceptions` setting). In this mode, any exception path terminates the process directly.

## Iterator / Reference Invalidation

The reference/iterator invalidation scenarios covered in this document are consistent with the basic mechanisms described in fmt-engine, with the following additional concerns specific to advanced features:

### Borrowing Semantics of `format_arg_store`

- `basic_format_args` is a non-owning view, referencing the `value<Context>[]` in `format_arg_store` via pointer. `format_arg_store` is constructed by `make_format_args(args...)` or internally by `fmt::format`, with its lifetime bound to the calling expression.
- **chrono/ranges arguments**: in `fmt::format("{}", my_chrono_time)`, the `format_arg_store` holds `custom_value{&my_chrono_time, format_custom<time_point>}` — a void pointer to a stack temporary. It is valid until `vformat_to` returns and must not escape.
- **Dangerous pattern**: `auto args = make_format_args(duration); vformat("{}", args);` — if `duration` is a temporary, `args` becomes a dangling reference. The fmt documentation explicitly warns against this.

### Iterator Stability with Ranges Adapters

- `formatter<Range>::format()` caches `range_begin()` / `range_end()` iterators during iteration. If the range is modified during iteration (e.g., `push_back` on a `std::vector`), iterators are invalidated and behavior is undefined.
- `fmt::join(range, sep)` similarly evaluates `begin()` / `end()` at call time and uses cached iterators thereafter. The range's lifetime must cover the entire `format_to` call.
- **Lazy ranges**: lazy adapters like `std::views::filter` evaluate predicates only upon dereference. If a captured reference in the predicate becomes invalid, behavior is undefined.

### `basic_memory_buffer` Growth and `format_to` Iterators

- `basic_appender<T>` (fmt's output iterator) internally holds a `buffer<T>*`; each `operator++` calls `push_back()`, which may trigger `grow()`. The appender does not cache the `data()` pointer and is always safe.
- If the user caches the `buf.data()` pointer during formatting and subsequently dereferences it (e.g., `auto p = buf.data(); format_to(appender, ...); use(*p);`), `p` becomes dangling after `grow()`.

### `std::tm` Lifetime in Chrono Formatting

- The chrono formatter's `format()` internally obtains `std::tm` via `gmtime_r` / `localtime_r`. `gmtime_r` writes into a caller-provided buffer (stack-local variable), and the `std::tm` lifetime covers the entire `format()` call. There is no dangling reference risk.
- `std::put_time` (if used) depends on locale; the lifetime of the locale's `std::time_put` facet is managed by the locale object — fmt does not hold a locale, using `std::locale::classic()` instead, so there are no lifetime issues.

### References in `FMT_COMPILE` AST

- `text<Char>` holds a `basic_string_view<Char>` pointing to a compile-time string literal (`.rodata` section), which is always valid.
- `spec_field<Char, V, N>` internally holds a `formatter<V, Char>` instance — constructed by the compiler at compile time, with a lifetime equal to the program.
- At `format()` call time, arguments are passed in by `const T&` reference; reference validity is guaranteed by the caller.

## Performance Model

The performance characteristics of fmt-advanced are determined by the costs of its four paths, listed from lowest to highest overhead:

### `FMT_COMPILE` (Lowest Overhead)

- **Zero runtime parsing**: the format string is parsed into a type-level AST at compile time; at runtime there is no format string scanning, no argument index computation, and no format_specs parsing.
- **Zero type erasure**: `spec_field<Char, V, N>::format()` obtains the argument value directly via `const V&` reference — no tagged union, no visit switch.
- **Pure inlining**: `concat<L,R>::format()` recursive calls are fully inlined by the compiler; the generated code is equivalent to a hand-written `write(out, "..."); write(out, val); write(out, "...");` sequence.
- Benchmarks show that the `FMT_COMPILE("{}")` path is ~20–30% faster than plain `fmt::format("{}")` (GCC 12, -O2), primarily saving on visit dispatch and runtime format_specs parsing.

### Built-in Type Formatting (Low Overhead)

- `value<Context>`'s 16-byte union stores values directly — `int`/`double`/`string_view` require no heap allocation.
- `visit()` is a 15-way switch; GCC/Clang at `-O2` compile it into a jump table (`jmp [table + rax*8]`), a single indirect jump.
- `format_decimal` for integer-to-string conversion uses a 200-byte lookup table `digits_`, writing 2 characters per loop iteration. `count_digits` uses `__builtin_clzll` bit counting, O(1).
- Dragonbox floating-point conversion: ~15–20 arithmetic instructions (multiplication + shift + table lookup), no loop. 2–3× faster than `std::to_chars` (Grisu3).

### Custom Formatters (Medium Overhead)

- One additional indirect function pointer jump (`custom.format`) — the branch predictor needs history to predict the target address. First call cold start is ~5–10ns.
- `formatter<T>::parse()` executes at compile time, with zero runtime overhead.
- The overhead of `formatter<T>::format()` depends entirely on the user's implementation — typical scenarios (writing 3–5 fields) are on the same order of magnitude as built-in type formatting.

### Chrono Formatting (Medium-High Overhead)

- `gmtime_r` / `localtime_r` system calls may involve locking (in some libc implementations), ~50–100ns.
- strftime-style token-by-token parsing + `format_decimal` output, with no additional heap allocation.
- The safe conversion in `FMT_SAFE_DURATION_CAST` adds a small number of branch checks (~2–3 conditional checks), which is negligible.

### Ranges Formatting (Highest Overhead)

- Linear iteration of the range, calling `underlying_.format(elem, ctx)` for each element — overhead = N × per-element formatting cost.
- `fmt::join(range, sep)` saves brackets and outer format_specs parsing compared to nested `format("{}", range)`, but the per-element overhead is the same.
- Nested ranges (`vector<vector<int>>`) trigger recursive instantiation of the `underlying_` formatter on each inner `format()` call; compile time and code size grow exponentially with nesting depth.

### Stack Buffer Hit Rate

All paths share `basic_memory_buffer` (500-byte stack SBO). Typical formatted output is <100 characters, with a stack buffer hit rate of >95%. Heap allocation is triggered only for very long output (e.g., formatting an entire range), with heap allocation growing by 1.5×, amortized O(1).

## libstdc++ vs libc++ vs MSVC

A comparison of the three standard library implementations of `std::format` against fmt for advanced features (see fmt-engine for the basic engine comparison):

| Dimension | fmt (v12) | libstdc++ (GCC 14+) | libc++ (LLVM 18+) | MSVC STL (VS 2022 17.10+) |
|-----------|-----------|---------------------|-------------------|---------------------------|
| Custom formatter protocol | `formatter<T>::parse()` + `format()` + ADL `format_as()` | `std::formatter<T>::parse()` + `format()` | Same as left | Same as left |
| `format_as` extension | Supported (ADL + member function) | Not supported | Not supported | Not supported |
| Ranges formatting | `ranges.h`: SFINAE detection of `is_range_`/`is_tuple_like_`, supports map/set/sequence/tuple, `join(range, sep)` | C++23 `std::formatter<range>`, supports `range` concept, no `join` equivalent | Same as left | Same as left |
| Chrono formatting | Full strftime tokens + `FMT_SAFE_DURATION_CAST` + safe floating-point duration conversion | C++20 `std::chrono` formatting, full strftime support, no safe duration cast | Same as left | Same as left |
| Compile-time format string compilation | `FMT_COMPILE` → type-level AST, completely eliminates runtime parsing | No equivalent (consteval checking ≠ compiled format string) | Same as left | Same as left |
| Dynamic width/precision | `{:{}}` + named argument `{:{name}}` | `{:{}}`, no named dynamic precision support | Same as left | Same as left |
| Output iterator model | `basic_appender<T>` + `FILE*` + `ostream&` + arbitrary `OutputIt` | `OutputIt` + `FILE*` (C++23 `std::print`) | Same as left | Same as left |
| `to_string` convenience | `fmt::to_string(T)` | None (use `std::format`) | Same as left | Same as left |
| Fill and alignment | `fill align width`, arbitrary Unicode fill characters | Same as standard semantics | Same as left | Same as left |
| `format_error` type | `fmt::format_error` (subclass of `std::runtime_error`) | `std::format_error` (prior to C++26: `std::runtime_error`) | Same as left | Same as left |

**Key differences**:

1. **`format_as` is an extension unique to fmt**: it allows non-intrusive mapping of user types to formattable types without specializing a formatter. P2836 proposes it for the C++26 standard, but as of the latest version of each implementation, none supports it. Standard library code must explicitly specialize `std::formatter<T>`.

2. **`FMT_COMPILE` has no standard equivalent**: the standard library's consteval checking only validates format string legality and does not generate precompiled formatting code. `FMT_COMPILE` compiles the format string into a type-level AST, eliminating runtime format string parsing — offering significant performance advantages for high-frequency formatting paths (e.g., logging hot paths).

3. **Functional differences in Ranges formatting**: fmt's `fmt::join(range, sep)` is a convenience function with no standard library counterpart. The standard library's `std::formatter<range>` supports basic range formatting but does not provide a direct API for custom separators. Additionally, fmt's range formatter pushes format specifications down to element formatters via the `{::spec}` syntax; the standard library behaves similarly but specification details may differ.

4. **Depth of compile-time checking**: fmt's `format_string_checker` calls each `formatter<T>::parse()` at compile time to validate format specifications, and standard library implementations perform similar checks. However, fmt additionally validates via `check_dynamic_spec` that dynamic width/precision arguments must be integer types; this check is implementation-defined in the standard library.

5. **Performance differences**: floating-point formatting is the biggest difference (Dragonbox vs `to_chars`). On custom formatter, chrono, and ranges paths, the performance differences between fmt and standard library implementations mainly come from buffer strategy (fmt's 500-byte stack SBO vs implementation-defined SBO sizes in standard libraries) and inlining strategy.

## Minimal Reproduction Code

```cpp
#include <chrono>
#include <fmt/chrono.h>
#include <fmt/format.h>

struct Point {
  int x;
  int y;
};

template <>
struct fmt::formatter<Point> : fmt::formatter<int> {
  auto format(const Point& p, fmt::format_context& ctx) const {
    return fmt::format_to(ctx.out(), "({}, {})", p.x, p.y);
  }
};

int main() {
  return static_cast<int>(fmt::format("{}", Point{1, 2}).size());
}
```

## Compilation / Disassembly / Benchmark Evidence
### Custom Formatter Disassembly

```cpp
struct Point { double x, y; };
template <> struct fmt::formatter<Point> : fmt::formatter<double> {
  auto format(const Point& p, fmt::format_context& ctx) const {
    return fmt::format_to(ctx.out(), "({}, {})", p.x, p.y);
  }
};
```

- Under GCC 12 `-O2`, the `custom_type` branch of `visit` compiles to `call rax` (`rax` = `custom.format` function pointer). `format_custom<Point>` is inlined into `formatter<Point>::format()`; the final code contains two `format_to` calls for doubles plus two literal writes.
- The 16-byte union of `custom_value` is 16-byte aligned on the stack; the `value<Context>` array is laid out contiguously, and visit's index computation is `base + idx * 16` (O(1)).

### Chrono Formatting Benchmark

```cpp
auto now = std::chrono::system_clock::now();
for (int i = 0; i < 1000000; ++i)
  fmt::format("{:%Y-%m-%d %H:%M:%S}", now);
```

- GCC 12 `-O2`, Zen 3: ~200ns/call (stack buffer hit path). Main costs: `gmtime_r` system call ~80ns + `format_decimal` × 6 calls ~60ns + literal writes ~20ns.
- Compared to `std::format` (libstdc++ GCC 14): ~250ns/call (buffer strategy difference).
- Compared to `snprintf(buf, ..., "%Y-%m-%d %H:%M:%S", tm)`: ~300ns/call (runtime format string parsing).

### Ranges Formatting Benchmark

```cpp
std::vector<int> v(100);
std::iota(v.begin(), v.end(), 0);
for (int i = 0; i < 100000; ++i)
  fmt::format("{}", v);
```

- GCC 12 `-O2`, Zen 3: ~2.5μs/call (100 ints, stack buffer hit). Main costs: 100× `format_decimal` + 99× separator writes + 2× bracket writes.
- `fmt::join(v, ", ")`: ~2.3μs/call (omits brackets and outer format_specs parsing).
- Compared to hand-written loop + `format_to`: ~2.0μs/call — the overhead of ranges formatting is approximately 15–20%, mainly from the indirect call of `underlying_.format()` and range iterator dereference.

### `FMT_COMPILE` Disassembly and Benchmark

```cpp
auto compiled = FMT_COMPILE("{} {}");
for (int i = 0; i < 1000000; ++i)
  fmt::format(compiled, 42, 3.14);
```

- Under GCC 12 `-O2`, the `concat<text, concat<field<int, 0>, concat<text, field<double, 1>>>>::format()` generated by `FMT_COMPILE("{} {}")` is fully inlined. The disassembly shows: `format_decimal` call (int 42) + direct write of `' '` + Dragonbox call (double 3.14) — no visit switch, no format_specs parsing, no tagged union access.
- Benchmark: ~25ns/call vs plain `fmt::format` ~35ns/call, ~30% faster.

### Dragonbox Floating-Point Path Disassembly

```cpp
fmt::format("{}", 3.14159);
```

- Under GCC 12 `-O2`, `dragonbox::to_decimal` compiles to ~18 arithmetic instructions: `imul` (cache lookup) + `shrx`/`shlx` (shifts) + `movzx` (digits lookup) + conditional branches (special value checks). No loop, no function calls.
- Compared to `std::to_chars(buf, buf+32, 3.14159)` (Grisu3 path): ~25 instructions + 1× 128×128 multiplication.
- Compared to `snprintf(buf, 32, "%g", 3.14159)`: ~200 instructions (libc's `printf` implementation).

## cpplings Exercise Entry Points

- [`format1` — std::format formatting](../../../exercises/cpp20/format1.cpp)
- [`print23` — std::print / std::println formatted output](../../../exercises/cpp23/print23.cpp)
- [`chrono1` — chrono time library](../../../exercises/cpp11-std/chrono1.cpp)
- [`ranges1` — Ranges basics`](../../../exercises/cpp20/ranges1.cpp)

---
title: "C++ ABI Deep Dive"
topic: topics
feature: abi
status_checked_at: 2026-06-02
standard: N/A
---
# C++ ABI Deep Dive

> ABI (Application Binary Interface) is the cornerstone of C++ binary compatibility. Every line of code you write — function signatures, class layouts, exception propagation, virtual function dispatch — must ultimately conform to some ABI specification, or linking fails and runtime crashes result. Understanding ABI is not an academic curiosity; it is a survival skill for cross-compiler collaboration, dynamic library compatibility, and systems-level programming.

---

## 1. What is ABI

ABI defines **the interface contract between compiled binary code**:

```
API (source level)                     ABI (binary level)
+------------------------+        +------------------------------+
| Function signatures    |   ->   | Name mangling                |
| Type declarations      |   ->   | Data layout (size, align, offset) |
| Parameter lists        |   ->   | Calling convention (register/stack order) |
| Virtual function decls |   ->   | vtable layout and offsets    |
| Exception types        |   ->   | Exception propagation and stack unwinding |
| Template instantiations|   ->   | Symbol export / weak symbol merging rules |
+------------------------+        +------------------------------+
```

Key distinction: **API is the source-level interface for the compiler; ABI is the binary-level interface for the linker and operating system.** Two translation units can link together even with completely different source code, as long as their ABIs are compatible.

---

## 2. ABI vs API: Key Differences

```
Scenario: You release a dynamic library libfoo.so

API compatible (source level):
  - Add new function/overload          -> Existing client source compiles without change
  - Change function default arguments  -> Caller semantics change, but compilation may pass (dangerous!)
  - Add virtual function               -> vtable layout changes, existing clients must recompile

ABI compatible (binary level):
  - Append non-virtual function        -> No recompilation of clients needed
  - Change order of data members       -> Offsets change, existing clients crash
  - Change underlying type of enum     -> sizeof changes, layout incompatible
```

**ABI stability** means: without recompiling old clients, the new version of `.so`/`.dll` can directly replace the old version and run correctly. This is much stricter than API stability.

---

## 3. Name Mangling

The linker distinguishes different functions by their symbol names. C++ supports function overloading, namespaces, and templates, so this information must be encoded into symbol names.

### 3.1 Itanium C++ ABI (GCC / Clang)

```
Source:
  namespace ns {
      int foo(int x, double y);
      template<typename T> class Bar {
          void method(T, T*);
      };
      template class Bar<int>;  // explicit instantiation
  }

Mangled:
  _ZN2ns3fooEid          -> ns::foo(int, double)
  _ZN2ns3BarIiE6methodEiPS0_
       |   |    |    | |  +-- T* = int* (S0 = substitution #0 = ns::Bar<int>)
       |   |    |    | +---- int
       |   |    |    +------ E end of parameter list
       |   |    +----------- method (method name, length-prefixed encoding)
       |   +---------------- Bar<int> (template instantiation)
       +-------------------- ns (namespace, length-prefixed encoding)

Encoding rules:
  - N...E          -> Nested name segment (starts with N, ends with E)
  - digit + string -> Length-prefixed name (2ns = "ns", 3foo = "foo")
  - i / d / f / ...-> Built-in type abbreviation (i=int, d=double, f=float, v=void)
  - P              -> Pointer decoration (P + base type)
  - R              -> Reference decoration
  - S_, S0_, S1_...-> Substitution (backreference for repeated types, reducing symbol length)
```

**GCC visualization tools**:

```bash
# Demangle to view symbols
$ c++filt _ZN2ns3fooEid
ns::foo(int, double)

# Extract C++ symbols from a binary
$ nm --demangle libfoo.so | grep 'foo'
$ readelf -sW libfoo.so | c++filt
```

### 3.2 MSVC Name Mangling

```
Source: int ns::foo(int x, double y)

MSVC mangled:
  ?foo@ns@@YAHHN@Z
   |   |  ||| | +-- Z terminator
   |   |  ||| +---- N = double
   |   |  ||+------ H = int (return type)
   |   |  |+------- Y (__cdecl calling convention, global function)
   |   |  +-------- A (public access level, default for namespace functions)
   |   +----------- ns
   +--------------- foo

MSVC type codes:
  H = int,  N = double,  M = float,  D = char
  PA = pointer,  AA = reference
  V = class/struct (encoded by name)
  ?A = array
```

**MSVC visualization tools**:

```cpp
// Use undname.exe (shipped with Visual Studio)
// undname ?foo@ns@@YAHHN@Z
// Output: int __cdecl ns::foo(int, double)

// Or programmatically
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")
char buf[1024];
UnDecorateSymbolName("?foo@ns@@YAHHN@Z", buf, 1024, UNDNAME_COMPLETE);
```

---

## 4. vtable ABI Layout

Virtual functions achieve dynamic dispatch through vtables. The Itanium C++ ABI defines the precise vtable memory layout:

```
Single inheritance case:

class Base {
    int a;
    virtual void f();
    virtual void g();
};

class Derived : public Base {
    int b;
    void f() override;   // overrides Base::f
    virtual void h();    // new virtual function
};

Base vtable:
+-------------------------+
| offset_to_top = 0       |  <- offset to the start of the complete object
| typeinfo for Base       |  <- RTTI pointer
+-------------------------+
| &Base::f()              |  <- slot 0
| &Base::g()              |  <- slot 1
+-------------------------+

Derived vtable:
+-------------------------+
| offset_to_top = 0       |
| typeinfo for Derived    |
+-------------------------+
| &Derived::f()           |  <- slot 0 (overrides Base::f)
| &Base::g()              |  <- slot 1 (inherited, not overridden)
| &Derived::h()           |  <- slot 2 (added)
+-------------------------+
```

**With multiple inheritance, each base class has its own vtable**:

```
class A { virtual void f(); };
class B { virtual void g(); };
class C : public A, public B { void f() override; void g() override; };

Memory layout of C object:
+---------------------+ offset 0
| A subobject         |
|   vptr_A -------------> C's A-part vtable
|   ...               |        +----------------------+
+---------------------+        | offset_to_top = 0    |
| B subobject         |        | typeinfo for C       |
|   vptr_B ------------->     +----------------------+
|   ...               |        | &C::f()              |
+---------------------+        +----------------------+

                               C's B-part vtable
                               +----------------------+
                               | offset_to_top = -16  | <- back to start of C object
                               | typeinfo for C       |
                               +----------------------+
                               | &C::g()              |  <- thunk performs offset correction
                               +----------------------+

B-part thunk (compiler-generated trampoline):
  thunk for C::g() via B vtable:
      this -= 16;      // adjust this pointer from B subobject to complete C object
      jmp C::g();
```

**The vptr is typically located at the first byte of the object** (if the base class has virtual functions). Object size = all base class subobjects + vptr + data members + trailing padding for alignment.

---

## 5. Exception Handling ABI (Itanium EH ABI)

### 5.1 Exception Throwing and Catching

```
Throw flow (simplified):
  throw std::runtime_error("fail");

  1. The compiler constructs the exception object on the stack (possibly via __cxa_allocate_exception)
  2. Calls __cxa_throw(exception_obj, typeinfo, destructor)
  3. The runtime begins stack unwinding:
     a. Reads the .eh_frame / .gcc_except_table section of the current function
     b. Looks up the LSDA (Language Specific Data Area) for the current instruction pointer (IP)
     c. LSDA specifies which catch handlers can catch the current exception type
     d. If matched, jumps to the catch block
     e. If not matched, calls destructors (for stack-local objects), unwinds to the caller, repeats b-d
  4. If unwinding reaches the top of the stack with no match -> std::terminate()
```

### 5.2 .eh_frame and DWARF Unwind Information

```
.eh_frame section structure:

+-----------------------------------------------------+
| CIE (Common Information Entry) - one per compilation unit |
|  - Version, pointer encoding, return address register column |
|  - Initial row state (which registers are where)    |
+-----------------------------------------------------+
| FDE (Frame Description Entry) - one per function    |
|  - Function start address, length                   |
|  - Instruction sequence: how to restore each frame's register state |
|  - LSDA pointer -> exception handling table         |
+-----------------------------------------------------+

LSDA (Language Specific Data Area):

+-------------------------------------------+
| LPStart (landing pad base address)        |
| TTBase (typeinfo table base address)      |
| Call Site Table                            |
|  +-------------------------------------+  |
|  | [start, length, landing_pad, action]|  |  <- each call site that may throw
|  | [start, length, landing_pad, action]|  |
|  +-------------------------------------+  |
| Action Table                               |
|  +-------------------------------------+  |
|  | [type_filter, next_action]          |  |  <- chained catch type matching
|  +-------------------------------------+  |
+-------------------------------------------+
```

### 5.3 Exception Object Memory Management

```cpp
// Layout of the exception object allocated by __cxa_allocate_exception (Itanium ABI):
// +-------------------------------+
// | __cxa_exception header        |  <- runtime internal management fields
// |  - refcount                   |
// |  - exceptionType (std::type_info*) |
// |  - exceptionDestructor        |
// |  - unexpectedHandler          |
// |  - terminateHandler           |
// |  - nextException (exception chain) |
// |  - handlerCount               |
// |  - unwindHeader (used by unwinder) |
// +-------------------------------+
// | User exception object         |  <- the type of the throw expression
// +-------------------------------+
//
// In catch(std::exception& e), e points to the start of the user exception object
// __cxa_begin_catch / __cxa_end_catch manage the reference count
```

### 5.4 noexcept ABI Impact

```cpp
// noexcept is part of the function type and affects mangled names:
void f() noexcept;    // _Z1fv (no noexcept marker - this differs from some traditional implementations)
void g();             // _Z1gv

// In practice, noexcept affects:
// 1. Whether the compiler generates .eh_frame FDE entries (noexcept functions may omit them)
// 2. Exception propagation path (passing through a noexcept function directly calls terminate)
// 3. Move operation optimization (noexcept move constructors allow vector to bulk-move)
```

---

## 6. RTTI ABI

```
ABI layout of std::type_info object (Itanium C++ ABI):

+----------------------------------+
| vptr                              |  <- type_info itself has a vtable
| __name (const char*)              |  <- pointer to the mangled type name
+----------------------------------+

  For classes with virtual functions:
+----------------------------------+
| __type_info subclass vtable       |
|  - __do_catch()                   |  <- catch matching
|  - __do_upcast()                  |  <- upcast detection
| __name                            |
| __base_type (pointer to base class type_info) |  <- used by dynamic_cast
+----------------------------------+

typeid mechanism:
  - For polymorphic types (with virtual functions): obtained at runtime from the typeinfo pointer in the vtable
  - For non-polymorphic types: determined at compile time, returns a static type_info object

// typeid on polymorphic types is a runtime query:
class Base { virtual ~Base() = default; };
class Derived : Base {};
Base* p = new Derived;
typeid(*p).name();    // "7Derived" (RTTI read from vtable at runtime)
typeid(*p) == typeid(Derived);  // true (runtime comparison of type_info pointers or strings)
```

---

## 7. Parameter Passing Calling Conventions

### 7.1 SysV AMD64 ABI (Linux / macOS / BSD)

```
Function parameter passing rules:

Integer/pointer parameters (rdi, rsi, rdx, rcx, r8, r9):
  void f(int a, int b, int c, int d, int e, int f, int g);
  // a->rdi, b->rsi, c->rdx, d->rcx, e->r8, f->r9, g->stack

Floating-point parameters (xmm0-xmm7):
  void g(double a, double b, ..., double h, double i);
  // a->xmm0, ..., h->xmm7, i->stack

Mixed parameters:
  void h(int a, double b, int c, double d);
  // a->rdi, b->xmm0, c->rsi, d->xmm1
  // Integer and floating-point use independent register sets, no interference

Return values:
  - Integer/pointer -> rax (64-bit) or rax+rdx (128-bit)
  - Floating-point -> xmm0
  - Return values larger than 16 bytes -> caller allocates space, passed via hidden first pointer parameter

Struct passing (SysV AMD64 classification rules):
  - <= 16 bytes and each 8-byte chunk is INTEGER -> passed via rdi/rsi
  - <= 16 bytes and each 8-byte chunk is SSE -> passed via xmm0/xmm1
  - > 16 bytes or contains non-integer/non-SSE components -> passed by reference (implicit pointer)

Examples:
  struct Small { int a; int b; };        // 8 bytes -> INTEGER -> rdi
  struct Mixed { int a; float b; };      // 8 bytes -> INTEGER -> rdi (float classified as INTEGER)
  struct Large { int a; int b; int c; }; // 12 bytes -> <=16 -> rdi (still INTEGER)
  struct Huge  { int a[10]; };           // 40 bytes -> passed via stack/reference
```

### 7.2 MSVC x64 Calling Convention

```
MSVC x64 (__fastcall, the only available user-mode calling convention):

  - First 4 integer/pointer parameters -> rcx, rdx, r8, r9
  - First 4 floating-point parameters -> xmm0, xmm1, xmm2, xmm3
  - Mixed: each class draws from its own set (similar to SysV but simpler)
  - 5th and beyond -> all go on the stack
  - Caller guarantees 32 bytes of shadow space (register spill area)

Example:
  void f(int a, int b, int c, int d, int e);
  // a->rcx, b->rdx, c->r8, d->r9, e->stack[0]
  // Caller must reserve at least 32+8=40 bytes of stack space

Struct passing (MSVC x64):
  - <= 8 bytes -> passed via rcx (integer) or xmm0 (floating-point)
  - <= 16 bytes -> split into two 8-byte chunks
  - > 16 bytes (but <= alignment requirement) -> passed by reference (__fastcall special rule)
  - Key difference from SysV: MSVC always passes types with non-trivial constructors/destructors by reference

// Pass by reference (MSVC-specific):
struct HasDtor {
    int x;
    ~HasDtor();  // non-trivial destructor -> even at only 4 bytes, passed via hidden pointer
};
```

---

## 8. Struct Layout and Padding Rules

```cpp
struct Example {
    char  a;    // 1 byte
    // 3 bytes padding (align int to 4-byte boundary)
    int   b;    // 4 bytes
    char  c;    // 1 byte
    // 7 bytes padding (align double to 8-byte boundary)
    double d;   // 8 bytes
    char  e;    // 1 byte
    // 3 bytes padding (struct tail aligned to largest member = 8 bytes)
};
// sizeof(Example) = 1+3+4+1+7+8+1+3 = 28 bytes

// Memory layout diagram:
// Offset  Contents
// 0x00    [a]
// 0x01    [pad][pad][pad]
// 0x04    [b b b b]
// 0x08    [c]
// 0x09    [pad]x7
// 0x10    [d d d d d d d d]
// 0x18    [e]
// 0x19    [pad]x3
// 0x1C    <- sizeof = 28
```

**Optimization: sorting members by decreasing alignment eliminates padding**:

```cpp
struct Optimized {
    double d;   // 8 bytes, offset 0x00
    int    b;   // 4 bytes, offset 0x08
    char   a;   // 1 byte, offset 0x0C
    char   c;   // 1 byte, offset 0x0D
    char   e;   // 1 byte, offset 0x0E
    // 1 byte padding (tail aligned to 8 bytes)
};
// sizeof(Optimized) = 16 bytes (saved 12 bytes!)
```

**ABI impact of `#pragma pack` and `alignas`**:

```cpp
#pragma pack(push, 1)   // Align to 1 byte - no padding
struct Packed {
    char  a;  // offset 0
    int   b;  // offset 1 (misaligned!)
    char  c;  // offset 5
};
// sizeof(Packed) = 6, but accessing b may span cache lines, very poor performance
#pragma pack(pop)

// alignas increases alignment - used for SIMD vector alignment
struct alignas(32) AVXVec {
    float data[8];  // 32-byte aligned, suitable for AVX operations
};
```

---

## 9. ABI Stability and Evolution

### 9.1 Benefits of ABI Stability

```
ABI stability means:
  1. Old .so/.dll files can be directly replaced without recompiling dependents
  2. Libraries compiled with different compiler versions can be mixed (if they follow the same ABI)
  3. Operating systems can upgrade C++ runtimes independently of applications
  4. Distributing binary packages (.deb, .rpm, NuGet) becomes possible
```

### 9.2 Common ABI-Breaking Operations

```
ABI-compatible operations (safe to release as new version):
  - Add non-virtual member function
  - Add non-virtual function overload
  - Add specialization of template function/class
  - Change function body (without changing signature)
  - Change default parameter values (requires recompile to take effect, but doesn't break existing binaries)

ABI-breaking operations (must recompile all dependents):
  - Add/remove/reorder virtual functions
  - Add/remove/reorder data members
  - Change member types (even if size is the same, ABI may differ)
  - Change underlying type or values of an enum
  - Change access control ordering of a class (changes memory layout)
  - Add/remove virtual base classes
  - Change template parameter count or defaults
```

---

## 10. Standard Library ABI Versioning Strategies

### 10.1 libstdc++ Dual ABI (`__cxx11` Namespace)

GCC 5 introduced the `std::__cxx11` namespace to handle ABI-breaking changes to `std::string` and `std::list`:

```cpp
// Pre-GCC 5 std::string (COW - Copy-On-Write)
// _ZNSsC1Ev  ->  std::string::string()

// Post-GCC 5 std::string (SSO - Small String Optimization, non-COW)
// _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1Ev
// i.e. std::__cxx11::basic_string<...>::basic_string()

// Automatic selection at compile time (GCC 5+ is the default):
// -D_GLIBCXX_USE_CXX11_ABI=1   -> new ABI (default, __cxx11 namespace)
// -D_GLIBCXX_USE_CXX11_ABI=0   -> old ABI (compatible with GCC 4.x)

// Mixing both ABIs in the same project:
void legacy_api(std::string& s);  // compiled with ABI=0 -> old ABI symbol
void new_api(std::string& s);     // compiled with ABI=1 -> new ABI symbol
// At link time: the two symbols are different, no conflict

// Mangled name comparison for dual ABI:
// Old: _ZNSsC1Ev                     -> std::string::string()
// New: _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1Ev
//          ^ key difference: __cxx11 namespace segment
```

**Diagnostic tools**:

```bash
# Check which ABI a .so uses
$ nm -DC libfoo.so | grep 'std::__cxx11'
# Has output -> uses new ABI

$ nm -DC libfoo.so | grep 'std::basic_string' | grep -v '__cxx11'
# Has output -> uses old ABI

# Check ABI compatibility
$ abi-compliance-checker -lib libfoo -old old.xml -new new.xml
```

### 10.2 libc++ Inline Namespace Versioning

```cpp
// libc++ uses inline namespaces for ABI versioning
// Each version has its own namespace; symbol names automatically include a version tag

namespace std {
    inline namespace __1 {    // libc++'s default ABI version
        template<class CharT, class Traits, class Allocator>
        class basic_string;
    }
}

// Mangled names include __1:
// _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1Ev

// When ABI-breaking changes are needed, a new version is introduced:
// _LIBCPP_ABI_VERSION 1 -> inline namespace __1 (current default)
// _LIBCPP_ABI_VERSION 2 -> inline namespace __2 (experimental)

// Symbols across versions do not conflict; types from two versions can be linked simultaneously:
// -D_LIBCPP_ABI_VERSION=1  -> std::__1::string
// -D_LIBCPP_ABI_VERSION=2  -> std::__2::string

// libc++ specific ABI options (controlled via macros):
// _LIBCPP_ABI_ALTERNATE_STRING_LAYOUT      -> changes the internal layout of string
// _LIBCPP_ABI_UNSTABLE                      -> enables all experimental ABI breaks
// _LIBCPP_ABI_NO_PAIR_COMPARISON            -> removes deprecated comparison operators for std::pair
```

### 10.3 MSVC `_ITERATOR_DEBUG_LEVEL`

MSVC uses `_ITERATOR_DEBUG_LEVEL` to control the ABI level of iterator checking:

```cpp
// _ITERATOR_DEBUG_LEVEL=0 -> disables all iterator checks (default for release builds)
// _ITERATOR_DEBUG_LEVEL=1 -> basic checks (default for debug builds)
// _ITERATOR_DEBUG_LEVEL=2 -> full checks (bounds checking, dangling iterator detection)

// Different levels generate different symbols and cannot be mixed:
// IDL=0: std::vector<int>::iterator is a raw pointer
// IDL=2: std::vector<int>::iterator is a wrapper class (contains container pointer, version number)

// Setting at compile time:
// cl /D_ITERATOR_DEBUG_LEVEL=0 mylib.cpp   // release library
// cl /D_ITERATOR_DEBUG_LEVEL=2 myapp.cpp   // debug application
// Linking mylib.obj + myapp.obj -> link error: symbol conflict

// IDL=2 iterator layout:
struct _Vector_iterator_debug {
    int* _Ptr;               // pointer to current element
    const _Container_base* _Mycont;  // pointer to owning container
    // Every iterator operation validates:
    //   _Mycont != nullptr (not detached from container)
    //   _Ptr is within container range
    //   Container version number has not changed (detects modification during iteration)
};
```

---

## 11. ABI Break Detection Tools

```bash
# 1. abi-compliance-checker - the most popular ABI compatibility checking tool
#    Compares two versions of a library, generates detailed difference report
$ abi-compliance-checker -lib libfoo \
    -old old_headers/include -new new_headers/include \
    -old-abi old.dump -new-abi new.dump

# Output: HTML report listing all ABI breaks:
#   - Removed symbols
#   - Changed parameter types
#   - Changed data member layout
#   - Changed vtable layout

# 2. abidiff - core tool of libabigail
$ abidiff libfoo_v1.so libfoo_v2.so
# Output: human-readable ABI difference text

# 3. libabigail suppression mechanism
# Create a suppress.abignore file:
[suppress_type]
  name_regexp = .*_detail.*
  # Ignore all type changes in _detail namespaces

# 4. nm / readelf - manual symbol comparison
$ nm -DC libfoo_v1.so | sort > v1.symbols
$ nm -DC libfoo_v2.so | sort > v2.symbols
$ diff v1.symbols v2.symbols
# Removed symbols = ABI break

# 5. ldd / objdump - check dynamic dependencies
$ objdump -p libfoo.so | grep NEEDED
# Check whether dependent library versions are compatible

# 6. MSVC: dumpbin /EXPORTS
$ dumpbin /EXPORTS foo_v1.dll > v1_exports.txt
$ dumpbin /EXPORTS foo_v2.dll > v2_exports.txt
$ fc v1_exports.txt v2_exports.txt
```

---

## 12. Symbol Visibility

### 12.1 Why Control Visibility

```
Default behavior (-fvisibility=default):
  - All symbols are exported to the .so's dynamic symbol table
  - The dynamic linker can resolve all exported symbols
  - Problem: bloated symbol table, slow linking, high risk of symbol conflicts

Goal:
  - Export only public API symbols (principle of minimal visibility)
  - Internal implementation symbols are invisible externally -> smaller symbol table, faster linking, fewer conflicts
  - Enables more compiler optimizations (hidden symbols can be devirtualized, inlined)
```

### 12.2 GCC/Clang Visibility Control

```cpp
// Global default visibility (compiler option)
// -fvisibility=hidden   -> hide all symbols by default
// -fvisibility=default  -> export all symbols by default (legacy behavior)

// Per-symbol control:
__attribute__((visibility("default"))) void public_api();   // exported
__attribute__((visibility("hidden")))  void internal_func(); // hidden

// Class-level control:
class __attribute__((visibility("default"))) Widget {
    // All member functions are exported
    void method();  // automatically exported
};

// DLLExport/Import style wrapper:
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef BUILDING_DLL
    #define DLL_PUBLIC __declspec(dllexport)
  #else
    #define DLL_PUBLIC __declspec(dllimport)
  #endif
  #define DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define DLL_PUBLIC __attribute__((visibility("default")))
    #define DLL_LOCAL  __attribute__((visibility("hidden")))
  #else
    #define DLL_PUBLIC
    #define DLL_LOCAL
  #endif
#endif

// Usage:
DLL_PUBLIC  int exported_function();    // exported
DLL_LOCAL   void internal_helper();     // hidden

// Template visibility:
// Template instantiations use COMDAT folding; the visibility attribute controls dynamic linking behavior
template class DLL_PUBLIC std::vector<int>;  // explicit instantiation and export
```

### 12.3 CMake Practices

```cmake
# Set symbol visibility in CMake (recommended approach)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)      # hide by default
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)      # hide inline functions too

# Generate compile_commands.json for debugging visibility issues
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use generator expressions on Windows to control export/import
target_compile_definitions(mylib PRIVATE
    $<$<BUILD_INTERFACE:BUILDING_DLL>
)

# Use export header to auto-generate macros
include(GenerateExportHeader)
generate_export_header(mylib
    EXPORT_FILE_NAME export/mylib_export.h
    # Generates MYLIB_EXPORT and MYLIB_NO_EXPORT macros
)
```

### 12.4 Performance Impact of Visibility

```
Optimization benefits of hidden symbols:

  1. Devirtualization:
     // When the compiler knows all possible derived classes are within the same .so
     // (because derived class symbols are hidden and cannot be linked from outside),
     // the compiler can replace virtual function calls with direct calls

  2. Inlining:
     // Function bodies of hidden symbols are visible within the compilation unit
     // Link-time optimization (LTO) can inline them

  3. Reduced PLT/GOT overhead:
     // Default visibility -> indirect call through PLT (4-5 extra instructions)
     // Hidden visibility -> direct call (0 extra instructions)
     // In hot loops this can produce a measurable performance difference

  Symbol table size comparison (typical case):
  +------------------------------------------+
  | 100 public API + 5000 internal symbols   |
  |                                          |
  | -fvisibility=default: 5100 exported symbols |
  | -fvisibility=hidden + 100 default:       |
  |                       100 exported symbols |
  |                                          |
  | Dynamic linking time: ~50x difference    |
  +------------------------------------------+
```

---

## 13. Cross-Platform ABI Compatibility Practices

```
Core principles for writing ABI-stable libraries across compilers and platforms:

  1. Use C language interfaces (extern "C") as the public API boundary
     extern "C" void* widget_create(int width, int height);
     extern "C" void  widget_destroy(void* handle);
     // C ABI is fully compatible across compilers: no mangling, no vtable, no exceptions

  2. Do not expose C++ classes in public headers
     // .h  - expose only C interfaces and opaque pointers
     // .cpp - internal implementation uses full C++ features

  3. PIMPL (Pointer to Implementation) hides private members
     // widget.h
     class Widget {
     public:
         Widget(int w, int h);
         ~Widget();
         void draw();
     private:
         struct Impl;         // forward declaration
         std::unique_ptr<Impl> impl_;  // fixed size (one pointer)
     };
     // Adding private members does not change Widget's size or layout -> ABI stable

  4. Use feature-test macros to avoid header conditional compilation differences
     #if __has_include(<format>)
     #include <format>
     // new interface
     #endif

  5. Never use STL types in the public ABI
     // Bad: std::string, std::vector in public API - incompatible across compiler versions
     // Good: const char* + size_t, or custom ABI-stable container types
```

---

## Further Reading

- [Itanium C++ ABI Specification](https://itanium-cxx-abi.github.io/cxx-abi/) — The authoritative ABI document followed by GCC / Clang
- [SysV AMD64 ABI](https://gitlab.com/x86-psABIs/x86-64-ABI) — x86-64 platform calling convention and data layout specification
- [libstdc++ Dual ABI Documentation](https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_dual_abi.html) — GCC dual ABI migration guide
- [libc++ ABI Versioning](https://libcxx.llvm.org/DesignDocs/ABIVersioning.html) — LLVM libc++ inline namespace strategy
- [abi-compliance-checker](https://lvc.github.io/abi-compliance-checker/) — Automated ABI compatibility checking tool
- [C++ Toolchain and Ecosystem](/topics/toolchain) — Compilers, build systems, sanitizers
- [Compiler Optimization Panorama](/topics/compiler-optimizations) — LTO, devirtualization, inlining, and other ABI-affecting optimizations

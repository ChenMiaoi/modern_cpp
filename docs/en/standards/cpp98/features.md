---
title: "C++98 Language Features"
topic: unknown
feature: features
standard: N/A
status_checked_at: 2026-06-02
---
# C++98 Language Features

## Classes and Object-Oriented Programming

C++98 introduced a complete object-oriented system on top of C:

- **Class Definition**: Encapsulation of data and behavior
- **Inheritance**: Single inheritance, multiple inheritance, virtual inheritance
- **Polymorphism**: Virtual functions (`virtual`), pure virtual functions
- **Access Control**: `public`, `protected`, `private`
- **Abstract Base Classes**: Classes containing pure virtual functions cannot be instantiated

## Templates

Function templates and class templates are the cornerstones of generic programming:

```cpp
// Function template
template<typename T>
T max(T a, T b) { return a > b ? a : b; }

// Class template
template<typename T>
class Stack {
    std::vector<T> elems;
public:
    void push(const T& elem);
    T pop();
};
```

## Exception Handling

```cpp
try {
    // Code that may throw an exception
    throw std::runtime_error("something went wrong");
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
} catch (...) {
    // Catch all exceptions
}
```

## Namespaces

```cpp
namespace MyLib {
    class Widget { /* ... */ };
}

MyLib::Widget w;  // Qualified via namespace when used
```

## Other Features

- **RTTI**: `typeid`, `dynamic_cast`
- **`const` Qualifier**: Variables, pointers, references, member functions
- **References**: Lvalue references (`T&`)
- **Operator Overloading**
- **`new` / `delete`**
- **`static_cast` / `dynamic_cast` / `const_cast` / `reinterpret_cast`**

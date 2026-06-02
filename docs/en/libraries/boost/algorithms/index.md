---
title: "Boost 算法与数学"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost Algorithms & Mathematics

## Multiprecision: Arbitrary-Precision Arithmetic

Core design: **frontend/backend separation**. The frontend (`number<Backend>`) provides a unified operator interface, while the backend determines storage and algorithms.

```cpp
template<typename Backend>
class number {
    Backend backend_;
public:
    number& operator*=(number const& other) {
        backend_.multiply(backend_, other.backend_);
        return *this;
    }
};
```

| Backend | Storage | Precision | Use Case |
|---------|---------|-----------|----------|
| `cpp_int` | Dynamic `vector<limb_type>` | Runtime arbitrary precision | General-purpose big integers |
| `int128_backend` | Stack-allocated 128-bit | Fixed 128-bit | Beyond uint64_t |
| `gmp_backend` | GMP's `mpz_t` | Arbitrary precision | Maximum performance |

### Small Value Optimization and Multiplication Strategy

```cpp
void multiply(cpp_int_backend& result, ...) {
    if (a.size() <= 2 && b.size() <= 2) {
        // Small: single mul instruction
    } else if (a.size() < 30 || b.size() < 30) {
        // Medium: naive O(n*m) multiplication
    } else {
        // Large: Karatsuba O(n^1.585)
        // Even larger: Toom-Cook-3 or FFT O(n log n)
    }
}
```

### Expression Templates

`a = b * c + d * e` uses expression templates to avoid 3 temporary objects — the entire expression tree is computed into `a`'s backend in a single assignment.

---

## Math: Mathematical Functions

Boost.Math provides extensions to standard mathematical functions:

- **Special functions**: Bessel functions, elliptic integrals, Gamma/Beta functions
- **Statistical distributions**: PDF/CDF/quantiles for normal, Poisson, etc.
- **Numerical integration**: adaptive integration, Gaussian quadrature
- **Constants**: compile-time mathematical constants (π, e, √2, etc.)

---

## Geometry: Computational Geometry

```cpp
namespace bg = boost::geometry;
using Point = bg::model::d2::point_xy<double>;
using Polygon = bg::model::polygon<Point>;

Polygon poly;
bg::read_wkt("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))", poly);
std::cout << bg::area(poly);             // 25
std::cout << bg::within(Point(2.5, 2.5), poly);  // 1
```

The strategy pattern allows switching implementations: distance calculations can choose Euclidean/Manhattan distance, point-in-polygon tests can choose different precision strategies.

---

## Range: Range Algorithms

Boost.Range is the predecessor of C++20 Ranges, providing pipe-based range operations:

```cpp
auto result = data | boost::adaptors::filtered(pred)
                   | boost::adaptors::transformed(fn);
```

---

## Other Algorithm Libraries

| Library | Description |
|---------|-------------|
| **Algorithm** | String algorithms (trim, split, join, replace) |
| **CRC** | CRC-16/32/64 checksums |
| **Conversion** | Type conversion (`lexical_cast`, `numeric_cast`) |
| **Endian** | Byte order conversion (`big_to_native`, `little_to_native`) |

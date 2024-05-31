# CANT
## Constexpr allocations (non transient) library

---

C++20-compatible library which implement constexpr non-transient allocator for standard containers.

Now it supports **std::string** and **std::vector** (just because there is no other dynamic constexpr containers in standard library)

Compilers support:
  - Clang >= 12.0.0
  - MSVC >= 19.31
  - GCC now is not supported (due to bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=115233)

Now there is convinient way to construct strings and vectors at compile time:
```cpp
constexpr std::string_view str1 = "It is string which was builded ";
constexpr std::string_view str2 = "as concatenation of two strings";

constexpr auto concat_result_1 = 
    cant::too_constexpr(
        []() -> std::string
        {
            return std::string(str1) + std::string(str2);
        }
);

// It does working!
static_assert(concat_result == "It is string which was builded as concatenation of two strings", "error");
s

```

# How to use

It's necessary to pass functional object (lambda, for example) which produce result to function `cant::too_constexpr`.

# Limitations

Lamda passed to `cant::too_constexpr` should **NOT**:
  - have any arguments (arguments with default values is possible)
  - capture any objects

It should use only other constexpr objects.

Nested containers e.g. `std::vector<std::string>`, now is not supported.

# Build examples

```shell
mkdir build
cd build
cmake ..
cmake --build .
```

Examples require C++23 standard, because it contain example with std::unordered_map.

Other examples can work with C++20.

# Possible errors

---

## Clang

If your constexpr object is too big and you get compile error which looks like `...constexpr evaluation hit maximum step limit...`, just increase constexpr steps by adding flat `-fconstexpr-steps=N`.

# Future
  - support nested containers
  - improve compile time
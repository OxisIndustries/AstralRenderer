---
name: Modern C++
description: Standards for using C++23 features in the codebase
---

# Modern C++ Skill

The AstralRenderer project targets **C++23**. All new code must adhere to modern standards.

## Core Features

### 1. Standard Library
- **`std::expected`**: Use for error handling instead of output parameters or exceptions where appropriate.
- **`std::span`**: Use for contiguous memory access instead of raw pointer + size.
- **`std::string_view`**: Use for read-only string arguments.
- **Modules:** (If enabled) Prefer standard imports; otherwise use standard `#include`.

### 2. Smart Pointers
- **Ownership:** default to `std::unique_ptr` for exclusive ownership.
- **Sharing:** Use `std::shared_ptr` ONLY when ownership is truly shared (e.g., Resources used by multiple scenes).
- **Raw Pointers:** Use ONLY for non-owning views (observers). Never delete a raw pointer.

### 3. Algorithms & Ranges
Use `std::ranges` for cleaner loop logic.

**Preferred:**
```cpp
std::ranges::for_each(items, [](auto& item) { item.update(); });
```

**Avoid:**
```cpp
for (auto it = items.begin(); it != items.end(); ++it) { ... }
```

### 4. Lambdas
Use captures carefully. Prefer `[&]` only in short-lived local scopes (like `RenderGraph` callbacks). explicit captures `[this]` or `[var]` are preferred for longer scopes.

### 5. Formatting
- **Indentation:** 4 spaces.
- **Braces:** Same line for functions/classes/control structures (K&R style variation or Allman depending on project `.clang-format`, check existing files).
  *(Note: Check `.clang-format` if available, otherwise match surrounding code).*

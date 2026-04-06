To solve this in a production environment, you need a prompt that forces the AI to focus on **memory layout**, **template metaprogramming**, and **type safety**.

Since you are dealing with deep nesting, the prompt must explicitly ask for a design that avoids the overhead of the default PEGTL `std::string` rule names and provides a mechanism for non-recursive traversal.

---

## The Master Prompt

> **Role:** You are a C++ Systems Engineer expert in the `tao::pegtl` library and AST optimization.
>
> **Task:** Provide a complete C++ implementation for a custom AST node system in PEGTL that replaces string-based rule identification with a high-performance `enum class` dispatch system.
>
> **Core Requirements:**
> 1.  **Custom Node Class:** Define `CustomNode` inheriting from `tao::pegtl::parse_tree::node`. It must include a `NodeType` enum (using `uint8_t` for memory efficiency).
> 2.  **The Selector/Transform:** Create a template `NodeSelector` that uses `std::is_same_v` within a `static void transform` function to map specific PEGTL grammar rules to `NodeType` enum members.
> 3.  **Deep Nesting Optimization:**
      >     *   Explain how to minimize the node size to improve cache locality.
>     *   Provide a skeletal example of a **stack-based (non-recursive)** visitor pattern that switches on the `NodeType` enum to prevent stack overflow on deeply nested trees.
> 4.  **Documentation:** Use Doxygen comments for all classes and methods, specifically detailing **Motivation**, **Pre-conditions**, and **Post-conditions**.
>
> **Constraints:**
> *   Use C++17 or later (using `if constexpr`).
> *   Ensure the solution is compatible with `tao::pegtl::parse_tree::parse`.
> *   Avoid `dynamic_cast` or `std::type_info` for node identification.
>
> **Output:** Provide the header file structure and a brief usage example in a `main()` function.

---

## Why this prompt works for your problem:

*   **Memory Efficiency:** By specifying `uint8_t`, you ensure that the node's memory footprint is minimized, which is critical when a deeply nested tree creates thousands of nodes.
*   **Compile-Time Dispatch:** It forces the use of `if constexpr`, ensuring that the mapping from "Rule" to "Enum" happens at compile time with zero runtime overhead.
*   **Safety for Deep Trees:** By explicitly asking for a **non-recursive** visitor, you ensure the AI doesn't give you a standard recursive solution that would crash on a deeply nested input.
*   **Strict Documentation:** By requesting the Doxygen format we discussed earlier, you ensure the code remains maintainable as your grammar grows.

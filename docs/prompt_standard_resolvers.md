To create a truly robust implementation, I have synthesized your requirements with the technical best practices for high-performance C++ GraphQL engines. This refined prompt ensures the AI addresses **caching**, **interface abstraction**, and **type safety**.

---

## The Refined Master Prompt

> **Role:** You are a Principal Systems Engineer specializing in C++ GraphQL execution engines and middleware design.
>
> **Task:** Design and implement a C++ infrastructure for a custom GraphQL directive: `@isched_resolve_with(type: String!, config_file: String!)`. This directive delegates field resolution to a "Standard Resolver" fueled by an external JSON configuration.
>
> **Architectural Components to Implement:**
>
> 1.  **Standard Resolver Interface:** Define a pure virtual base class `IStandardResolver`.
      >     *   **Method:** `resolve(const nlohmann::json& config, const ResolveContext& ctx)`
>     *   **Motivation:** To decouple the specific logic (like `static_json`) from the directive interception logic.
>
> 2.  **Resolver Registry:** A singleton or managed registry that allows the server to look up an `IStandardResolver` instance by its string name (e.g., "static_json").
>
> 3.  **Config Provider with Caching:** A component responsible for loading and parsing JSON files.
      >     *   **Requirement:** Implement a thread-safe cache (e.g., `std::unordered_map<std::string, nlohmann::json>`) to prevent redundant disk I/O.
>     *   **Motivation:** High-frequency queries must not be bottlenecked by filesystem latency.
>
> 4.  **Directive Interceptor/Middleware:**
      >     *   **Logic:** Upon encountering `@isched_resolve_with`, the middleware must:
                >         1. Extract `type` and `config_file` arguments.
>         2. Fetch the parsed JSON from the **Config Provider**.
>         3. Execute the matching **Standard Resolver**.
>         4. **Post-condition:** Ensure the returned JSON structure matches the expected GraphQL field type (basic validation).
>
> **Documentation Standards:**
> *   All classes and methods must use **Doxygen inline documentation**.
> *   Each block must explicitly state **Motivation**, **Pre-conditions**, and **Post-conditions**.
>
> **Specific Implementation Example:**
> *   Provide the full code for a `StaticJsonResolver`. If the config file is `{"data": {"msg": "hello"}}` and the query asks for `msg`, it should return the specific leaf or subtree.
>
> **Constraints:**
> *   Use modern C++ (C++17 or C++20).
> *   Assume `nlohmann::json` for data handling.
> *   The code should be framework-agnostic but demonstrate where it would hook into a standard field-resolver execution chain.

---

### Why these refinements matter for Isched:

*   **The Interface Approach:** By forcing the AI to create `IStandardResolver`, you make your server extensible. Next month, if you need a `database_query` or `rest_proxy` resolver, you just implement the interface without touching the directive code.
*   **Context Awareness:** Adding `ResolveContext` to the resolver ensures that even if you are using a static JSON file, you still have access to things like user authentication headers or other field arguments if needed.
*   **Thread Safety:** Since most C++ GraphQL servers (like `graphql-cpp`) are multi-threaded, the **Config Provider** must be thread-safe to avoid crashes during simultaneous file access.

How do you plan to handle file updates? Should the server re-cache the JSON if the file on disk changes, or is a server restart acceptable for your use case?
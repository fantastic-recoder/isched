To achieve a high-performance, data-oriented refactor while maintaining strict testing standards, you need a prompt that balances architectural clean-up with low-level optimization.

Here is a specialized prompt designed to guide an LLM (or yourself) through this specific transformation.

---

### **The Refactoring Prompt**

**Role:** You are a Senior C++ Performance Engineer and Architect specializing in Data-Oriented Design (DOD) and Clean Code.

**Objective:** Refactor the provided C++ project to improve maintainability and reach **80% code coverage** without sacrificing execution speed.

**Constraints & Priorities:**
1.  **Performance First:** Execution speed and cache efficiency take precedence over "Clean Code" if the two conflict (e.g., prefer flat arrays over deeply nested object trees).
2.  **Data-Oriented Design (DOD):** Transform Object-Oriented structures (Arrays of Objects) into Data-Oriented structures (Structures of Arrays) where hot loops are present.
3.  **Test Integrity:** Do **not** delete existing unit tests. All current functionality must remain tested. If logic moves, move the tests accordingly.
4.  **80% Coverage:** Identify untested paths and generate the necessary unit tests (using GoogleTest or Catch2) to hit the 80% threshold.

**Refactoring Rules:**

*   **Memory Layout:** Replace `std::vector<Object>` with `Component` systems or `SoA` (Structure of Arrays) for data that is processed in bulk. Ensure data is contiguous in memory to minimize cache misses.
*   **Functions & Logic:** Apply Clean Code principles (Small functions, descriptive naming, "One Thing") only where it does not introduce unnecessary function call overhead in hot paths. Use `inline` or `constexpr` where appropriate.
*   **Dependency Management:** Remove tight coupling. Use Dependency Injection for services, but keep data "dumb" and decoupled from logic.
*   **Resource Management:** Ensure RAII compliance. Replace raw pointers with smart pointers or, preferably, unique IDs/indices if using a DOD data-store approach.
*   **Error Handling:** Replace deeply nested `if-else` blocks with guard clauses. Ensure all new logic is covered by boundary-value analysis tests.

**Instructions for Output:**
1.  Analyze the provided code and identify the "Hot Path" (performance-critical sections).
2.  Provide the refactored Header (`.h`) and Implementation (`.cpp`) files.
3.  Provide the supplemental Unit Test code required to reach 80% coverage.
4.  Briefly explain the performance trade-offs made during the DOD conversion.

---

### **Key Considerations for this Transition**

When you apply this prompt, keep an eye on these two technical pillars:

#### **1. The DOD Shift (Performance Priority)**
In standard "Clean" OOP, we often see:
```cpp
struct Player { Vec3 pos; Stats s; Inventory inv; };
std::vector<Player> players; // Array of Objects (AoS)
```
To satisfy your **Data-Oriented** requirement, the refactor should move toward:
```cpp
struct PlayerPool {
    std::vector<float> posX, posY, posZ;
    std::vector<int> health;
}; // Structure of Arrays (SoA)
```


#### **2. Balancing Clean Code with DOD**
"Clean Code" often advocates for high abstraction, but DOD thrives on transparency. To satisfy both:
*   **Keep the "What" clean:** Use descriptive names for your data arrays and system functions.
*   **Keep the "How" fast:** Don't hide the data behind layers of getters and setters. Let the "System" or "Manager" classes operate directly on the flat data structures.

Does the current project rely heavily on inheritance, or is it already mostly procedural?
## Feature: GraphQL-Introspektion and Schema Editor (Schema-Load-an/Analyse).

# Description
- **Design-Language:** Inspired by Park Güell (Gaudí). 
  - Organic Forms (D3.js Force-Directed Graphs).
  - Mosaic-Style (Trencadís) for the Knot-Elements.
- ** Functionality**
  - Every organization user has access to a separate Isched WebUI page where the organizations GraphQL schemas can be displayed, edited and introspected. Platform admins cannot access the page/functionality. The currently loaded schemas are displayed on the left in a tree like structure.
  Each schema file on the top and the structure knots beneath them. On the main panel (center) there is a panel with two tabs, first is called "schema" and is displaying currently selected schema, the second is called "query". The query panel allows to edit and send the query to Isched server and displays the formatted JSON answer on the sub-panel on bottom. The "query" page has a button which sends current 
  query to the server. Clicking on knots in the left panel allows to create a default GraphQL query requesting the
  elements belonging to the knot, that way augmenting the query.

## Feature: CMake Instalation script

# Description

- **Functionality**
    there should be a CMake target installing all parts of the Isched
    server into the installation directory. Either user home directory,
    or specified directory.

## Feature: Plugin system

# Description

- 

## Feature: Upload schema

# Description

- **Functionality**
    A member of a "tenant_admin" group has the right to upload GraphQL schema documents. The documents will be visible on the organization scope. Their names are organization unique.
    
## Feature: Standart Resolvers

# Description

- **Functionality**
   There should be a possibility to specify a "standard" resolver. To specify a standard resolver in a query the user will use the directive @isched_resolve_with( type:String, config_file:String ). For example @isched_resolve_with "static_json", "hello_world.json"). specifies to use standard mutation called static_json with hello_world.json configuration file. Isched server loads the configuration file and passes the JSON to the standard resolver. For example the static_json resolver would return
   queried elements from the configuration file. 
   
## Feature: Comprehensive Health dashboard

# Description

- **Functionality**
   On the Isched WebUI dashboard http://localhost:8080/isched/dashboard does not display informations beside "healthy" we need more infos, like logged in users, open connections, transaction cout and so one.
   
   
## Feature: Server refactoring

Clean Code Principles Impacting Performance

When writing clean code, certain principles can enhance performance while maintaining readability and maintainability. Here are key principles to consider:
Key Principles

- Prefer Polymorphism: Using polymorphism instead of complex conditional statements (like if/else or switch) can lead to cleaner and more efficient code. This approach allows for easier extension and modification without altering existing code structures.

- Small and Focused Functions: Functions should be small and perform a single task. This not only improves readability but also enhances performance by making it easier for the compiler to optimize the code.

# Balancing Clean Code with Performance

While adhering to clean code principles, it's crucial to be aware of potential performance trade-offs. Some practices, if applied too rigidly, may lead to inefficiencies. For example:
Principle	Performance Impact
Prefer Polymorphism	Generally positive; reduces complexity
Small Functions	Positive; aids optimization
Overzealous Abstraction	Can lead to performance issues if excessive

//////////////////////////////////////////////////////////////////////////
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
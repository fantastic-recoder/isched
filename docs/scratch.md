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

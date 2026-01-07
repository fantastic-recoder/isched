
# Comprehensive Guide to Apollo GraphQL Resolvers

This guide covers the basics of writing resolvers, how to handle arguments, and how to manage object relationships.

## 1. What are Resolvers?

While the schema defines **what** data is available, resolvers define **how** to fetch it. They are the implementation details behind your GraphQL API.

## 2. The Resolver Signature

Every resolver function in Apollo Server receives four positional arguments:
```
typescript
fieldName: (parent, args, context, info) => { ... }
```
*   **`parent`**: The return value of the resolver for this field's parent. (Commonly used in field-level resolvers).
*   **`args`**: An object containing all GraphQL arguments provided for this field.
*   **`context`**: A shared object across all resolvers (useful for database connections, authentication, or global state).
*   **`info`**: Information about the execution state (rarely used in basic implementations).

---

## 3. Basic Implementation

To implement basic queries, create an object that matches the structure of your GraphQL `Query` type.

### Example Schema (`hello_world_schema.graphql`):
```
graphql
type Query {
hello_ping: String
hello_who(p_name: String): String
}
```
### Example Resolver Implementation:
```
typescript
const resolvers = {
Query: {
// A simple field returning a static string
hello_ping: () => "pong",

    // A field using the 'args' parameter to provide dynamic data
    hello_who: (_parent: any, args: { p_name?: string }) => {
      const name = args.p_name || "World";
      return `Hello, ${name}!`;
    }
}
};
```
---

## 4. Handling Object Relationships

If your schema has nested types (e.g., an `Article` that belongs to an `Author`), you define a resolver for the specific Type to handle data fetching for that field.

### Schema Example:
```
graphql
type Article {
id: ID!
title: String
author: Author
}

type Author {
id: ID!
name: String
}
```
### Resolver Example:
```
typescript
const resolvers = {
Query: {
// This returns the base object
article: () => fetchArticleFromDB(),
// Example return: { id: "1", title: "GraphQL Tips", authorId: "123" }
},
Article: {
// The 'parent' here is the article object returned by the Query above
author: (parent) => {
// We use the ID from the parent to fetch the related author
return fetchAuthorById(parent.authorId);
}
}
};
```
---

## 5. Default Resolvers

If you don't define a resolver for a specific field, Apollo Server automatically defines a **default resolver** for it. 

The default resolver looks at the `parent` object and attempts to find a property with the same name as the field. If it finds one, it returns that value.

### Example Schema:
```graphql
type User {
  id: ID!
  username: String
  email: String
}

type Query {
  me: User
}
```

### Example Implementation (Minimal):
```typescript
const resolvers = {
  Query: {
    me: () => {
      // We only return the object. 
      // id, username, and email will be handled by default resolvers.
      return {
        id: "1",
        username: "jdoe",
        email: "john@example.com"
      };
    }
  }
  // No "User" object needed here if properties match field names
};
```

---

## 6. Nested Resolvers (Multi-level depth)

GraphQL excels at fetching nested data in a single request. Resolvers can be chained to any depth. When a field returns an object type, Apollo looks for a resolver for that type to handle its sub-fields.

### 3-Level Depth Example

In this example, we fetch a **School**, its **Departments**, and the **Teachers** within those departments.

#### Schema Example:
```graphql
type School {
  id: ID!
  name: String
  departments: [Department]
}

type Department {
  id: ID!
  name: String
  teachers: [Teacher]
}

type Teacher {
  id: ID!
  name: String
}

type Query {
  school(id: ID!): School
}
```

#### Resolver Example:
```typescript
const resolvers = {
  Query: {
    // Level 1: Fetch the root object (School)
    school: (_, { id }) => fetchSchoolById(id),
  },
  School: {
    // Level 2: Fetch related Departments using School's ID
    departments: (school) => fetchDepartmentsBySchoolId(school.id),
  },
  Department: {
    // Level 3: Fetch related Teachers using Department's ID
    teachers: (department) => fetchTeachersByDepartmentId(department.id),
  }
};
```

---

## 7. Resolver Processing Algorithm

When a GraphQL query is received, the server follows a specific algorithm to resolve the data:

1.  **Parsing**: The query string is parsed into an Abstract Syntax Tree (AST).
2.  **Validation**: The AST is validated against the schema to ensure requested fields exist and arguments are correct.
3.  **Execution**: The server traverses the AST in a **depth-first** manner.
    *   For each field, the server looks for an associated resolver.
    *   If a resolver is found, it is executed with the current `parent` value.
    *   If the field returns a scalar (String, Int, etc.), the execution for that branch is complete.
    *   If the field returns an object type, the return value becomes the `parent` for the next level of resolvers.
4.  **Result Mapping**: All resolved values are collected into a JSON object that mirrors the structure of the query.

### Execution Example:

For a query like `school { departments { teachers { name } } }`:
1.  Call `Query.school` -> returns `schoolObj`.
2.  Call `School.departments(schoolObj)` -> returns `[dept1, dept2]`.
3.  For each department (e.g., `dept1`), call `Department.teachers(dept1)` -> returns `[teacher1, teacher2]`.
4.  For each teacher, call `Teacher.name(teacherObj)` (handled by default resolver).

---

## 8. Tips for Success

*   **Async Support**: Resolvers can be `async`. Apollo will automatically await any returned Promises before sending the response.
*   **Destructuring**: It is common practice to use underscores for unused parameters (e.g., `_parent`) and destructure `args` or `context` for cleaner code:
    ```typescript
    hello_who: (_, { p_name }) => `Hello, ${p_name}`
    ```
*   **Context for Auth**: Use the `context` argument to pass user sessions or database instances so they are accessible to every resolver in the tree.
```

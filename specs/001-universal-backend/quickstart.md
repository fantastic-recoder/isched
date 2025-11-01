# Quickstart Guide: Universal Application Server Backend

**Purpose**: Step-by-step guide for frontend developers to get started with Isched Universal Backend.
**Target Audience**: Frontend developers with basic Python or TypeScript knowledge.
**Prerequisites**: Isched server installed and running.

## Overview

Isched Universal Backend eliminates the need for external databases, authentication services, and API frameworks. With just a configuration script, you can have a complete GraphQL backend running in minutes.

## Quick Start (5 Minutes)

### Step 1: Install Isched

```bash
# Download and install Isched (example - replace with actual installation)
curl -sSL https://install.isched.dev | bash
# or
npm install -g isched-server
```

### Step 2: Create Your First Configuration

Create a file named `my-backend.py`:

```python
from isched import define_model, define_auth, start_server

# Define your data models
User = define_model('User', {
    'name': {'type': 'string', 'required': True},
    'email': {'type': 'string', 'required': True, 'unique': True},
    'age': {'type': 'integer', 'min': 0}
})

Product = define_model('Product', {
    'name': {'type': 'string', 'required': True},
    'price': {'type': 'float', 'required': True, 'min': 0},
    'description': {'type': 'string', 'max_length': 1000},
    'in_stock': {'type': 'boolean', 'default': True}
})

# Configure authentication
define_auth({
    'oauth_providers': ['google', 'github'],
    'jwt_secret': 'auto-generated',
    'session_timeout': 3600,
    'allow_registration': True
})

# Start the server
if __name__ == '__main__':
    start_server(port=8080)
```

### Step 3: Run Your Backend

```bash
python my-backend.py
```

Your GraphQL backend is now running at `http://localhost:8080/graphql`!

## TypeScript Configuration Example

Create `my-backend.ts`:

```typescript
import { defineModel, defineAuth, startServer } from 'isched';

// Define your data models
const User = defineModel('User', {
  name: { type: 'string', required: true },
  email: { type: 'string', required: true, unique: true },
  age: { type: 'number', min: 0 }
});

const Product = defineModel('Product', {
  name: { type: 'string', required: true },
  price: { type: 'number', required: true, min: 0 },
  description: { type: 'string', maxLength: 1000 },
  inStock: { type: 'boolean', default: true }
});

// Configure authentication
defineAuth({
  oauthProviders: ['google', 'github'],
  jwtSecret: 'auto-generated',
  sessionTimeout: 3600,
  allowRegistration: true
});

// Start the server
startServer({ port: 8080 });
```

Run with:

```bash
npx tsx my-backend.ts
```

## Testing Your API

### Using GraphQL Playground

Visit `http://localhost:8080/graphql` in your browser to access the GraphQL Playground.

### Sample Queries

```graphql
# Create a user
mutation {
  createUser(input: {
    name: "John Doe"
    email: "john@example.com"
    age: 30
  }) {
    id
    name
    email
    createdAt
  }
}

# Query users
query {
  users {
    id
    name
    email
    age
  }
}

# Create a product
mutation {
  createProduct(input: {
    name: "Laptop"
    price: 999.99
    description: "High-performance laptop"
    inStock: true
  }) {
    id
    name
    price
  }
}

# Query products
query {
  products(filter: { inStock: true }) {
    id
    name
    price
    description
  }
}
```

### Using cURL

```bash
# Create a user
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{
    "query": "mutation { createUser(input: { name: \"Jane Doe\", email: \"jane@example.com\" }) { id name email } }"
  }'

# Query users
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{
    "query": "query { users { id name email } }"
  }'
```

## Authentication Flow

### 1. User Registration

```graphql
mutation {
  register(input: {
    email: "user@example.com"
    password: "securepassword"
    name: "New User"
  }) {
    accessToken
    refreshToken
    user {
      id
      email
      name
    }
  }
}
```

### 2. User Login

```graphql
mutation {
  login(input: {
    email: "user@example.com"
    password: "securepassword"
  }) {
    accessToken
    refreshToken
    expiresAt
  }
}
```

### 3. Using Authentication Token

```bash
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "query": "query { currentUser { id name email } }"
  }'
```

### 4. OAuth Authentication

Visit `http://localhost:8080/auth/google` or `http://localhost:8080/auth/github` to initiate OAuth flow.

## Advanced Configuration

### Custom Validation

```python
from isched import define_model, custom_validator

@custom_validator
def validate_email_domain(value):
    allowed_domains = ['company.com', 'partner.org']
    domain = value.split('@')[1]
    return domain in allowed_domains

User = define_model('User', {
    'email': {
        'type': 'string',
        'required': True,
        'validators': [validate_email_domain]
    }
})
```

### Relationships Between Models

```python
Category = define_model('Category', {
    'name': {'type': 'string', 'required': True}
})

Product = define_model('Product', {
    'name': {'type': 'string', 'required': True},
    'category': {'type': 'relationship', 'model': 'Category', 'required': True}
})
```

### Custom Business Logic

```python
from isched import define_resolver

@define_resolver('Product', 'discountedPrice')
def calculate_discounted_price(product, discount_percentage=10):
    return product.price * (1 - discount_percentage / 100)

# This adds a computed field to the GraphQL schema:
# type Product {
#   discountedPrice(discountPercentage: Float = 10): Float!
# }
```

### Environment-Specific Configuration

```python
import os
from isched import define_auth

define_auth({
    'jwt_secret': os.getenv('JWT_SECRET', 'auto-generated'),
    'oauth_providers': {
        'google': {
            'client_id': os.getenv('GOOGLE_CLIENT_ID'),
            'client_secret': os.getenv('GOOGLE_CLIENT_SECRET')
        }
    },
    'cors_origins': os.getenv('CORS_ORIGINS', '*').split(',')
})
```

## Frontend Integration

### React Example

```typescript
import { ApolloClient, InMemoryCache, gql, useQuery, useMutation } from '@apollo/client';

const client = new ApolloClient({
  uri: 'http://localhost:8080/graphql',
  cache: new InMemoryCache(),
  headers: {
    authorization: localStorage.getItem('token') ? `Bearer ${localStorage.getItem('token')}` : '',
  }
});

const GET_PRODUCTS = gql`
  query GetProducts {
    products {
      id
      name
      price
      description
    }
  }
`;

const CREATE_PRODUCT = gql`
  mutation CreateProduct($input: CreateProductInput!) {
    createProduct(input: $input) {
      id
      name
      price
    }
  }
`;

function ProductList() {
  const { loading, error, data } = useQuery(GET_PRODUCTS);
  const [createProduct] = useMutation(CREATE_PRODUCT);

  if (loading) return <p>Loading...</p>;
  if (error) return <p>Error: {error.message}</p>;

  return (
    <div>
      {data.products.map(product => (
        <div key={product.id}>
          <h3>{product.name}</h3>
          <p>${product.price}</p>
        </div>
      ))}
    </div>
  );
}
```

### Vue.js Example

```vue
<template>
  <div>
    <div v-for="user in users" :key="user.id">
      <h3>{{ user.name }}</h3>
      <p>{{ user.email }}</p>
    </div>
  </div>
</template>

<script>
import { ref, onMounted } from 'vue';

export default {
  setup() {
    const users = ref([]);

    const fetchUsers = async () => {
      const response = await fetch('http://localhost:8080/graphql', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          query: `
            query {
              users {
                id
                name
                email
              }
            }
          `
        })
      });
      
      const result = await response.json();
      users.value = result.data.users;
    };

    onMounted(fetchUsers);

    return { users };
  }
};
</script>
```

## Troubleshooting

### Common Issues

**Server won't start**:
- Check if port 8080 is already in use
- Verify configuration script syntax
- Check server logs for error details

**Authentication not working**:
- Verify JWT token is being sent in Authorization header
- Check token expiration
- Ensure user has required permissions

**GraphQL queries failing**:
- Validate query syntax in GraphQL Playground
- Check if requested fields exist in schema
- Verify authentication for protected fields

### Development Tips

1. **Use GraphQL Playground** for testing queries during development
2. **Enable debug logging** in development environment
3. **Use introspection queries** to explore available schema
4. **Test with different user roles** to verify permissions
5. **Monitor server logs** for performance and error insights

## Documentation Generation

### Building API Documentation

The Isched build process automatically generates comprehensive documentation including:

- API reference documentation
- Source code examples  
- Inline code snippets
- Developer guides

```bash
# Generate documentation (included in build process)
mkdir build && cd build
cmake .. -DDOXYGEN_ENABLED=ON
make docs

# View generated documentation
open docs/html/index.html  # macOS
xdg-open docs/html/index.html  # Linux
```

### Accessing Documentation

- **API Reference**: `/docs/api/` - Complete GraphQL API documentation
- **Source Code**: `/docs/source/` - Annotated source code with examples
- **Guides**: `/docs/guides/` - Step-by-step development guides
- **Examples**: `/docs/examples/` - Complete working examples

## Next Steps

- Explore the [Complete API Reference](contracts/graphql-schema.md)
- Learn about [Advanced Configuration](../data-model.md)
- Set up [Production Deployment](../deployment-guide.md)
- Read about [Performance Optimization](../performance-guide.md)
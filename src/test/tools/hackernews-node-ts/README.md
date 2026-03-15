# Demo News GraphQL Server (Apollo)

A minimal Apollo GraphQL server that serves a demo “news site” backend with static in‑memory data for authors and articles. Built following the Apollo Server tutorial style using `@apollo/server` and an executable schema from `@graphql-tools/schema`.

## Features
- GraphQL types: `Author`, `Article`, and a `DateTime` scalar
- Queries: list and fetch single `Article`/`Author`
- Relationship resolvers: `Article.author`, `Author.articles`
- Static data, no database

## Prerequisites
- Node.js 18+ (recommended LTS)
- npm

## Install
```bash
npm install
```

## Run
- Start server:
```bash
npm start
```
- Start in watch mode (auto‑reload during edits):
```bash
npm run dev
```

The server will print a URL like:
```
🚀 Apollo GraphQL server ready at http://localhost:4000/
```
Open that URL in your browser to access Apollo Sandbox.

## Sample Queries
- Fetch site info and all articles with authors:
```graphql
{
  info
  articles {
    id
    title
    publishedAt
    author { id name }
  }
}
```

- Fetch a single article by ID with its author:
```graphql
{
  article(id: "p2") {
    id
    title
    content
    publishedAt
    author { id name bio }
  }
}
```

- Fetch authors with their articles:
```graphql
{
  authors {
    id
    name
    articles { id title }
  }
}
```

## Project Structure
```
src/
  index.ts        # Apollo Server startup (standalone)
  schema.ts       # Executable schema + resolvers + static data
  schema.graphql  # GraphQL SDL (types & queries)
```

## Notes
- Data are static and in‑memory; there are no database calls.
- `DateTime` is an ISO‑8601 string scalar used by `Article.publishedAt`.
- Default port is `4000`; override with `PORT` environment variable.

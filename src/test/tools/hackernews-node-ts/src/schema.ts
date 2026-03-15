import { makeExecutableSchema } from "@graphql-tools/schema";
import { readFileSync } from "fs";
import { fileURLToPath } from "url";
import { dirname, join } from 'path';
import { GraphQLScalarType, Kind } from "graphql";

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const typeDefs = readFileSync(join(__dirname, 'schema.graphql'), 'utf-8');

// Static demo data
const authors = [
  { id: "a1", name: "Ada Lovelace", bio: "Pioneer of computing" },
  { id: "a2", name: "Grace Hopper", bio: "Computer scientist and US Navy rear admiral" },
  { id: "a3", name: "Alan Turing", bio: "Father of theoretical computer science and AI" },
];

const articles = [
  { id: "p1", title: "Intro to Algorithms", content: "Algorithms are step-by-step procedures...", publishedAt: new Date("2024-05-01T10:00:00Z").toISOString(), authorId: "a1" },
  { id: "p2", title: "Debugging at Scale", content: "Debugging large systems requires...", publishedAt: new Date("2024-07-15T09:30:00Z").toISOString(), authorId: "a2" },
  { id: "p3", title: "Decision Problems", content: "Decidability and computability...", publishedAt: new Date("2024-09-20T12:15:00Z").toISOString(), authorId: "a3" },
];

// Simple DateTime scalar that uses ISO-8601 strings
const DateTime = new GraphQLScalarType({
  name: "DateTime",
  description: "ISO-8601 DateTime scalar",
  serialize(value: unknown): string {
    if (typeof value === "string") return value; // assume already ISO string
    if (value instanceof Date) return value.toISOString();
    throw new TypeError(`DateTime cannot serialize value: ${value}`);
  },
  parseValue(value: unknown): string {
    if (typeof value === "string") return new Date(value).toISOString();
    throw new TypeError(`DateTime cannot parse value: ${value}`);
  },
  parseLiteral(ast): string | null {
    if (ast.kind === Kind.STRING) return new Date(ast.value).toISOString();
    return null;
  },
});

export const schema = makeExecutableSchema({
  typeDefs,
  resolvers: {
    DateTime,
    Query: {
      info: () => "This is the API of a demo News site (static data)",
      articles: () => articles,
      article: (_: unknown, args: { id: string }) => articles.find(a => a.id === args.id) || null,
      authors: () => authors,
      author: (_: unknown, args: { id: string }) => authors.find(a => a.id === args.id) || null,
    },
    Article: {
      author: (article: any) => authors.find(a => a.id === article.authorId)!,
    },
    Author: {
      articles: (author: any) => articles.filter(a => a.authorId === author.id),
    },
  },
});

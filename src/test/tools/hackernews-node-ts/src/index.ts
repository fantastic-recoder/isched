import { ApolloServer } from "@apollo/server";
import { startStandaloneServer } from "@apollo/server/standalone";
import { schema } from "./schema.js";

async function main() {
  const server = new ApolloServer({ schema });

  const { url } = await startStandaloneServer(server, {
    listen: { port: Number(process.env.PORT) || 4000 },
  });

  console.log(`🚀 Apollo GraphQL server ready at ${url}`);
  console.log(`Try running a query like:`);
  console.log(`{
  info
  articles { id title publishedAt author { id name } }
}`);
}

main().catch((err) => {
  console.error("Failed to start server:", err);
  process.exit(1);
});

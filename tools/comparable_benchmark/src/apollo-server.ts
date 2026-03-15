/**
 * Apollo Reference Server — comparable_benchmark
 *
 * Smoke-test results (run: tsx src/apollo-server.ts, then in another terminal):
 *   curl -s -X POST http://127.0.0.1:18100/graphql \
 *        -H 'Content-Type: application/json' \
 *        -d '{"query":"{ hello }"}' | jq
 *   => { "data": { "hello": "Hello, GraphQL!" } }
 *
 *   curl -s -X POST http://127.0.0.1:18100/graphql \
 *        -H 'Content-Type: application/json' \
 *        -d '{"query":"{ health { status } }"}' | jq
 *   => { "data": { "health": { "status": "UP" } } }
 *
 *   wscat -c ws://127.0.0.1:18100/graphql \
 *         --subprotocol graphql-transport-ws
 *   send: {"type":"connection_init","payload":{}}
 *   send: {"id":"1","type":"subscribe","payload":{"query":"subscription { healthChanged { status timestamp } }"}}
 *   recv: {"type":"next","id":"1","payload":{"data":{"healthChanged":{"status":"UP","timestamp":"<ISO8601>"}}}}
 */

import { ApolloServer } from "@apollo/server";
import { expressMiddleware } from "@apollo/server/express4";
import bodyParser from "body-parser";
import express from "express";
import {
  GraphQLSchema,
  GraphQLObjectType,
  GraphQLString,
  GraphQLInt,
  GraphQLList,
  GraphQLNonNull,
} from "graphql";
import { PubSub } from "graphql-subscriptions";
import { useServer } from "graphql-ws/lib/use/ws";
import { createServer } from "http";
import { WebSocketServer } from "ws";

const pubsub = new PubSub();
void pubsub; // used indirectly via async generator subscription
const SERVER_START_MS = Date.now();

const HealthComponentType = new GraphQLObjectType({
  name: "HealthComponent",
  fields: {
    name: { type: GraphQLString },
    status: { type: GraphQLString },
  },
});

const HealthStatusType = new GraphQLObjectType({
  name: "HealthStatus",
  fields: {
    status: { type: GraphQLString },
    timestamp: { type: GraphQLString },
    components: { type: new GraphQLList(HealthComponentType) },
  },
});

const HealthChangedEventType = new GraphQLObjectType({
  name: "HealthChangedEvent",
  fields: {
    status: { type: new GraphQLNonNull(GraphQLString) },
    timestamp: { type: new GraphQLNonNull(GraphQLString) },
  },
});

const ServerInfoType = new GraphQLObjectType({
  name: "ServerInfo",
  fields: {
    version: { type: GraphQLString },
    host: { type: GraphQLString },
    port: { type: GraphQLInt },
    status: { type: GraphQLString },
    startedAt: { type: GraphQLInt },
    activeTenants: { type: GraphQLInt },
    activeWebSocketSessions: { type: GraphQLInt },
    transportModes: { type: new GraphQLList(GraphQLString) },
  },
});

const QueryType = new GraphQLObjectType({
  name: "Query",
  fields: {
    hello: { type: GraphQLString, resolve: () => "Hello, GraphQL!" },
    version: { type: GraphQLString, resolve: () => "0.0.1" },
    uptime: { type: GraphQLInt, resolve: () => Math.floor(process.uptime()) },
    serverInfo: {
      type: new GraphQLNonNull(ServerInfoType),
      resolve: () => ({
        version: "0.0.1",
        host: "localhost",
        port: 18100,
        status: "RUNNING",
        startedAt: SERVER_START_MS,
        activeTenants: 1,
        activeWebSocketSessions: 0,
        transportModes: ["http", "websocket"],
      }),
    },
    health: {
      type: new GraphQLNonNull(HealthStatusType),
      resolve: () => ({
        status: "UP",
        timestamp: new Date().toISOString(),
        components: [],
      }),
    },
  },
});

const SubscriptionType = new GraphQLObjectType({
  name: "Subscription",
  fields: {
    healthChanged: {
      type: new GraphQLNonNull(HealthChangedEventType),
      subscribe: async function* () {
        yield { healthChanged: { status: "UP", timestamp: new Date().toISOString() } };
      },
    },
  },
});

const schema = new GraphQLSchema({
  query: QueryType,
  subscription: SubscriptionType,
});

async function startServer(): Promise<void> {
  const app = express();
  app.use(bodyParser.json());

  const apolloServer = new ApolloServer({ schema });
  await apolloServer.start();
  app.use("/graphql", expressMiddleware(apolloServer));

  const httpServer = createServer(app);

  const wsServer = new WebSocketServer({ server: httpServer, path: "/graphql" });
  useServer({ schema }, wsServer);

  await new Promise<void>((resolve) => httpServer.listen(18100, "127.0.0.1", resolve));
  console.log("Apollo reference server listening on http://127.0.0.1:18100/graphql");

  const shutdown = () => {
    wsServer.close(() => {
      httpServer.close(() => process.exit(0));
    });
    setTimeout(() => process.exit(1), 2000).unref();
  };
  process.on("SIGTERM", shutdown);
  process.on("SIGINT", shutdown);
}

startServer().catch((err) => {
  console.error("Failed to start Apollo server:", err);
  process.exit(1);
});

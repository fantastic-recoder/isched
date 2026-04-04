"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.root = exports.schema = void 0;
const graphql_1 = require("graphql");
exports.schema = (0, graphql_1.buildSchema)(`
  type Query {
    hello: String
  }
`);
exports.root = {
    hello: () => {
        return 'Hello world!';
    },
};

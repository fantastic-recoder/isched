import express from 'express';
import { graphqlHTTP } from 'express-graphql';
import { schema, root } from './schema';

const app = express();
app.use('/graphql', graphqlHTTP({
  schema: schema,
  rootValue: root,
  graphiql: true,
}));

const port = 4000;
app.listen(port, () => {
  console.log(`Running a GraphQL API server at http://localhost:${port}/graphql`);
});

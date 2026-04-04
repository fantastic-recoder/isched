import { buildSchema } from 'graphql';
import fs from 'fs';
import path from 'path';

// Load SDL from external schema file. We try local directory first (works with ts-node),
// then fall back to project-root src path (useful during certain run setups).
function loadSDL(): string {
  const primary = path.join(__dirname, '../../hello_world_schema.graphql');
  if (fs.existsSync(primary)) {
    return fs.readFileSync(primary, 'utf8');
  }
  const fallback = path.join(process.cwd(), 'src/../..', 'hello_world_schema.graphql');
  return fs.readFileSync(fallback, 'utf8');
}

export const schema = buildSchema(loadSDL());

export const root = {
  hello: () => {
    return 'Hello world!';
  },
  hello_who: ({ p_name }: { p_name: string }) => {
    return `Hello ${p_name}!`;
  },
};

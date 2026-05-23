// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground-introspection.service.ts
 * @brief Loads and normalizes GraphQL introspection + uploaded schema documents
 *        into a single SchemaTreeNode[] for the playground schema tree (SP-011).
 */

import { Injectable, inject, signal } from '@angular/core';
import { GraphQLService } from './graphql.service';
import {
  IntrospectionArg,
  NodeKind,
  OperationKind,
  RawArgDef,
  RawFieldDef,
  RawIntrospectionResult,
  RawIntrospectionType,
  RawTypeRef,
  SchemaDocument,
  SchemaTreeNode,
  typeRefToString,
  unwrapTypeName,
} from './playground-introspection.models';

const INTROSPECTION_QUERY = `
  query IntrospectionQuery {
    __schema {
      queryType { name }
      mutationType { name }
      subscriptionType { name }
      types {
        kind
        name
        fields(includeDeprecated: true) {
          name
          type { kind name ofType { kind name ofType { kind name ofType { kind name } } } }
          args {
            name
            type { kind name ofType { kind name ofType { kind name ofType { kind name } } } }
          }
        }
        inputFields {
          name
          type { kind name ofType { kind name ofType { kind name ofType { kind name } } } }
        }
        enumValues(includeDeprecated: true) { name }
        interfaces { name }
        possibleTypes { name }
      }
      directives {
        name
        locations
        args {
          name
          type { kind name ofType { kind name ofType { kind name ofType { kind name } } } }
        }
      }
    }
  }
`;

const SCHEMA_DOCUMENTS_QUERY = `
  query SchemaDocuments {
    schemaDocuments { id name content }
  }
`;

@Injectable({ providedIn: 'root' })
export class PlaygroundIntrospectionService {
  private readonly gql = inject(GraphQLService);

  readonly treeNodes = signal<SchemaTreeNode[]>([]);
  readonly loading = signal(false);
  readonly error = signal<string | null>(null);

  load(): void {
    this.loading.set(true);
    this.error.set(null);

    this.gql.query<RawIntrospectionResult>(INTROSPECTION_QUERY).subscribe({
      next: (introspection) => {
        this.loadSchemaDocuments(introspection);
      },
      error: (err: unknown) => {
        this.loading.set(false);
        this.error.set(errorMessage(err, 'Failed to load schema introspection'));
      },
    });
  }

  private loadSchemaDocuments(introspection: RawIntrospectionResult): void {
    this.gql
      .query<{ schemaDocuments: SchemaDocument[] }>(SCHEMA_DOCUMENTS_QUERY)
      .subscribe({
        next: ({ schemaDocuments }) => {
          this.loading.set(false);
          this.treeNodes.set(buildTree(introspection, schemaDocuments ?? []));
        },
        error: () => {
          // Schema documents are optional – just continue without them
          this.loading.set(false);
          this.treeNodes.set(buildTree(introspection, []));
        },
      });
  }
}

// ─── Tree normalization ────────────────────────────────────────────────────

function buildTree(
  introspection: RawIntrospectionResult,
  docs: SchemaDocument[],
): SchemaTreeNode[] {
  const schema = introspection.__schema;
  const queryTypeName = schema.queryType?.name ?? null;
  const mutationTypeName = schema.mutationType?.name ?? null;
  const subscriptionTypeName = schema.subscriptionType?.name ?? null;

  const typeMap = new Map<string, RawIntrospectionType>(
    schema.types.filter((t) => t.name !== null).map((t) => [t.name!, t]),
  );

  const nodes: SchemaTreeNode[] = [];

  // Operation groups
  if (queryTypeName) {
    const group = buildOperationGroup('Queries', 'query', typeMap.get(queryTypeName));
    if (group) nodes.push(group);
  }
  if (mutationTypeName) {
    const group = buildOperationGroup('Mutations', 'mutation', typeMap.get(mutationTypeName));
    if (group) nodes.push(group);
  }
  if (subscriptionTypeName) {
    const group = buildOperationGroup('Subscriptions', 'subscription', typeMap.get(subscriptionTypeName));
    if (group) nodes.push(group);
  }

  // Non-builtin types grouped by kind
  const builtinNames = new Set([queryTypeName, mutationTypeName, subscriptionTypeName].filter(Boolean));
  const visibleTypes = schema.types.filter(
    (t) => t.name && !t.name.startsWith('__') && !builtinNames.has(t.name),
  );

  const objectTypes = visibleTypes.filter((t) => t.kind === 'OBJECT');
  const inputTypes = visibleTypes.filter((t) => t.kind === 'INPUT_OBJECT');
  const enumTypes = visibleTypes.filter((t) => t.kind === 'ENUM');
  const interfaceTypes = visibleTypes.filter((t) => t.kind === 'INTERFACE');
  const scalarTypes = visibleTypes.filter((t) => t.kind === 'SCALAR');

  if (objectTypes.length > 0) nodes.push(buildTypeGroup('Types', 'objectType', objectTypes));
  if (inputTypes.length > 0) nodes.push(buildTypeGroup('Inputs', 'inputType', inputTypes));
  if (enumTypes.length > 0) nodes.push(buildTypeGroup('Enums', 'enumType', enumTypes));
  if (interfaceTypes.length > 0) nodes.push(buildTypeGroup('Interfaces', 'interfaceType', interfaceTypes));
  if (scalarTypes.length > 0) nodes.push(buildTypeGroup('Scalars', 'scalarType', scalarTypes));

  // Directives
  if (schema.directives && schema.directives.length > 0) {
    nodes.push(buildDirectivesGroup(schema.directives));
  }

  // Uploaded schema documents merged into the tree
  if (docs.length > 0) {
    nodes.push(buildSchemaDocumentsGroup(docs));
  }

  return nodes;
}

function buildOperationGroup(
  label: string,
  opKind: OperationKind,
  type: RawIntrospectionType | undefined,
): SchemaTreeNode | null {
  if (!type || !type.fields) return null;

  const children: SchemaTreeNode[] = type.fields.map((f) =>
    buildFieldNode(f, opKind),
  );

  return {
    id: `group:${opKind}`,
    kind: 'operationGroup',
    name: label,
    children,
    isSelectable: false,
  };
}

function buildFieldNode(field: RawFieldDef, opKind: OperationKind): SchemaTreeNode {
  return {
    id: `field:${opKind}:${field.name}`,
    kind: 'field',
    name: field.name,
    typeName: typeRefToString(field.type as RawTypeRef),
    args: (field.args ?? []).map(mapArg),
    children: [],
    isSelectable: true,
    operationKind: opKind,
  };
}

function buildTypeGroup(
  label: string,
  kind: NodeKind,
  types: RawIntrospectionType[],
): SchemaTreeNode {
  const children: SchemaTreeNode[] = types.map((t) => ({
    id: `type:${kind}:${t.name!}`,
    kind,
    name: t.name!,
    children: [],
    isSelectable: false,
  }));

  return {
    id: `group:${kind}`,
    kind: 'operationGroup',
    name: label,
    children,
    isSelectable: false,
  };
}

function buildDirectivesGroup(
  directives: { name: string; locations: string[]; args: RawArgDef[] }[],
): SchemaTreeNode {
  const children: SchemaTreeNode[] = directives.map((d) => ({
    id: `directive:${d.name}`,
    kind: 'directive' as const,
    name: `@${d.name}`,
    children: [],
    isSelectable: false,
  }));

  return {
    id: 'group:directive',
    kind: 'operationGroup',
    name: 'Directives',
    children,
    isSelectable: false,
  };
}

function buildSchemaDocumentsGroup(docs: SchemaDocument[]): SchemaTreeNode {
  const children: SchemaTreeNode[] = docs.map((doc) => ({
    id: `doc:${doc.id}`,
    kind: 'schemaDocument' as const,
    name: doc.name,
    children: [],
    isSelectable: false,
    sdl: doc.content,
  }));

  return {
    id: 'group:schemaDocument',
    kind: 'operationGroup',
    name: 'Schema Documents',
    children,
    isSelectable: false,
  };
}

function mapArg(arg: RawArgDef): IntrospectionArg {
  const typeRef = arg.type as RawTypeRef;
  return {
    name: arg.name,
    typeName: unwrapTypeName(typeRef),
    isNonNull: typeRef.kind === 'NON_NULL',
  };
}

function errorMessage(err: unknown, fallback: string): string {
  if (err && typeof err === 'object' && 'message' in err) {
    return (err as { message: string }).message || fallback;
  }
  return fallback;
}


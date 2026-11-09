// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground-introspection.service.ts
 * @brief Loads and normalizes GraphQL introspection + uploaded schema documents
 *        into a single SchemaTreeNode[] for the playground schema tree (SP-011).
 *        Automatically reloads when a plugin is loaded/unloaded (SP-012).
 */

import { Injectable, inject, signal } from '@angular/core';
import {
  buildClientSchema,
  getIntrospectionQuery,
  type GraphQLSchema,
  type IntrospectionQuery,
} from 'graphql';
import { GraphQLService } from './graphql.service';
import { GraphQLSubscriptionService } from './graphql-subscription.service';
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

const INTROSPECTION_QUERY = getIntrospectionQuery({
  descriptions: true,
  directiveIsRepeatable: true,
  schemaDescription: true,
  specifiedByUrl: true,
});

const SCHEMA_DOCUMENTS_QUERY = `
  query SchemaDocuments {
    isched {
      schemaDocuments { id name content }
    }
  }
`;

const PLUGIN_REGISTRY_CHANGED_SUBSCRIPTION = `
  subscription PluginRegistryChanged {
    isched {
      pluginRegistryChanged {
        action
        pluginId
        timestamp
      }
    }
  }
`;

@Injectable({ providedIn: 'root' })
export class PlaygroundIntrospectionService {
  private readonly gql = inject(GraphQLService);
  private readonly gqlSubscription = inject(GraphQLSubscriptionService);

  readonly treeNodes = signal<SchemaTreeNode[]>([]);
  readonly loading = signal(false);
  readonly error = signal<string | null>(null);
  readonly schema = signal<GraphQLSchema | null>(null);
  readonly schemaStatus = signal<'loading' | 'ready' | 'unavailable'>('unavailable');

  load(): void {
    this.loading.set(true);
    this.error.set(null);
    this.schema.set(null);
    this.schemaStatus.set('loading');

    this.gql.query<IntrospectionQuery>(INTROSPECTION_QUERY).subscribe({
      next: (response) => {
        const schema = response?.__schema;
        if (schema) {
          try {
            const introspection = { __schema: schema } as IntrospectionQuery;
            this.schema.set(buildClientSchema(introspection));
            this.schemaStatus.set('ready');
            this.loadSchemaDocuments(introspection as unknown as RawIntrospectionResult);
          } catch (error: unknown) {
            this.loading.set(false);
            this.schemaStatus.set('unavailable');
            this.error.set(errorMessage(error, 'Failed to build the active GraphQL schema'));
          }
        } else {
          this.loading.set(false);
          this.schemaStatus.set('unavailable');
          this.error.set('Failed to load schema introspection: missing __schema');
        }
      },
      error: (err: unknown) => {
        this.loading.set(false);
        this.schemaStatus.set('unavailable');
        this.error.set(errorMessage(err, 'Failed to load schema introspection'));
      },
    });
  }

  /**
   * Begin watching for plugin registry changes over WebSocket.
   * Each `pluginRegistryChanged` event triggers a full schema tree reload.
   * The caller is responsible for unsubscribing (pass the returned subscription
   * to `takeUntilDestroyed` or unsubscribe manually).
   */
  watchPluginRegistry(): ReturnType<GraphQLSubscriptionService['subscribe']> {
    const obs = this.gqlSubscription.subscribe<{
      isched?: { pluginRegistryChanged?: { action: string; pluginId: string; timestamp: string } };
    }>(PLUGIN_REGISTRY_CHANGED_SUBSCRIPTION);

    obs.subscribe({
      next: (data) => {
        const event = (data as { isched?: { pluginRegistryChanged?: unknown } })?.isched?.pluginRegistryChanged;
        if (event) {
          // Skip the synthetic "initial" event emitted on subscribe
          const action = (event as { action: string }).action;
          if (action !== 'initial') {
            this.load();
          }
        }
      },
      error: () => {
        // Subscription errors are transient (network); reconnect is handled by
        // GraphQLSubscriptionService — silently ignore here.
      },
    });

    return obs;
  }

  private loadSchemaDocuments(introspection: RawIntrospectionResult): void {
    this.gql
      .query<{ isched?: { schemaDocuments?: SchemaDocument[] } }>(SCHEMA_DOCUMENTS_QUERY)
      .subscribe({
        next: (response) => {
          const schemaDocuments = response?.isched?.schemaDocuments ?? [];
          this.loading.set(false);
          this.treeNodes.set(buildTree(introspection, schemaDocuments));
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
    const group = buildOperationGroup('Queries', 'query', typeMap.get(queryTypeName), typeMap);
    if (group) nodes.push(group);
  }
  if (mutationTypeName) {
    const group = buildOperationGroup('Mutations', 'mutation', typeMap.get(mutationTypeName), typeMap);
    if (group) nodes.push(group);
  }
  if (subscriptionTypeName) {
    const group = buildOperationGroup('Subscriptions', 'subscription', typeMap.get(subscriptionTypeName), typeMap);
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
  typeMap: Map<string, RawIntrospectionType>,
): SchemaTreeNode | null {
  if (!type || !type.fields) return null;

  const visibleFields = type.fields.filter((f) => !f.name.startsWith('__'));

  const children: SchemaTreeNode[] = visibleFields.flatMap((f) => {
    const fieldTypeName = unwrapTypeName(f.type as RawTypeRef);
    const fieldType = typeMap.get(fieldTypeName);
    const fieldTypeFields = fieldType?.fields ?? [];

    // Only treat OBJECT types ending with "Namespace" as namespace containers
    if (
      fieldType?.kind === 'OBJECT' &&
      fieldTypeFields.length > 0 &&
      fieldTypeName.endsWith('Namespace')
    ) {
      // This field is a namespace — create a namespace node with nested fields
      return [buildNamespaceNode(f.name, fieldTypeFields, opKind, typeMap, [f.name])];
    }

    return [buildFieldNode(f, opKind)];
  });

  return {
    id: `group:${opKind}`,
    kind: 'operationGroup',
    name: label,
    children,
    isSelectable: false,
  };
}

function buildNamespaceNode(
  namespaceName: string,
  namespaceFields: RawFieldDef[],
  opKind: OperationKind,
  typeMap: Map<string, RawIntrospectionType>,
  namespacePath: string[] = [],
): SchemaTreeNode {
  const children: SchemaTreeNode[] = namespaceFields.flatMap((f) => {
    const fieldTypeName = unwrapTypeName(f.type as RawTypeRef);
    const fieldType = typeMap.get(fieldTypeName);
    const fieldTypeFields = fieldType?.fields ?? [];

    // Only treat OBJECT types ending with "Namespace" as nested namespace containers
    if (
      fieldType?.kind === 'OBJECT' &&
      fieldTypeFields.length > 0 &&
      fieldTypeName.endsWith('Namespace')
    ) {
      // Nested namespace — recursively create namespace node
      return [buildNamespaceNode(f.name, fieldTypeFields, opKind, typeMap, [...namespacePath, f.name])];
    }

    return [buildFieldNode(f, opKind, namespacePath)];
  });

  return {
    id: `namespace:${namespacePath.join('.')}`,
    kind: 'namespace',
    name: namespaceName,
    children,
    isSelectable: false,
  };
}

function buildFieldNode(
  field: RawFieldDef,
  opKind: OperationKind,
  namespacePath: string[] = [],
): SchemaTreeNode {
  return {
    id: `field:${opKind}:${namespacePath.join('.')}:${field.name}`,
    kind: 'field',
    name: field.name,
    typeName: typeRefToString(field.type as RawTypeRef),
    args: (field.args ?? []).map(mapArg),
    children: [],
    isSelectable: true,
    operationKind: opKind,
    namespacePath,
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

// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground-introspection.models.ts
 * @brief Data models for the GraphQL Playground schema tree (SP-011)
 */

export type NodeKind =
  | 'operationGroup'
  | 'field'
  | 'objectType'
  | 'inputType'
  | 'enumType'
  | 'directive'
  | 'interfaceType'
  | 'scalarType'
  | 'schemaDocument';

export type OperationKind = 'query' | 'mutation' | 'subscription';

export interface SchemaTreeNode {
  /** Unique identifier for this node in the tree */
  id: string;
  kind: NodeKind;
  name: string;
  /** Return type name for field nodes */
  typeName?: string;
  /** Argument definitions for field nodes */
  args?: IntrospectionArg[];
  children: SchemaTreeNode[];
  /** True for operation fields that can be selected and generate a query */
  isSelectable: boolean;
  /** For schemaDocument nodes – raw SDL content */
  sdl?: string;
  /** Which root operation type this field belongs to (for stub generation) */
  operationKind?: OperationKind;
}

export interface IntrospectionArg {
  name: string;
  typeName: string;
  isNonNull: boolean;
}

// ─── Raw introspection shapes ───────────────────────────────────────────────

export interface RawIntrospectionType {
  kind: string;
  name: string | null;
  fields?: RawFieldDef[] | null;
  inputFields?: RawInputFieldDef[] | null;
  enumValues?: { name: string }[] | null;
  interfaces?: { name: string }[] | null;
  possibleTypes?: { name: string }[] | null;
}

export interface RawFieldDef {
  name: string;
  type: RawTypeRef;
  args?: RawArgDef[];
}

export interface RawInputFieldDef {
  name: string;
  type: RawTypeRef;
}

export interface RawArgDef {
  name: string;
  type: RawTypeRef;
}

export interface RawTypeRef {
  kind: string;
  name: string | null;
  ofType: RawTypeRef | null;
}

export interface RawIntrospectionResult {
  __schema: {
    queryType: { name: string } | null;
    mutationType: { name: string } | null;
    subscriptionType: { name: string } | null;
    types: RawIntrospectionType[];
    directives?: { name: string; locations: string[]; args: RawArgDef[] }[];
  };
}

export interface SchemaDocument {
  id: string;
  name: string;
  content: string;
}

/** Unwrap NON_NULL / LIST wrappers to get the named type name */
export function unwrapTypeName(typeRef: RawTypeRef | null): string {
  if (!typeRef) return 'Unknown';
  if (typeRef.name) return typeRef.name;
  return unwrapTypeName(typeRef.ofType);
}

/** Produce a string like "String!", "[ID!]!" for display */
export function typeRefToString(typeRef: RawTypeRef | null): string {
  if (!typeRef) return 'Unknown';
  if (typeRef.kind === 'NON_NULL') return `${typeRefToString(typeRef.ofType)}!`;
  if (typeRef.kind === 'LIST') return `[${typeRefToString(typeRef.ofType)}]`;
  return typeRef.name ?? 'Unknown';
}


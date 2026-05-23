// SPDX-License-Identifier: MPL-2.0
/**
 * @file query-stub-generator.ts
 * @brief Pure helper that generates a minimal valid GraphQL operation stub from a SchemaTreeNode (SP-011).
 */

import { IntrospectionArg, OperationKind, SchemaTreeNode } from '../../services/playground-introspection.models';

const SCALAR_PLACEHOLDERS: Record<string, string> = {
  String: '"..."',
  ID: '"..."',
  Int: '0',
  Float: '0.0',
  Boolean: 'false',
};

function placeholderForType(typeName: string): string {
  return SCALAR_PLACEHOLDERS[typeName] ?? '{}';
}

function buildArgs(args: IntrospectionArg[]): string {
  if (args.length === 0) return '';
  const parts = args.map((a) => `${a.name}: ${placeholderForType(a.typeName)}`);
  return `(${parts.join(', ')})`;
}

function operationKeyword(kind: OperationKind): string {
  switch (kind) {
    case 'query': return 'query';
    case 'mutation': return 'mutation';
    case 'subscription': return 'subscription';
  }
}

/**
 * Generate a minimal valid GraphQL operation stub from a selectable field node.
 * @param node - A SchemaTreeNode with isSelectable === true
 * @returns A GraphQL operation string ready to paste into the editor
 */
export function generateStub(node: SchemaTreeNode): string {
  const opKind = node.operationKind ?? 'query';
  const keyword = operationKeyword(opKind);
  const args = buildArgs(node.args ?? []);
  const operationName = capitalize(node.name);

  // Determine selection set: if the return type looks like a scalar, omit nested selection.
  // For object-like returns use __typename, which is valid on all composite GraphQL object results.
  const typeName = node.typeName?.replace(/[!\[\]]/g, '') ?? '';
  const isScalarLike = isLikelyScalar(typeName);
  const selectionSet = isScalarLike ? '' : ' {\n    __typename\n  }';

  return `${keyword} ${operationName} {\n  ${node.name}${args}${selectionSet}\n}`;
}

function capitalize(s: string): string {
  return s.charAt(0).toUpperCase() + s.slice(1);
}

const KNOWN_SCALARS = new Set(['String', 'Int', 'Float', 'Boolean', 'ID']);

function isLikelyScalar(typeName: string): boolean {
  return KNOWN_SCALARS.has(typeName) || typeName === '' || typeName === 'Unknown';
}


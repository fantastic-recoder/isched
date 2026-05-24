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

  const typeName = node.typeName?.replace(/[!\[\]]/g, '') ?? '';
  const isScalarLike = isLikelyScalar(typeName);

  const fieldSelection = isScalarLike
    ? `${node.name}${args}`
    : `${node.name}${args} {\n      __typename\n    }`;

  const wrappedSelection = wrapWithNamespaces(fieldSelection, node.namespacePath ?? []);

  return `${keyword} ${operationName} {\n${indentBy(wrappedSelection, 2)}\n}`;
}

function capitalize(s: string): string {
  return s.charAt(0).toUpperCase() + s.slice(1);
}

const KNOWN_SCALARS = new Set(['String', 'Int', 'Float', 'Boolean', 'ID']);

function isLikelyScalar(typeName: string): boolean {
  return KNOWN_SCALARS.has(typeName) || typeName === '' || typeName === 'Unknown';
}

function wrapWithNamespaces(selection: string, namespacePath: string[]): string {
  let result = selection;
  for (let i = namespacePath.length - 1; i >= 0; i -= 1) {
    result = `${namespacePath[i]} {\n${indentBy(result, 2)}\n}`;
  }
  return result;
}

function indentBy(text: string, spaces: number): string {
  const prefix = ' '.repeat(spaces);
  return text
    .split('\n')
    .map((line) => `${prefix}${line}`)
    .join('\n');
}

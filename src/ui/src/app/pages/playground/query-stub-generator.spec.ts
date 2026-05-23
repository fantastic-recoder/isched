// SPDX-License-Identifier: MPL-2.0
/**
 * @file query-stub-generator.spec.ts
 * @brief Unit tests for the generateStub pure function (SP-011)
 */

import { generateStub } from './query-stub-generator';
import { SchemaTreeNode } from '../../services/playground-introspection.models';

function makeFieldNode(overrides: Partial<SchemaTreeNode>): SchemaTreeNode {
  return {
    id: 'field:query:test',
    kind: 'field',
    name: 'test',
    typeName: 'String',
    args: [],
    children: [],
    isSelectable: true,
    operationKind: 'query',
    ...overrides,
  };
}

describe('generateStub', () => {
  it('generates a query stub for a scalar return field with no args', () => {
    const stub = generateStub(makeFieldNode({ name: 'version', typeName: 'String' }));
    expect(stub).toContain('query Version');
    expect(stub).toContain('version');
    // scalar — no selection set braces after field name
    expect(stub).not.toContain('version {');
  });

  it('generates a query stub with selection set for object return type', () => {
    const stub = generateStub(makeFieldNode({ name: 'health', typeName: 'Health' }));
    expect(stub).toContain('query Health');
    expect(stub).toContain('health {');
    expect(stub).toContain('__typename');
  });

  it('includes ID argument placeholder', () => {
    const stub = generateStub(
      makeFieldNode({
        name: 'organization',
        typeName: 'Organization',
        args: [{ name: 'id', typeName: 'ID', isNonNull: true }],
      }),
    );
    expect(stub).toContain('id: "..."');
  });

  it('includes String argument placeholder', () => {
    const stub = generateStub(
      makeFieldNode({
        name: 'search',
        typeName: 'String',
        args: [{ name: 'query', typeName: 'String', isNonNull: true }],
      }),
    );
    expect(stub).toContain('query: "..."');
  });

  it('includes Int argument placeholder', () => {
    const stub = generateStub(
      makeFieldNode({
        name: 'listItems',
        typeName: '[Item]',
        args: [{ name: 'limit', typeName: 'Int', isNonNull: false }],
      }),
    );
    expect(stub).toContain('limit: 0');
  });

  it('includes Boolean argument placeholder', () => {
    const stub = generateStub(
      makeFieldNode({
        name: 'listUsers',
        typeName: '[User]',
        args: [{ name: 'active', typeName: 'Boolean', isNonNull: false }],
      }),
    );
    expect(stub).toContain('active: false');
  });

  it('uses {} placeholder for custom input types', () => {
    const stub = generateStub(
      makeFieldNode({
        name: 'createOrg',
        typeName: 'Organization',
        args: [{ name: 'input', typeName: 'CreateOrgInput', isNonNull: true }],
      }),
    );
    expect(stub).toContain('input: {}');
  });

  it('generates mutation stub with mutation keyword', () => {
    const stub = generateStub(
      makeFieldNode({ name: 'login', typeName: 'String', operationKind: 'mutation' }),
    );
    expect(stub).toContain('mutation Login');
  });

  it('generates subscription stub with subscription keyword', () => {
    const stub = generateStub(
      makeFieldNode({ name: 'events', typeName: 'Event', operationKind: 'subscription' }),
    );
    expect(stub).toContain('subscription Events');
  });

  it('handles node with no args array', () => {
    const stub = generateStub(makeFieldNode({ args: undefined, typeName: 'String' }));
    expect(stub).toBeTruthy();
    expect(stub).not.toContain('(');
  });

  it('generates valid stub for Float argument', () => {
    const stub = generateStub(
      makeFieldNode({
        name: 'calc',
        typeName: 'Float',
        args: [{ name: 'value', typeName: 'Float', isNonNull: true }],
      }),
    );
    expect(stub).toContain('value: 0.0');
  });
});


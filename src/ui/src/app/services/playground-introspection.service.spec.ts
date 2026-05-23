// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground-introspection.service.spec.ts
 * @brief Unit tests for PlaygroundIntrospectionService (SP-011)
 */

import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { PlaygroundIntrospectionService } from './playground-introspection.service';

const MINIMAL_INTROSPECTION = {
  __schema: {
    queryType: { name: 'Query' },
    mutationType: { name: 'Mutation' },
    subscriptionType: null,
    types: [
      {
        kind: 'OBJECT',
        name: 'Query',
        fields: [
          {
            name: 'health',
            type: { kind: 'OBJECT', name: 'Health', ofType: null },
            args: [],
          },
        ],
        inputFields: null,
        enumValues: null,
        interfaces: [],
        possibleTypes: null,
      },
      {
        kind: 'OBJECT',
        name: 'Mutation',
        fields: [
          {
            name: 'login',
            type: { kind: 'SCALAR', name: 'String', ofType: null },
            args: [],
          },
        ],
        inputFields: null,
        enumValues: null,
        interfaces: [],
        possibleTypes: null,
      },
      {
        kind: 'OBJECT',
        name: 'Health',
        fields: [
          {
            name: 'status',
            type: { kind: 'SCALAR', name: 'String', ofType: null },
            args: [],
          },
        ],
        inputFields: null,
        enumValues: null,
        interfaces: [],
        possibleTypes: null,
      },
    ],
    directives: [],
  },
};

describe('PlaygroundIntrospectionService', () => {
  let service: PlaygroundIntrospectionService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [
        PlaygroundIntrospectionService,
        provideHttpClient(),
        provideHttpClientTesting(),
      ],
    });
    service = TestBed.inject(PlaygroundIntrospectionService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('starts with empty tree, not loading, no error', () => {
    expect(service.treeNodes()).toEqual([]);
    expect(service.loading()).toBe(false);
    expect(service.error()).toBeNull();
  });

  it('sets loading=true immediately after load() is called', () => {
    service.load();
    expect(service.loading()).toBe(true);
    httpMock.expectOne('/graphql').flush({ data: MINIMAL_INTROSPECTION });
    httpMock.expectOne('/graphql').flush({ data: { schemaDocuments: [] } });
  });

  it('populates treeNodes with operation groups after successful load', () => {
    service.load();

    httpMock.expectOne('/graphql').flush({ data: MINIMAL_INTROSPECTION });
    httpMock.expectOne('/graphql').flush({ data: { schemaDocuments: [] } });

    const nodes = service.treeNodes();
    expect(nodes.length).toBeGreaterThan(0);

    const queryGroup = nodes.find((n) => n.name === 'Queries');
    expect(queryGroup).toBeDefined();
    expect(queryGroup!.children.length).toBeGreaterThan(0);
    expect(queryGroup!.children[0].name).toBe('health');
    expect(queryGroup!.children[0].isSelectable).toBe(true);

    const mutationGroup = nodes.find((n) => n.name === 'Mutations');
    expect(mutationGroup).toBeDefined();

    expect(service.loading()).toBe(false);
    expect(service.error()).toBeNull();
  });

  it('merges schema documents into the tree', () => {
    service.load();

    httpMock.expectOne('/graphql').flush({ data: MINIMAL_INTROSPECTION });
    httpMock.expectOne('/graphql').flush({
      data: {
        schemaDocuments: [{ id: 'doc-1', name: 'SchedulerSchema', content: 'type Foo { id: ID }' }],
      },
    });

    const nodes = service.treeNodes();
    const docGroup = nodes.find((n) => n.name === 'Schema Documents');
    expect(docGroup).toBeDefined();
    expect(docGroup!.children[0].name).toBe('SchedulerSchema');
  });

  it('sets error signal and clears loading when introspection fails', () => {
    service.load();

    httpMock.expectOne('/graphql').flush(
      { errors: [{ message: 'Unauthorized' }] },
      { status: 200, statusText: 'OK' },
    );

    expect(service.loading()).toBe(false);
    expect(service.error()).toBeTruthy();
  });

  it('continues without schema documents when schemaDocuments request fails', () => {
    service.load();

    httpMock.expectOne('/graphql').flush({ data: MINIMAL_INTROSPECTION });
    httpMock.expectOne('/graphql').error(new ProgressEvent('network error'));

    const nodes = service.treeNodes();
    expect(nodes.length).toBeGreaterThan(0);
    expect(service.loading()).toBe(false);

    // No Schema Documents group when request fails
    const docGroup = nodes.find((n) => n.name === 'Schema Documents');
    expect(docGroup).toBeUndefined();
  });

  it('does not include subscription group when subscriptionType is null', () => {
    service.load();
    httpMock.expectOne('/graphql').flush({ data: MINIMAL_INTROSPECTION });
    httpMock.expectOne('/graphql').flush({ data: { schemaDocuments: [] } });

    const nodes = service.treeNodes();
    const subGroup = nodes.find((n) => n.name === 'Subscriptions');
    expect(subGroup).toBeUndefined();
  });
});


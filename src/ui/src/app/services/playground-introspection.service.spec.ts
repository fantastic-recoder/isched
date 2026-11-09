// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground-introspection.service.spec.ts
 * @brief Unit tests for PlaygroundIntrospectionService (SP-011)
 */

import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { buildSchema, introspectionFromSchema } from 'graphql';
import { PlaygroundIntrospectionService } from './playground-introspection.service';

const MINIMAL_INTROSPECTION = introspectionFromSchema(
  buildSchema(`
    schema { query: Query mutation: Mutation }
    type Query { isched: IschedQueryNamespace }
    type Mutation { isched: IschedMutationNamespace }
    type IschedQueryNamespace { health: Health }
    type IschedMutationNamespace { login: String }
    type Health { """Current service health.""" status: String }
  `),
);

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
    expect(service.schema()).toBeNull();
    expect(service.schemaStatus()).toBe('unavailable');
  });

   it('sets loading=true immediately after load() is called', () => {
     service.load();
     expect(service.loading()).toBe(true);
     httpMock.expectOne('/graphql').flush({ data: MINIMAL_INTROSPECTION });
     httpMock.expectOne('/graphql').flush({ data: { isched: { schemaDocuments: [] } } });
   });

    it('populates treeNodes with operation groups after successful load', () => {
      service.load();

      httpMock.expectOne('/graphql').flush({ data: MINIMAL_INTROSPECTION });
      httpMock.expectOne('/graphql').flush({ data: { isched: { schemaDocuments: [] } } });

      const nodes = service.treeNodes();
      expect(nodes.length).toBeGreaterThan(0);

      const queryGroup = nodes.find((n) => n.name === 'Queries');
      expect(queryGroup).toBeDefined();
      expect(queryGroup!.children.length).toBeGreaterThan(0);

      // First child should be the namespace node
      const ischedNamespace = queryGroup!.children.find((n) => n.name === 'isched');
      expect(ischedNamespace).toBeDefined();
      expect(ischedNamespace!.kind).toBe('namespace');
      expect(ischedNamespace!.children.length).toBeGreaterThan(0);

      // The field is nested under the namespace
      const healthField = ischedNamespace!.children.find((n) => n.name === 'health');
      expect(healthField).toBeDefined();
      expect(healthField!.isSelectable).toBe(true);

      const mutationGroup = nodes.find((n) => n.name === 'Mutations');
      expect(mutationGroup).toBeDefined();

      expect(service.loading()).toBe(false);
      expect(service.error()).toBeNull();
      expect(service.schema()).not.toBeNull();
      expect(service.schemaStatus()).toBe('ready');
    });

  it('requests a complete described schema through the GraphQL endpoint', () => {
    service.load();

    const request = httpMock.expectOne('/graphql');
    expect(request.request.body.query).toContain('description');
    expect(request.request.body.query).toContain('inputFields');
    expect(request.request.body.query).toContain('enumValues');
    expect(request.request.body.query).toContain('args');
    request.flush({ data: MINIMAL_INTROSPECTION });
    httpMock.expectOne('/graphql').flush({ data: { isched: { schemaDocuments: [] } } });

    expect(service.schemaStatus()).toBe('ready');
    expect(service.schema()?.getQueryType()?.getFields()['isched'].type).toBeDefined();
  });

  it('retains described field, argument, input, enum, and nested type references', () => {
    const describedSchema = buildSchema(`
      """Root query description."""
      type Query {
        """Find matching records."""
        search(
          """Search constraints."""
          filter: SearchFilter!,
          mode: SearchMode = ALL
        ): [SearchResult!]!
      }
      """Filter input description."""
      input SearchFilter {
        """Text matched by the search."""
        text: String!
      }
      """Search modes."""
      enum SearchMode {
        """Return every matching record."""
        ALL
      }
      """A matching record."""
      type SearchResult { id: ID! }
    `);
    const completeIntrospection = introspectionFromSchema(describedSchema);

    service.load();
    httpMock.expectOne('/graphql').flush({ data: completeIntrospection });
    httpMock.expectOne('/graphql').flush({ data: { isched: { schemaDocuments: [] } } });

    const schema = service.schema();
    expect(schema?.getQueryType()?.description).toBe('Root query description.');
    const search = schema?.getQueryType()?.getFields()['search'];
    expect(search?.description).toBe('Find matching records.');
    expect(search?.args[0].type.toString()).toBe('SearchFilter!');
    expect(search?.args[0].description).toBe('Search constraints.');
    expect(search?.type.toString()).toBe('[SearchResult!]!');
    expect(schema?.getType('SearchFilter')?.description).toBe('Filter input description.');
    const filterType = schema?.getType('SearchFilter');
    expect(filterType && 'getFields' in filterType).toBe(true);
    if (filterType && 'getFields' in filterType) {
      expect(filterType.getFields()['text'].description).toBe('Text matched by the search.');
    }
    expect(schema?.getType('SearchMode')?.description).toBe('Search modes.');
    const modeType = schema?.getType('SearchMode');
    expect(modeType && 'getValues' in modeType).toBe(true);
    if (modeType && 'getValues' in modeType) {
      expect(modeType.getValues()[0].description).toBe('Return every matching record.');
    }
    expect(schema?.getType('SearchResult')?.description).toBe('A matching record.');
  });

   it('merges schema documents into the tree', () => {
     service.load();

     httpMock.expectOne('/graphql').flush({ data: MINIMAL_INTROSPECTION });
     httpMock.expectOne('/graphql').flush({
       data: {
         isched: {
           schemaDocuments: [{ id: 'doc-1', name: 'SchedulerSchema', content: 'type Foo { id: ID }' }],
         },
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
    expect(service.schemaStatus()).toBe('unavailable');
    expect(service.schema()).toBeNull();
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
      httpMock.expectOne('/graphql').flush({ data: { isched: { schemaDocuments: [] } } });

      const nodes = service.treeNodes();
      const subGroup = nodes.find((n) => n.name === 'Subscriptions');
      expect(subGroup).toBeUndefined();
    });

    it('supports nested namespaces (namespace within namespace)', () => {
      const nestedIntrospection = introspectionFromSchema(
        buildSchema(`
          schema { query: Query }
          type Query { admin: AdminQueryNamespace }
          type AdminQueryNamespace { reports: ReportsNamespace }
          type ReportsNamespace { systemMetrics: String }
        `),
      );

      service.load();
      httpMock.expectOne('/graphql').flush({ data: nestedIntrospection });
      httpMock.expectOne('/graphql').flush({ data: { isched: { schemaDocuments: [] } } });

      const nodes = service.treeNodes();
      const queryGroup = nodes.find((n) => n.name === 'Queries');
      expect(queryGroup).toBeDefined();

      // First level: admin namespace
      const adminNamespace = queryGroup!.children.find((n) => n.name === 'admin');
      expect(adminNamespace).toBeDefined();
      expect(adminNamespace!.kind).toBe('namespace');

      // Second level: reports nested namespace
      const reportsNamespace = adminNamespace!.children.find((n) => n.name === 'reports');
      expect(reportsNamespace).toBeDefined();
      expect(reportsNamespace!.kind).toBe('namespace');

      // Third level: systemMetrics field
      const metricsField = reportsNamespace!.children.find((n) => n.name === 'systemMetrics');
      expect(metricsField).toBeDefined();
      expect(metricsField!.kind).toBe('field');
      expect(metricsField!.isSelectable).toBe(true);
    });
});

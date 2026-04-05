// SPDX-License-Identifier: MPL-2.0
/**
 * @file organization.service.spec.ts
 * @brief Unit tests for OrganizationService – verifies the GraphQL query/mutation
 *        shapes match the backend contract (TDD for organizations connection API).
 */

import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { OrganizationService } from './organization.service';
import { GraphQLService } from './graphql.service';

describe('OrganizationService', () => {
  let service: OrganizationService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [OrganizationService, GraphQLService, provideHttpClient(), provideHttpClientTesting()],
    });
    service = TestBed.inject(OrganizationService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => httpMock.verify());

  // ---------------------------------------------------------------------------
  // listOrganizations — connection-shape contract
  // ---------------------------------------------------------------------------

  it('should POST a paginated organizations query with page/sort/filter variables', () => {
    service.listOrganizations({
      page: { number: 1, size: 10 },
      sort: [{ field: 'name', direction: 'ASC' }],
    }).subscribe();

    const req = httpMock.expectOne('/graphql');
    expect(req.request.method).toBe('POST');

    const body = req.request.body as { query: string; variables: unknown };
    expect(body.query).toContain('organizations');
    expect(body.query).toContain('PageInput');
    expect(body.query).toContain('SortInput');
    expect(body.query).toContain('nodes');
    expect(body.query).toContain('pageInfo');
    expect(body.variables).toMatchObject({
      page: { number: 1, size: 10 },
      sort: [{ field: 'name', direction: 'ASC' }],
    });

    req.flush({
      data: {
        organizations: {
          nodes: [{ id: 'org-1', name: 'Acme', status: 'ACTIVE', revision: 0, updatedAt: '2026-04-05T00:00:00Z' }],
          pageInfo: { number: 1, size: 10, totalElements: 1, totalPages: 1 },
        },
      },
    });
  });

  it('should request status, revision, and updatedAt fields on each organization node', () => {
    service.listOrganizations().subscribe();

    const req = httpMock.expectOne('/graphql');
    const body = req.request.body as { query: string };
    // These fields are required by the WebUI organizations list
    expect(body.query).toContain('status');
    expect(body.query).toContain('revision');
    expect(body.query).toContain('updatedAt');

    req.flush({
      data: {
        organizations: {
          nodes: [],
          pageInfo: { number: 1, size: 10, totalElements: 0, totalPages: 0 },
        },
      },
    });
  });

  it('should unwrap the connection and return nodes + pageInfo', (done) => {
    const mockNodes = [
      { id: 'org-1', name: 'Acme', status: 'ACTIVE' as const, revision: 2, updatedAt: '2026-04-05T00:00:00Z' },
    ];
    const mockPageInfo = { number: 1, size: 10, totalElements: 1, totalPages: 1 };

    service.listOrganizations().subscribe((conn) => {
      expect(conn.nodes).toEqual(mockNodes);
      expect(conn.pageInfo).toEqual(mockPageInfo);
      done();
    });

    httpMock.expectOne('/graphql').flush({
      data: { organizations: { nodes: mockNodes, pageInfo: mockPageInfo } },
    });
  });

  // ---------------------------------------------------------------------------
  // updateOrganization — expectedRevision top-level argument contract
  // ---------------------------------------------------------------------------

  it('should include expectedRevision as a top-level mutation variable', () => {
    service.updateOrganization('org-1', { name: 'NewName' }, 3).subscribe();

    const req = httpMock.expectOne('/graphql');
    const body = req.request.body as { query: string; variables: unknown };

    // expectedRevision must be a top-level variable, NOT nested inside input
    expect(body.query).toContain('$expectedRevision');
    expect(body.query).toContain('expectedRevision: $expectedRevision');
    expect(body.variables).toMatchObject({ id: 'org-1', expectedRevision: 3, input: { name: 'NewName' } });

    req.flush({
      data: {
        updateOrganization: {
          id: 'org-1', name: 'NewName', status: 'ACTIVE', revision: 4, updatedAt: '2026-04-05T01:00:00Z',
        },
      },
    });
  });

  it('should return updated organization fields including status and revision', (done) => {
    service.updateOrganization('org-1', { status: 'SUSPENDED' }, 1).subscribe((org) => {
      expect(org.id).toBe('org-1');
      expect(org.status).toBe('SUSPENDED');
      expect(org.revision).toBe(2);
      done();
    });

    httpMock.expectOne('/graphql').flush({
      data: {
        updateOrganization: {
          id: 'org-1', name: 'Acme', status: 'SUSPENDED', revision: 2, updatedAt: '2026-04-05T01:00:00Z',
        },
      },
    });
  });

  // ---------------------------------------------------------------------------
  // createOrganization
  // ---------------------------------------------------------------------------

  it('should return created organization with status, revision, and updatedAt', (done) => {
    service.createOrganization({ name: 'NewOrg' }).subscribe((org) => {
      expect(org.status).toBe('ACTIVE');
      expect(org.revision).toBe(0);
      expect(org.updatedAt).toBeTruthy();
      done();
    });

    httpMock.expectOne('/graphql').flush({
      data: {
        createOrganization: {
          id: 'org-new', name: 'NewOrg', status: 'ACTIVE', revision: 0, updatedAt: '2026-04-05T02:00:00Z',
        },
      },
    });
  });
});


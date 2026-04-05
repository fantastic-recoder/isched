import { Injectable, inject } from '@angular/core';
import { map, Observable } from 'rxjs';
import { GraphQLService } from './graphql.service';

export interface PageInput {
  number: number;
  size: number;
}

export interface SortInput {
  field: string;
  direction: 'ASC' | 'DESC';
}

export interface FilterInput {
  field: string;
  op: string;
  value: string;
}

export interface PageInfo {
  number: number;
  size: number;
  totalElements: number;
  totalPages: number;
}

export interface Organization {
  id: string;
  name: string;
  status: 'ACTIVE' | 'SUSPENDED';
  revision: number;
  updatedAt: string;
}

export interface OrganizationConnection {
  nodes: Organization[];
  pageInfo: PageInfo;
}

export interface OrganizationListOptions {
  page?: PageInput;
  sort?: SortInput[];
  filter?: FilterInput[];
}

export interface CreateOrganizationInput {
  name: string;
}

export interface UpdateOrganizationInput {
  name?: string;
  status?: Organization['status'];
}

interface OrganizationsResponse {
  organizations: OrganizationConnection;
}

interface CreateOrganizationResponse {
  createOrganization: Organization;
}

interface UpdateOrganizationResponse {
  updateOrganization: Organization;
}

const DEFAULT_PAGE: PageInput = { number: 1, size: 10 };
const DEFAULT_SORT: SortInput[] = [{ field: 'name', direction: 'ASC' }];

@Injectable({ providedIn: 'root' })
export class OrganizationService {
  private readonly gql = inject(GraphQLService);

  listOrganizations(options: OrganizationListOptions = {}): Observable<OrganizationConnection> {
    return this.gql
      .query<OrganizationsResponse>(
        `query Organizations($page: PageInput!, $sort: [SortInput!], $filter: [FilterInput!]) {
          organizations(page: $page, sort: $sort, filter: $filter) {
            nodes {
              id
              name
              status
              revision
              updatedAt
            }
            pageInfo {
              number
              size
              totalElements
              totalPages
            }
          }
        }`,
        {
          page: options.page ?? DEFAULT_PAGE,
          sort: options.sort ?? DEFAULT_SORT,
          filter: options.filter,
        },
      )
      .pipe(map(({ organizations }) => organizations));
  }

  createOrganization(input: CreateOrganizationInput): Observable<Organization> {
    return this.gql
      .mutate<CreateOrganizationResponse>(
        `mutation CreateOrganization($input: CreateOrganizationInput!) {
          createOrganization(input: $input) {
            id
            name
            status
            revision
            updatedAt
          }
        }`,
        { input },
      )
      .pipe(map(({ createOrganization }) => createOrganization));
  }

  updateOrganization(
    id: string,
    input: UpdateOrganizationInput,
    expectedRevision: number,
  ): Observable<Organization> {
    return this.gql
      .mutate<UpdateOrganizationResponse>(
        `mutation UpdateOrganization($id: ID!, $input: UpdateOrganizationInput!, $expectedRevision: Int!) {
          updateOrganization(id: $id, input: $input, expectedRevision: $expectedRevision) {
            id
            name
            status
            revision
            updatedAt
          }
        }`,
        { id, input, expectedRevision },
      )
      .pipe(map(({ updateOrganization }) => updateOrganization));
  }
}


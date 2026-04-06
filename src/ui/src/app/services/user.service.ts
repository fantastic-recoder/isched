import { Injectable, inject } from '@angular/core';
import { catchError, defer, map, Observable, tap, throwError } from 'rxjs';
import { OrgContextService } from './org-context.service';
import { FilterInput, PageInput, PageInfo, SortInput } from './organization.service';
import { GraphQLRequestError, GraphQLService } from './graphql.service';
import { ShellStatusService } from './shell-status.service';

export interface RoleAssignmentSummary {
  roleId: string;
  effective: boolean;
}

export interface UserRecord {
  id: string;
  organizationId: string;
  loginId: string;
  displayName: string;
  status: 'ACTIVE' | 'DISABLED';
  revision: number;
  updatedAt: string;
  roleAssignments: RoleAssignmentSummary[];
}

export interface UserConnection {
  nodes: UserRecord[];
  pageInfo: PageInfo;
}

export interface UserListOptions {
  organizationId: string;
  page?: PageInput;
  sort?: SortInput[];
  filter?: FilterInput[];
}

export interface CreateUserInput {
  loginId: string;
  displayName: string;
}

export interface UpdateUserInput {
  loginId?: string;
  displayName?: string;
  status?: UserRecord['status'];
}

interface UsersResponse {
  users: UserConnection;
}

interface CreateUserResponse {
  createUser: UserRecord;
}

interface UpdateUserResponse {
  updateUser: UserRecord;
}

const DEFAULT_PAGE: PageInput = { number: 1, size: 10 };
const DEFAULT_SORT: SortInput[] = [{ field: 'displayName', direction: 'ASC' }];

@Injectable({ providedIn: 'root' })
export class UserService {
  private readonly gql = inject(GraphQLService);
  private readonly orgContext = inject(OrgContextService);
  private readonly shellStatus = inject(ShellStatusService);

  listUsers(options: UserListOptions): Observable<UserConnection> {
    return defer(() => {
      const sequence = this.shellStatus.beginOperation(
        'organization-users:list',
        'Loading organization users',
      );

      return this.gql
        .query<UsersResponse>(
          `query Users($organizationId: ID!, $page: PageInput!, $sort: [SortInput!], $filter: [FilterInput!]) {
            users(organizationId: $organizationId, page: $page, sort: $sort, filter: $filter) {
              nodes {
                id
                organizationId
                loginId
                displayName
                status
                revision
                updatedAt
                roleAssignments {
                  roleId
                  effective
                }
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
            organizationId: options.organizationId,
            page: options.page ?? DEFAULT_PAGE,
            sort: options.sort ?? DEFAULT_SORT,
            filter: options.filter,
          },
        )
        .pipe(
          map(({ users }) => users),
          tap(() => {
            this.shellStatus.completeOperation(
              'organization-users:list',
              'Organization users loaded',
              sequence,
            );
          }),
          catchError((error: unknown) => {
            this.shellStatus.failOperation(
              'organization-users:list',
              this.organizationUsersFailureMessage(error),
              sequence,
            );
            return throwError(() => error);
          }),
        );
    });
  }

  createUser(organizationId: string, input: CreateUserInput): Observable<UserRecord> {
    return defer(() => {
      const scopeCheck = this.orgContext.validateMutationScope(organizationId);
      if (!scopeCheck.ok) {
        return throwError(() => new GraphQLRequestError(scopeCheck.message, scopeCheck.code));
      }

      return this.gql
        .mutate<CreateUserResponse>(
          `mutation CreateUser($organizationId: ID!, $input: CreateUserInput!) {
            createUser(organizationId: $organizationId, input: $input) {
              id
              organizationId
              loginId
              displayName
              status
              revision
              updatedAt
              roleAssignments {
                roleId
                effective
              }
            }
          }`,
          { organizationId, input },
        )
        .pipe(map(({ createUser }) => createUser));
    });
  }

  updateUser(
    organizationId: string,
    id: string,
    input: UpdateUserInput,
    expectedRevision: number,
  ): Observable<UserRecord> {
    return defer(() => {
      const scopeCheck = this.orgContext.validateMutationScope(organizationId);
      if (!scopeCheck.ok) {
        return throwError(() => new GraphQLRequestError(scopeCheck.message, scopeCheck.code));
      }

      return this.gql
        .mutate<UpdateUserResponse>(
          `mutation UpdateUser($organizationId: ID!, $id: ID!, $input: UpdateUserInput!, $expectedRevision: Int!) {
            updateUser(
              organizationId: $organizationId
              id: $id
              input: $input
              expectedRevision: $expectedRevision
            ) {
              id
              organizationId
              loginId
              displayName
              status
              revision
              updatedAt
              roleAssignments {
                roleId
                effective
              }
            }
          }`,
          { organizationId, id, input, expectedRevision },
        )
        .pipe(map(({ updateUser }) => updateUser));
    });
  }

  private organizationUsersFailureMessage(error: unknown): string {
    if (error instanceof GraphQLRequestError) {
      return 'Unable to load organization users';
    }

    return error instanceof Error && error.message.trim().length > 0
      ? 'Unable to load organization users'
      : 'Unable to load organization users';
  }
}


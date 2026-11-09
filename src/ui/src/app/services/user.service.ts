import { Injectable, inject } from '@angular/core';
import { catchError, defer, map, Observable, tap, throwError } from 'rxjs';
import { OrgContextService } from './org-context.service';
import { FilterInput, PageInput, PageInfo, SortInput } from './organization.service';
import { GraphQLRequestError, GraphQLService } from './graphql.service';
import { ShellStatusService } from './shell-status.service';

export interface UserRecord {
  id: string;
  organizationId: string;
  email: string;
  displayName: string;
  status: 'ACTIVE' | 'DISABLED';
  roles: string[];
  createdAt: string;
  lastLogin: string | null;
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
  email: string;
  password: string;
  displayName: string;
}

export interface UpdateUserInput {
  displayName?: string;
  roles?: string[];
  isActive?: boolean;
}

interface GraphQLUserRecord {
  id: string;
  email: string;
  displayName: string;
  roles: string[];
  isActive: boolean;
  createdAt: string;
  lastLogin: string | null;
}

interface UsersResponse {
  isched: {
    users: GraphQLUserRecord[];
  };
}

interface CreateUserResponse {
  isched: {
    createUser: GraphQLUserRecord;
  };
}

interface UpdateUserResponse {
  isched: {
    updateUser: GraphQLUserRecord;
  };
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
          `query Users($organizationId: ID) {
            isched {
              users(organizationId: $organizationId) {
                id
                email
                displayName
                roles
                isActive
                createdAt
                lastLogin
              }
            }
          }`,
          { organizationId: options.organizationId },
        )
        .pipe(
          map(({ isched }) => this.toConnection(isched.users, options)),
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
            isched {
              createUser(organizationId: $organizationId, input: $input) {
                id
                email
                displayName
                roles
                isActive
                createdAt
                lastLogin
              }
            }
          }`,
          { organizationId, input },
        )
        .pipe(map(({ isched }) => this.toUserRecord(isched.createUser, organizationId)));
    });
  }

  updateUser(
    organizationId: string,
    id: string,
    input: UpdateUserInput,
  ): Observable<UserRecord> {
    return defer(() => {
      const scopeCheck = this.orgContext.validateMutationScope(organizationId);
      if (!scopeCheck.ok) {
        return throwError(() => new GraphQLRequestError(scopeCheck.message, scopeCheck.code));
      }

      return this.gql
        .mutate<UpdateUserResponse>(
          `mutation UpdateUser($organizationId: ID!, $id: ID!, $input: UpdateUserInput!) {
            isched {
              updateUser(
                organizationId: $organizationId
                id: $id
                input: $input
              ) {
                id
                email
                displayName
                roles
                isActive
                createdAt
                lastLogin
              }
            }
          }`,
          { organizationId, id, input },
        )
        .pipe(map(({ isched }) => this.toUserRecord(isched.updateUser, organizationId)));
    });
  }

  private toConnection(records: GraphQLUserRecord[], options: UserListOptions): UserConnection {
    let users = records.map((record) => this.toUserRecord(record, options.organizationId));
    for (const filter of options.filter ?? []) {
      const field = filter.field === 'loginId' ? 'email' : filter.field;
      if (field !== 'email' && field !== 'displayName') continue;
      users = users.filter((user) => {
        const value = user[field].toLocaleLowerCase();
        const expected = filter.value.toLocaleLowerCase();
        return filter.op === 'eq' ? value === expected : value.includes(expected);
      });
    }

    const sort = options.sort ?? DEFAULT_SORT;
    if (sort.length > 0) {
      const { field, direction } = sort[0];
      if (field === 'email' || field === 'loginId' || field === 'displayName') {
        users = [...users].sort((left, right) => {
          const a = field === 'loginId' ? left.email : left[field];
          const b = field === 'loginId' ? right.email : right[field];
          return direction === 'DESC' ? b.localeCompare(a) : a.localeCompare(b);
        });
      }
    }

    const page = options.page ?? DEFAULT_PAGE;
    const totalElements = users.length;
    const totalPages = totalElements === 0 ? 0 : Math.ceil(totalElements / page.size);
    const offset = Math.max(0, page.number - 1) * page.size;
    return {
      nodes: users.slice(offset, offset + page.size),
      pageInfo: { ...page, totalElements, totalPages },
    };
  }

  private toUserRecord(record: GraphQLUserRecord, organizationId: string): UserRecord {
    return {
      ...record,
      organizationId,
      status: record.isActive ? 'ACTIVE' : 'DISABLED',
      roles: record.roles ?? [],
      lastLogin: record.lastLogin ?? null,
    };
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

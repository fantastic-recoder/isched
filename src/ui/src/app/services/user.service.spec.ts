import { TestBed } from '@angular/core/testing';
import { of, Subject, throwError } from 'rxjs';
import { OrgContextGuardResult, OrgContextService } from './org-context.service';
import { GraphQLRequestError, GraphQLService, GRAPHQL_ERROR_CODES } from './graphql.service';
import { ShellStatusService } from './shell-status.service';
import { UserService } from './user.service';

describe('UserService shell digest publication', () => {
  let service: UserService;
  let shellStatus: ShellStatusService;
  const gql = {
    query: jest.fn(),
    mutate: jest.fn(),
  };

  beforeEach(() => {
    gql.query.mockReset();
    gql.mutate.mockReset();

    TestBed.configureTestingModule({
      providers: [
        UserService,
        ShellStatusService,
        {
          provide: GraphQLService,
          useValue: gql,
        },
        {
          provide: OrgContextService,
          useValue: {
            validateMutationScope: jest.fn(() => ({ ok: true })),
          },
        },
      ],
    });

    service = TestBed.inject(UserService);
    shellStatus = TestBed.inject(ShellStatusService);
    shellStatus.reset();
  });

  it('publishes loading then success digests for organization user loads', () => {
    const response$ = new Subject<{
      isched: {
        users: [];
      };
    }>();
    gql.query.mockReturnValue(response$.asObservable());

    const nextSpy = jest.fn();
    service.listUsers({ organizationId: 'org-1' }).subscribe({ next: nextSpy });

    expect(shellStatus.operationDigest()).toMatchObject({
      state: 'loading',
      message: 'Loading organization users',
      operationKey: 'organization-users:list',
    });

    response$.next({
      isched: {
        users: [],
      },
    });
    response$.complete();

    expect(nextSpy).toHaveBeenCalledWith({
      nodes: [],
      pageInfo: { number: 1, size: 10, totalElements: 0, totalPages: 0 },
    });
    expect(shellStatus.operationDigest()).toMatchObject({
      state: 'success',
      message: 'Organization users loaded',
      operationKey: 'organization-users:list',
    });
  });

  describe('UserService GraphQL operations and organization scope', () => {
    const record = {
      id: 'user-1',
      email: 'jdoe@example.test',
      displayName: 'Jane Doe',
      roles: ['viewer'],
      isActive: true,
      createdAt: '2026-04-05T12:00:00Z',
      lastLogin: null,
    };
    const gql = { query: jest.fn(), mutate: jest.fn() };
    const validateMutationScope = jest.fn((): OrgContextGuardResult => ({ ok: true }));

    beforeEach(() => {
      TestBed.resetTestingModule();
      gql.query.mockReset();
      gql.mutate.mockReset();
      validateMutationScope.mockReset();
      validateMutationScope.mockReturnValue({ ok: true });

      TestBed.configureTestingModule({
        providers: [
          UserService,
          ShellStatusService,
          { provide: GraphQLService, useValue: gql },
          { provide: OrgContextService, useValue: { validateMutationScope } },
        ],
      });
    });

    it('lists and locally pages organization users using the supported GraphQL user shape', () => {
      gql.query.mockReturnValue(
        of({
          isched: {
            users: [record],
          },
        }),
      );
      const service = TestBed.inject(UserService);
      service.listUsers({
        organizationId: 'org-1',
        page: { number: 1, size: 5 },
        filter: [{ field: 'loginId', op: 'contains', value: 'doe' }],
      }).subscribe((result) => {
        expect(result.nodes).toEqual([{
          ...record,
          organizationId: 'org-1',
          status: 'ACTIVE',
        }]);
        expect(result.pageInfo).toEqual({
          number: 1,
          size: 5,
          totalElements: 1,
          totalPages: 1,
        });
      });

      const [document, variables] = gql.query.mock.calls[0] as [string, Record<string, unknown>];
      expect(document).toContain('users(organizationId: $organizationId)');
      expect(document).toContain('isActive');
      expect(variables).toEqual({ organizationId: 'org-1' });
    });

    it('uses GraphQL create and status update mutations', () => {
      gql.mutate
        .mockReturnValueOnce(of({ isched: { createUser: record } }))
        .mockReturnValueOnce(of({ isched: { updateUser: { ...record, displayName: 'Jane Updated', isActive: false } } }));
      const service = TestBed.inject(UserService);

      service.createUser('org-1', {
        email: 'jdoe@example.test',
        password: 'Valid!Password123',
        displayName: 'Jane Doe',
      }).subscribe((result) => {
        expect(result).toMatchObject({ email: 'jdoe@example.test', status: 'ACTIVE' });
      });
      service.updateUser('org-1', 'user-1', { isActive: false }).subscribe((result) => {
        expect(result.displayName).toBe('Jane Updated');
        expect(result.status).toBe('DISABLED');
      });

      expect(gql.mutate).toHaveBeenCalledTimes(2);
      expect(gql.mutate.mock.calls[0]?.[0]).toContain('createUser');
      expect(gql.mutate.mock.calls[0]?.[1]).toEqual({
        organizationId: 'org-1',
        input: {
          email: 'jdoe@example.test',
          password: 'Valid!Password123',
          displayName: 'Jane Doe',
        },
      });
      expect(gql.mutate.mock.calls[1]?.[0]).toContain('updateUser');
      expect(gql.mutate.mock.calls[1]?.[1]).toEqual({
        organizationId: 'org-1',
        id: 'user-1',
        input: { isActive: false },
      });
    });

    it('rejects create/update outside the selected organization before dispatching mutations', () => {
      validateMutationScope.mockReturnValue({
        ok: false,
        code: GRAPHQL_ERROR_CODES.CONTEXT_MISMATCH,
        message: 'Select the matching organization.',
      });
      const service = TestBed.inject(UserService);
      const errors: unknown[] = [];

      service.createUser('org-foreign', {
        email: 'jdoe@example.test',
        password: 'Valid!Password123',
        displayName: 'Jane Doe',
      }).subscribe({
        error: (error: unknown) => errors.push(error),
      });
      service.updateUser('org-foreign', 'user-1', { isActive: false }).subscribe({
        error: (error: unknown) => errors.push(error),
      });

      expect(errors).toHaveLength(2);
      expect(errors[0]).toBeInstanceOf(GraphQLRequestError);
      expect((errors[0] as GraphQLRequestError).code).toBe(GRAPHQL_ERROR_CODES.CONTEXT_MISMATCH);
      expect(gql.mutate).not.toHaveBeenCalled();
    });
  });

  it('publishes an understandable failure digest when organization user loading fails', () => {
    gql.query.mockReturnValue(
      throwError(() => new GraphQLRequestError('users failed', GRAPHQL_ERROR_CODES.TRANSIENT_NETWORK)),
    );

    service.listUsers({ organizationId: 'org-1' }).subscribe({ error: () => undefined });

    expect(shellStatus.operationDigest()).toMatchObject({
      state: 'error',
      message: 'Unable to load organization users',
      operationKey: 'organization-users:list',
    });
  });
});

import { TestBed } from '@angular/core/testing';
import { Subject, throwError } from 'rxjs';
import { OrgContextService } from './org-context.service';
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
      users: {
        nodes: [];
        pageInfo: { number: number; size: number; totalElements: number; totalPages: number };
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
      users: {
        nodes: [],
        pageInfo: { number: 1, size: 10, totalElements: 0, totalPages: 0 },
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


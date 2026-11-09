import { TestBed } from '@angular/core/testing';
import { of, Subject, tap, throwError } from 'rxjs';
import { OrganizationService } from '../../services/organization.service';
import { ShellStatusService } from '../../services/shell-status.service';
import { UserService } from '../../services/user.service';
import { UsersPage } from './users.page';
import { GraphQLRequestError, GRAPHQL_ERROR_CODES } from '../../services/graphql.service';

const flushMicrotasks = () => new Promise<void>((resolve) => setTimeout(resolve, 0));
const listOrganizations = jest.fn(() =>
  of({
    nodes: [{ id: 'org-a', name: 'Org A', status: 'ACTIVE', revision: 2, updatedAt: '2026-04-05T12:00:00Z' }],
    pageInfo: { number: 1, size: 25, totalElements: 1, totalPages: 1 },
  }),
);
const listUsers = jest.fn(() =>
  of({
    nodes: [],
    pageInfo: { number: 1, size: 10, totalElements: 0, totalPages: 0 },
  }),
);
const createUser = jest.fn((organizationId: string, input: { email: string; password: string; displayName: string }) =>
  of({
    id: 'user-1',
    organizationId,
    email: input.email,
    displayName: input.displayName,
    status: 'ACTIVE',
    roles: [],
    createdAt: '2026-04-05T12:05:00Z',
    lastLogin: null,
  }),
);
const updateUser = jest.fn(
  (
    organizationId: string,
    id: string,
    input: { displayName?: string; isActive?: boolean },
  ) =>
    of({
      id,
      organizationId,
      email: 'jdoe@example.test',
      displayName: input.displayName ?? 'Jane Doe',
      status: input.isActive === false ? 'DISABLED' : 'ACTIVE',
      roles: [],
      createdAt: '2026-04-05T12:00:00Z',
      lastLogin: null,
    }),
);

describe('UsersPage accessibility', () => {
  beforeEach(async () => {
    listOrganizations.mockClear();
    listUsers.mockClear();
    createUser.mockClear();
    updateUser.mockClear();

    await TestBed.configureTestingModule({
      imports: [UsersPage],
      providers: [
        ShellStatusService,
        {
          provide: OrganizationService,
          useValue: {
            listOrganizations,
          },
        },
        {
          provide: UserService,
          useValue: {
            listUsers,
            createUser,
            updateUser,
          },
        },
      ],
    }).compileComponents();
  });

  it('exposes accessible labels for user CRUD interactive controls', () => {
    const fixture = TestBed.createComponent(UsersPage);
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;

    const orgSelect = root.querySelector<HTMLSelectElement>('#user-org-select');
    const form = root.querySelector<HTMLFormElement>('#users-form');
    const email = root.querySelector<HTMLInputElement>('#users-email');
    const displayName = root.querySelector<HTMLInputElement>('#users-displayName');
    const saveButton = root.querySelector<HTMLButtonElement>('#users-save-submit');
    const deactivateButton = root.querySelector<HTMLButtonElement>('#users-deactivate');
    const reactivateButton = root.querySelector<HTMLButtonElement>('#users-reactivate');

    expect(root.querySelector('label[for="user-org-select"]')?.textContent).toContain('Organization');
    expect(orgSelect?.getAttribute('aria-label')).toBe('Select organization context');
    expect(form?.getAttribute('aria-label')).toBe('Organization user form');
    expect(root.querySelector('label[for="users-email"]')?.textContent).toContain('Email address');
    expect(root.querySelector('label[for="users-displayName"]')?.textContent).toContain('Display Name');
    expect(email?.getAttribute('aria-required')).toBe('true');
    expect(displayName?.getAttribute('aria-required')).toBe('true');
    expect(saveButton?.getAttribute('aria-label')).toBe('Save user changes');
    expect(deactivateButton?.getAttribute('aria-label')).toBe('Deactivate selected user');
    expect(reactivateButton?.getAttribute('aria-label')).toBe('Reactivate selected user');
    expect(saveButton?.className).toContain('focus-visible:outline');
  });

  it('saves and changes user status with focus-managed status region', async () => {
    const fixture = TestBed.createComponent(UsersPage);
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;

    const email = root.querySelector<HTMLInputElement>('#users-email');
    const password = root.querySelector<HTMLInputElement>('#users-password');
    const displayName = root.querySelector<HTMLInputElement>('#users-displayName');
    const form = root.querySelector<HTMLFormElement>('#users-form');

    expect(email).toBeTruthy();
    expect(password).toBeTruthy();
    expect(displayName).toBeTruthy();
    expect(form).toBeTruthy();

    email!.focus();
    email!.value = 'jdoe@example.test';
    email!.dispatchEvent(new Event('input', { bubbles: true }));
    password!.value = 'Valid!Password123';
    password!.dispatchEvent(new Event('input', { bubbles: true }));

    displayName!.value = 'Jane Doe';
    displayName!.dispatchEvent(new Event('input', { bubbles: true }));

    form!.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#users-status')?.textContent).toContain('User changes saved.');
    expect((document.activeElement as HTMLElement | null)?.id).toBe('users-status');
    expect(createUser).toHaveBeenCalledWith('org-a', {
      email: 'jdoe@example.test',
      password: 'Valid!Password123',
      displayName: 'Jane Doe',
    });

    const deactivateButton = root.querySelector<HTMLButtonElement>('#users-deactivate');
    expect(deactivateButton?.disabled).toBe(false);

    deactivateButton!.click();
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#users-status')?.textContent).toContain('Jane Doe is now disabled.');

    const reactivateButton = root.querySelector<HTMLButtonElement>('#users-reactivate');
    expect(reactivateButton?.disabled).toBe(false);

    reactivateButton!.click();
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#users-status')?.textContent).toContain('Jane Doe is now active.');
    expect(updateUser).toHaveBeenLastCalledWith('org-a', 'user-1', { isActive: true });
  });

  it('should hide loading spinner when organization selection results in empty user list', async () => {
    const fixture = TestBed.createComponent(UsersPage);
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    const root = fixture.nativeElement as HTMLElement;
    const orgSelect = root.querySelector<HTMLSelectElement>('#user-org-select');

    // Initially, no spinner should be visible
    expect(root.querySelector('.loading')).toBeFalsy();

    // Simulate selecting an organization
    orgSelect!.value = 'org-a';
    orgSelect!.dispatchEvent(new Event('change', { bubbles: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    // Spinner should be hidden after API returns (even with empty user list)
    expect(root.querySelector('.loading')).toBeFalsy();
    expect(root.querySelector('#users-list')?.textContent).toContain('No users match the current server-side query.');
  });

  it('publishes shell digest loading and success text during initial organization-user load', async () => {
    const shellStatus = TestBed.inject(ShellStatusService);
    const response$ = new Subject<{
      nodes: [];
      pageInfo: { number: number; size: number; totalElements: number; totalPages: number };
    }>();

    listUsers.mockImplementationOnce(() => {
      const sequence = shellStatus.beginOperation('organization-users:list', 'Loading organization users');
      return response$.asObservable().pipe(
        tap(() => {
          shellStatus.completeOperation(
            'organization-users:list',
            'Organization users loaded',
            sequence,
          );
        }),
      );
    });

    const fixture = TestBed.createComponent(UsersPage);
    fixture.detectChanges();

    expect(shellStatus.operationDigest().message).toBe('Loading organization users');

    response$.next({
      nodes: [],
      pageInfo: { number: 1, size: 10, totalElements: 0, totalPages: 0 },
    });
    response$.complete();

    await flushMicrotasks();
    fixture.detectChanges();

    expect(shellStatus.operationDigest().message).toBe('Organization users loaded');
  });

  it('updates a selected user and renders the final state returned by GraphQL', async () => {
    listUsers.mockReturnValueOnce(
      of({
        nodes: [{
          id: 'user-existing',
          organizationId: 'org-a',
          email: 'jdoe@example.test',
          displayName: 'Jane Doe',
          status: 'ACTIVE' as const,
          roles: [],
          createdAt: '2026-04-05T12:00:00Z',
          lastLogin: null,
        }],
        pageInfo: { number: 1, size: 10, totalElements: 1, totalPages: 1 },
      }),
    );
    const fixture = TestBed.createComponent(UsersPage);
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;

    root.querySelector<HTMLInputElement>('#users-displayName')!.value = 'Jane Updated';
    root.querySelector<HTMLInputElement>('#users-displayName')!.dispatchEvent(
      new Event('input', { bubbles: true }),
    );
    root.querySelector<HTMLFormElement>('#users-form')!.dispatchEvent(
      new Event('submit', { bubbles: true, cancelable: true }),
    );
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(updateUser).toHaveBeenCalledWith(
      'org-a',
      'user-existing',
      { displayName: 'Jane Updated' },
    );
    expect(root.querySelector('#users-list')?.textContent).toContain('Jane Updated');
    expect(root.querySelector('#users-status')?.textContent).toContain('User changes saved.');
  });

  it('retains user form values and reports conflict without presenting a success state', async () => {
    listUsers.mockReturnValueOnce(
      of({
        nodes: [{
          id: 'user-existing',
          organizationId: 'org-a',
          email: 'jdoe@example.test',
          displayName: 'Jane Doe',
          status: 'ACTIVE' as const,
          roles: [],
          createdAt: '2026-04-05T12:00:00Z',
          lastLogin: null,
        }],
        pageInfo: { number: 1, size: 10, totalElements: 1, totalPages: 1 },
      }),
    );
    updateUser.mockImplementationOnce(() =>
      throwError(
        () =>
          new GraphQLRequestError(
            'Stale user update',
            GRAPHQL_ERROR_CODES.CONFLICT,
          ),
      ),
    );
    const fixture = TestBed.createComponent(UsersPage);
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;
    const displayName = root.querySelector<HTMLInputElement>('#users-displayName')!;
    displayName.value = 'Jane Retry';
    displayName.dispatchEvent(new Event('input', { bubbles: true }));
    root.querySelector<HTMLFormElement>('#users-form')!.dispatchEvent(
      new Event('submit', { bubbles: true, cancelable: true }),
    );
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(displayName.value).toBe('Jane Retry');
    expect(root.querySelector('[role="alert"]')?.textContent).toContain(
      'Another administrator changed this user',
    );
    expect(root.querySelector('#users-status')).toBeNull();
  });
});

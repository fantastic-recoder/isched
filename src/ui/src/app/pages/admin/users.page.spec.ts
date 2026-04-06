import { TestBed } from '@angular/core/testing';
import { of, Subject, tap } from 'rxjs';
import { OrganizationService } from '../../services/organization.service';
import { ShellStatusService } from '../../services/shell-status.service';
import { UserService } from '../../services/user.service';
import { UsersPage } from './users.page';

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
const createUser = jest.fn((organizationId: string, input: { loginId: string; displayName: string }) =>
  of({
    id: 'user-1',
    organizationId,
    loginId: input.loginId,
    displayName: input.displayName,
    status: 'ACTIVE',
    revision: 1,
    updatedAt: '2026-04-05T12:05:00Z',
    roleAssignments: [],
  }),
);
const updateUser = jest.fn(
  (
    organizationId: string,
    id: string,
    input: { loginId?: string; displayName?: string; status?: string },
    expectedRevision: number,
  ) =>
    of({
      id,
      organizationId,
      loginId: input.loginId ?? 'jdoe',
      displayName: input.displayName ?? 'Jane Doe',
      status: input.status ?? 'ACTIVE',
      revision: expectedRevision + 1,
      updatedAt: '2026-04-05T12:06:00Z',
      roleAssignments: [],
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
    const loginId = root.querySelector<HTMLInputElement>('#users-loginId');
    const displayName = root.querySelector<HTMLInputElement>('#users-displayName');
    const saveButton = root.querySelector<HTMLButtonElement>('#users-save-submit');
    const deactivateButton = root.querySelector<HTMLButtonElement>('#users-deactivate');
    const reactivateButton = root.querySelector<HTMLButtonElement>('#users-reactivate');

    expect(root.querySelector('label[for="user-org-select"]')?.textContent).toContain('Organization');
    expect(orgSelect?.getAttribute('aria-label')).toBe('Select organization context');
    expect(form?.getAttribute('aria-label')).toBe('Organization user form');
    expect(root.querySelector('label[for="users-loginId"]')?.textContent).toContain('Login ID');
    expect(root.querySelector('label[for="users-displayName"]')?.textContent).toContain('Display Name');
    expect(loginId?.getAttribute('aria-required')).toBe('true');
    expect(displayName?.getAttribute('aria-required')).toBe('true');
    expect(saveButton?.getAttribute('aria-label')).toBe('Save user changes');
    expect(deactivateButton?.getAttribute('aria-label')).toBe('Deactivate selected user');
    expect(reactivateButton?.getAttribute('aria-label')).toBe('Reactivate selected user');
    expect(saveButton?.className).toContain('focus-visible:outline');
  });

  it('supports keyboard-only save/deactivate/reactivate flow with focus-managed status region', async () => {
    const fixture = TestBed.createComponent(UsersPage);
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;

    const loginId = root.querySelector<HTMLInputElement>('#users-loginId');
    const displayName = root.querySelector<HTMLInputElement>('#users-displayName');
    const form = root.querySelector<HTMLFormElement>('#users-form');

    expect(loginId).toBeTruthy();
    expect(displayName).toBeTruthy();
    expect(form).toBeTruthy();

    loginId!.focus();
    loginId!.value = 'jdoe';
    loginId!.dispatchEvent(new Event('input', { bubbles: true }));

    displayName!.value = 'Jane Doe';
    displayName!.dispatchEvent(new Event('input', { bubbles: true }));

    form!.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#users-status')?.textContent).toContain('User changes saved.');
    expect((document.activeElement as HTMLElement | null)?.id).toBe('users-status');
    expect(createUser).toHaveBeenCalledWith('org-a', { loginId: 'jdoe', displayName: 'Jane Doe' });

    const deactivateButton = root.querySelector<HTMLButtonElement>('#users-deactivate');
    expect(deactivateButton?.disabled).toBe(false);

    deactivateButton!.dispatchEvent(new KeyboardEvent('keydown', { key: ' ', bubbles: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#users-status')?.textContent).toContain('Jane Doe is now disabled.');

    const reactivateButton = root.querySelector<HTMLButtonElement>('#users-reactivate');
    expect(reactivateButton?.disabled).toBe(false);

    reactivateButton!.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#users-status')?.textContent).toContain('Jane Doe is now active.');
    expect(updateUser).toHaveBeenLastCalledWith('org-a', 'user-1', { status: 'ACTIVE' }, 2);
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
});


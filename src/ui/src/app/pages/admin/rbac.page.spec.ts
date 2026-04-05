import { ComponentFixture, TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { RbacPage } from './rbac.page';

const flushMicrotasks = () => new Promise<void>((resolve) => setTimeout(resolve, 0));

const defaultRoles = [
  { id: 'role_user', name: 'User', scope: 'tenant' },
  { id: 'role_admin', name: 'Admin', scope: 'platform' },
];

describe('RbacPage', () => {
  let httpMock: HttpTestingController;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [RbacPage],
      providers: [provideHttpClient(), provideHttpClientTesting()],
    }).compileComponents();

    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  function createComponentWithRoles(roles = defaultRoles): ComponentFixture<RbacPage> {
    const fixture = TestBed.createComponent(RbacPage);
    fixture.detectChanges();

    httpMock.expectOne('/graphql').flush({
      data: { roles },
    });

    fixture.detectChanges();
    return fixture;
  }

  it('loads roles on init', () => {
    const fixture = createComponentWithRoles([{ id: 'role_user', name: 'User', scope: 'tenant' }]);

    expect(fixture.componentInstance.roles().length).toBe(1);
  });

  it('exposes screen-reader labels for role create/edit/assign controls', () => {
    const fixture = createComponentWithRoles();
    const root = fixture.nativeElement as HTMLElement;

    const roleForm = root.querySelector<HTMLFormElement>('#rbac-role-form');
    const roleIdInput = root.querySelector<HTMLInputElement>('#rbac-role-id-input');
    const roleNameInput = root.querySelector<HTMLInputElement>('#rbac-role-name-input');
    const scopeSelect = root.querySelector<HTMLSelectElement>('#rbac-role-scope-select');
    const roleSubmit = root.querySelector<HTMLButtonElement>('#rbac-role-submit');
    const assignForm = root.querySelector<HTMLFormElement>('#rbac-assign-form');
    const assignUserInput = root.querySelector<HTMLInputElement>('#rbac-assign-user-input');
    const assignRoleSelect = root.querySelector<HTMLSelectElement>('#rbac-assign-role-select');
    const assignSubmit = root.querySelector<HTMLButtonElement>('#rbac-assign-submit');
    const editButton = root.querySelector<HTMLButtonElement>('#rbac-edit-role_user');
    const rowAssignButton = root.querySelector<HTMLButtonElement>('#rbac-select-assign-role_user');

    expect(roleForm?.getAttribute('aria-label')).toBe('Create or edit custom role form');
    expect(root.querySelector('label[for="rbac-role-id-input"]')?.textContent).toContain('Role ID');
    expect(root.querySelector('label[for="rbac-role-name-input"]')?.textContent).toContain('Role Name');
    expect(roleIdInput?.getAttribute('aria-required')).toBe('true');
    expect(roleNameInput?.getAttribute('aria-required')).toBe('true');
    expect(scopeSelect?.getAttribute('aria-label')).toBe('Select role scope');
    expect(roleSubmit?.getAttribute('aria-label')).toBe('Create custom role');
    expect(assignForm?.getAttribute('aria-label')).toBe('Assign role form');
    expect(assignUserInput?.getAttribute('aria-required')).toBe('true');
    expect(assignRoleSelect?.getAttribute('aria-label')).toBe('Choose role to assign');
    expect(assignSubmit?.getAttribute('aria-label')).toBe('Assign selected role');
    expect(editButton?.getAttribute('aria-label')).toBe('Edit role User');
    expect(rowAssignButton?.getAttribute('aria-label')).toBe('Assign role User');
    expect(roleSubmit?.className).toContain('focus-visible:outline');
  });

  it('supports keyboard-only create/edit/assign path with focus-managed status feedback', async () => {
    const fixture = createComponentWithRoles([{ id: 'role_user', name: 'User', scope: 'tenant' }]);
    const root = fixture.nativeElement as HTMLElement;

    const roleIdInput = root.querySelector<HTMLInputElement>('#rbac-role-id-input');
    const roleNameInput = root.querySelector<HTMLInputElement>('#rbac-role-name-input');
    const roleForm = root.querySelector<HTMLFormElement>('#rbac-role-form');

    expect(roleIdInput).toBeTruthy();
    expect(roleNameInput).toBeTruthy();
    expect(roleForm).toBeTruthy();

    roleIdInput!.focus();
    roleIdInput!.value = 'role_ops';
    roleIdInput!.dispatchEvent(new Event('input', { bubbles: true }));
    roleNameInput!.value = 'Operations';
    roleNameInput!.dispatchEvent(new Event('input', { bubbles: true }));
    roleForm!.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#rbac-status')?.textContent).toContain('Created role Operations.');
    expect((document.activeElement as HTMLElement | null)?.id).toBe('rbac-status');

    const editRoleButton = root.querySelector<HTMLButtonElement>('#rbac-edit-role_ops');
    expect(editRoleButton).toBeTruthy();
    editRoleButton!.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    const editedRoleNameInput = root.querySelector<HTMLInputElement>('#rbac-role-name-input');
    expect(editedRoleNameInput?.value).toBe('Operations');
    editedRoleNameInput!.value = 'Operations Team';
    editedRoleNameInput!.dispatchEvent(new Event('input', { bubbles: true }));
    roleForm!.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#rbac-status')?.textContent).toContain('Updated role Operations Team.');

    const selectAssignButton = root.querySelector<HTMLButtonElement>('#rbac-select-assign-role_ops');
    expect(selectAssignButton).toBeTruthy();
    selectAssignButton!.dispatchEvent(new KeyboardEvent('keydown', { key: ' ', bubbles: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect((document.activeElement as HTMLElement | null)?.id).toBe('rbac-assign-user-input');

    const assignUserInput = root.querySelector<HTMLInputElement>('#rbac-assign-user-input');
    const assignForm = root.querySelector<HTMLFormElement>('#rbac-assign-form');
    assignUserInput!.value = 'user-100';
    assignUserInput!.dispatchEvent(new Event('input', { bubbles: true }));
    assignForm!.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#rbac-status')?.textContent).toContain('Assigned role Operations Team to user-100.');
    expect((document.activeElement as HTMLElement | null)?.id).toBe('rbac-status');
  });
});


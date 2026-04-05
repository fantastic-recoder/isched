import { TestBed } from '@angular/core/testing';
import { UsersPage } from './users.page';

const flushMicrotasks = () => new Promise<void>((resolve) => setTimeout(resolve, 0));

describe('UsersPage accessibility', () => {
  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [UsersPage],
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

    const deactivateButton = root.querySelector<HTMLButtonElement>('#users-deactivate');
    expect(deactivateButton?.disabled).toBe(false);

    deactivateButton!.dispatchEvent(new KeyboardEvent('keydown', { key: ' ', bubbles: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#users-status')?.textContent).toContain('User marked inactive.');

    const reactivateButton = root.querySelector<HTMLButtonElement>('#users-reactivate');
    expect(reactivateButton?.disabled).toBe(false);

    reactivateButton!.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#users-status')?.textContent).toContain('User reactivated.');
  });
});


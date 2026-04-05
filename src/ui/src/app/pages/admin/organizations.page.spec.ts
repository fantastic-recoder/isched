import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { OrganizationsPage } from './organizations.page';
import { OrganizationService } from '../../services/organization.service';

const flushMicrotasks = () => new Promise<void>((resolve) => setTimeout(resolve, 0));
const listOrganizations = jest.fn(() =>
  of({
    nodes: [{ id: 'org-a', name: 'Org A', status: 'ACTIVE', revision: 2, updatedAt: '2026-04-05T12:00:00Z' }],
    pageInfo: { number: 1, size: 10, totalElements: 1, totalPages: 1 },
  }),
);
const createOrganization = jest.fn((input: { name: string }) =>
  of({
    id: 'local-org-1',
    name: input.name,
    status: 'ACTIVE',
    revision: 1,
    updatedAt: '2026-04-05T12:05:00Z',
  }),
);
const updateOrganization = jest.fn((id: string, input: { status?: string; name?: string }, expectedRevision: number) =>
  of({
    id,
    name: input.name ?? 'Org Keyboard',
    status: input.status ?? 'ACTIVE',
    revision: expectedRevision + 1,
    updatedAt: '2026-04-05T12:06:00Z',
  }),
);

describe('OrganizationsPage accessibility', () => {
  beforeEach(async () => {
    listOrganizations.mockClear();
    createOrganization.mockClear();
    updateOrganization.mockClear();

    await TestBed.configureTestingModule({
      imports: [OrganizationsPage],
      providers: [
        {
          provide: OrganizationService,
          useValue: {
            listOrganizations,
            createOrganization,
            updateOrganization,
          },
        },
      ],
    }).compileComponents();
  });

  it('exposes accessible labels for organization CRUD controls', () => {
    const fixture = TestBed.createComponent(OrganizationsPage);
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;

    const createForm = root.querySelector<HTMLFormElement>('#org-create-form');
    const nameInput = root.querySelector<HTMLInputElement>('#org-name-input');
    const createButton = root.querySelector<HTMLButtonElement>('#org-create-submit');
    const editButton = root.querySelector<HTMLButtonElement>('#org-edit-org-a');
    const toggleButton = root.querySelector<HTMLButtonElement>('#org-toggle-org-a');

    expect(createForm?.getAttribute('aria-label')).toBe('Create organization form');
    expect(root.querySelector('label[for="org-name-input"]')?.textContent).toContain('Organization Name');
    expect(nameInput?.getAttribute('aria-required')).toBe('true');
    expect(nameInput?.getAttribute('aria-describedby')).toContain('org-name-help');
    expect(createButton?.getAttribute('aria-label')).toBe('Create organization');
    expect(editButton?.getAttribute('aria-label')).toBe('Edit organization Org A');
    expect(toggleButton?.getAttribute('aria-label')).toBe('Deactivate organization Org A');
    expect(createButton?.className).toContain('focus-visible:outline');
  });

  it('supports keyboard-only create and toggle path with focus-managed status feedback', async () => {
    const fixture = TestBed.createComponent(OrganizationsPage);
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;

    const nameInput = root.querySelector<HTMLInputElement>('#org-name-input');
    const form = root.querySelector<HTMLFormElement>('#org-create-form');

    expect(nameInput).toBeTruthy();
    expect(form).toBeTruthy();

    nameInput!.focus();
    nameInput!.value = 'Org Keyboard';
    nameInput!.dispatchEvent(new Event('input', { bubbles: true }));
    form!.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.textContent).toContain('Org Keyboard');
    expect(root.querySelector('#organizations-status')?.textContent).toContain('Created organization Org Keyboard.');
    expect(createOrganization).toHaveBeenCalledWith({ name: 'Org Keyboard' });

    const toggleButton = root.querySelector<HTMLButtonElement>('#org-toggle-local-org-1');
    expect(toggleButton).toBeTruthy();

    toggleButton!.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
    fixture.detectChanges();
    await flushMicrotasks();
    fixture.detectChanges();

    expect(root.querySelector('#organizations-status')?.textContent).toContain('Org Keyboard is now suspended.');
    expect((document.activeElement as HTMLElement | null)?.id).toBe('organizations-status');
    expect(updateOrganization).toHaveBeenCalledWith('local-org-1', { status: 'SUSPENDED' }, 1);
  });
});


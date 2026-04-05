import { TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { OrganizationService } from '../../services/organization.service';
import { GraphQLRequestError, GRAPHQL_ERROR_CODES } from '../../services/graphql.service';
import { UserService } from '../../services/user.service';
import { OrganizationsPage } from './organizations.page';
import { UsersPage } from './users.page';

const baseOrganizations = {
  nodes: [{ id: 'org-a', name: 'Org A', status: 'ACTIVE', revision: 3, updatedAt: '2026-04-05T12:00:00Z' }],
  pageInfo: { number: 1, size: 10, totalElements: 1, totalPages: 1 },
};

describe('Organization and user admin deterministic errors', () => {
  it('surfaces a deterministic conflict message for organization saves', async () => {
    await TestBed.configureTestingModule({
      imports: [OrganizationsPage],
      providers: [
        {
          provide: OrganizationService,
          useValue: {
            listOrganizations: jest.fn(() => of(baseOrganizations)),
            createOrganization: jest.fn(() =>
              throwError(
                () =>
                  new GraphQLRequestError(
                    'stale organization write',
                    GRAPHQL_ERROR_CODES.CONFLICT,
                  ),
              ),
            ),
            updateOrganization: jest.fn(),
          },
        },
      ],
    }).compileComponents();

    const fixture = TestBed.createComponent(OrganizationsPage);
    fixture.detectChanges();

    const component = fixture.componentInstance;
    component.createForm.controls.name.setValue('Org A Prime');
    component.submitOrganization();
    fixture.detectChanges();

    expect(component.error()).toBe('Another administrator changed this organization. Refresh the list and retry your edit.');
  });

  it('maps user validation failures to field and global feedback', async () => {
    await TestBed.configureTestingModule({
      imports: [UsersPage],
      providers: [
        {
          provide: OrganizationService,
          useValue: {
            listOrganizations: jest.fn(() => of(baseOrganizations)),
          },
        },
        {
          provide: UserService,
          useValue: {
            listUsers: jest.fn(() =>
              of({
                nodes: [],
                pageInfo: { number: 1, size: 10, totalElements: 0, totalPages: 0 },
              }),
            ),
            createUser: jest.fn(() =>
              throwError(
                () =>
                  new GraphQLRequestError('validation failed', GRAPHQL_ERROR_CODES.VALIDATION_FAILED, {
                    loginId: ['Login ID is already in use within this organization.'],
                  }),
              ),
            ),
            updateUser: jest.fn(),
          },
        },
      ],
    }).compileComponents();

    const fixture = TestBed.createComponent(UsersPage);
    fixture.detectChanges();

    const component = fixture.componentInstance;
    component.form.controls.loginId.setValue('existing-user');
    component.form.controls.displayName.setValue('Existing User');
    component.saveUser();
    fixture.detectChanges();

    expect(component.error()).toBe('Please correct the highlighted fields and try again.');
    expect(component.form.controls.loginId.errors?.['server']).toBe('Login ID is already in use within this organization.');
  });
});

describe('UsersPage org context guard', () => {
  it('blocks organization switch while dirty form guard is active', async () => {
    await TestBed.configureTestingModule({
      imports: [UsersPage],
      providers: [
        {
          provide: OrganizationService,
          useValue: {
            listOrganizations: jest.fn(() => of(baseOrganizations)),
          },
        },
        {
          provide: UserService,
          useValue: {
            listUsers: jest.fn(() =>
              of({
                nodes: [],
                pageInfo: { number: 1, size: 10, totalElements: 0, totalPages: 0 },
              }),
            ),
            createUser: jest.fn(),
            updateUser: jest.fn(),
          },
        },
      ],
    }).compileComponents();

    const fixture = TestBed.createComponent(UsersPage);
    const component = fixture.componentInstance;

    component.markDirty();
    component.onOrgChange('org-b');

    expect(component.warning()).toContain('unsaved changes');
  });
});


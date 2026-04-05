import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { of } from 'rxjs';
import { DashboardComponent } from './dashboard';
import { GraphQLService } from '../../services/graphql.service';
import { AuthService } from '../../services/auth.service';

describe('DashboardComponent minimum content', () => {
  const gqlMock = {
    query: jest.fn(),
    mutate: jest.fn(),
  };

  const authMock = {
    signOut: jest.fn(() => of({ success: true })),
  };

  beforeEach(async () => {
    gqlMock.query.mockImplementation((query: string) => {
      if (query.includes('health')) {
        return of({ health: { status: 'UP' }, version: 'test-version' });
      }
      if (query.includes('organizations')) {
        return of({ organizations: [] });
      }
      if (query.includes('users')) {
        return of({ users: [] });
      }
      return of({});
    });

    await TestBed.configureTestingModule({
      imports: [DashboardComponent],
      providers: [
        provideRouter([
          { path: 'admin/organizations', component: DashboardComponent },
          { path: 'admin/users', component: DashboardComponent },
          { path: 'admin/rbac', component: DashboardComponent },
          { path: 'login', component: DashboardComponent },
        ]),
        { provide: GraphQLService, useValue: gqlMock },
        { provide: AuthService, useValue: authMock },
      ],
    }).compileComponents();
  });

  afterEach(() => {
    jest.clearAllMocks();
  });

  it('renders FR-020 minimum dashboard content with a health summary badge', () => {
    const fixture = TestBed.createComponent(DashboardComponent);
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    const healthCard = host.querySelector<HTMLElement>('[data-testid="dashboard-health-card"]');
    const healthBadge = host.querySelector<HTMLElement>('[data-testid="dashboard-health-badge"]');

    expect(healthCard).not.toBeNull();
    expect(healthBadge).not.toBeNull();
    expect(healthBadge?.textContent).toContain('Healthy');
  });

  it('renders Organizations/Users/RBAC quicklinks with expected route targets', () => {
    const fixture = TestBed.createComponent(DashboardComponent);
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    const organizationsLink = host.querySelector<HTMLAnchorElement>('[data-testid="quicklink-organizations"]');
    const usersLink = host.querySelector<HTMLAnchorElement>('[data-testid="quicklink-users"]');
    const rbacLink = host.querySelector<HTMLAnchorElement>('[data-testid="quicklink-rbac"]');

    expect(organizationsLink).not.toBeNull();
    expect(usersLink).not.toBeNull();
    expect(rbacLink).not.toBeNull();

    expect(organizationsLink?.getAttribute('href')).toContain('/admin/organizations');
    expect(usersLink?.getAttribute('href')).toContain('/admin/users');
    expect(rbacLink?.getAttribute('href')).toContain('/admin/rbac');
  });
});


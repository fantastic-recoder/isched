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
    localStorage.clear();

    gqlMock.query.mockImplementation((query: string) => {
      if (query.includes('health')) {
        return of({ isched: { health: { status: 'UP' }, version: 'test-version' } });
      }
      if (query.includes('organizations')) {
        return of({ isched: { organizations: { nodes: [] } } });
      }
      if (query.includes('plugins')) {
        return of({
          isched: {
            plugins: [
              {
                id: 'plugin-1',
                kind: 'REQUEST',
                name: 'AuditPlugin',
                version: '1.0.0',
                description: 'Captures request metadata',
                libraryPath: '/tmp/libaudit_plugin.so',
                isLoaded: true,
                loadedAt: '2026-01-01T00:00:00Z',
                unloadedAt: '',
              },
            ],
          },
        });
      }
      if (query.includes('users')) {
        return of({ isched: { users: [] } });
      }
      return of({});
    });

    gqlMock.mutate.mockImplementation((mutation: string) => {
      if (mutation.includes('loadPlugin')) {
        return of({ isched: { loadPlugin: { success: true, pluginId: 'resolver-plugin', errors: [] } } });
      }
      if (mutation.includes('unloadPlugin')) {
        return of({ isched: { unloadPlugin: { success: true, pluginId: 'resolver-plugin', errors: [] } } });
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
    localStorage.clear();
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

  it('renders the plugins administration panel with active plugin information', () => {
    const fixture = TestBed.createComponent(DashboardComponent);
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    expect(host.textContent).toContain('Plugins');
    expect(host.textContent).toContain('AuditPlugin');
    expect(host.textContent).toContain('Unload Selected');
  });

  it('restores persisted dashboard accordion state from localStorage', () => {
    localStorage.setItem(
      'isched.dashboard.panelState',
      JSON.stringify({ overviewHealth: false, overviewAdmin: true, organizations: false, plugins: true }),
    );

    const fixture = TestBed.createComponent(DashboardComponent);
    fixture.detectChanges();

    const host = fixture.nativeElement as HTMLElement;
    const healthToggle = host.querySelector<HTMLInputElement>('[data-testid="dashboard-panel-health-toggle"]');
    const adminToggle = host.querySelector<HTMLInputElement>('[data-testid="dashboard-panel-admin-toggle"]');
    const orgToggle = host.querySelector<HTMLInputElement>('[data-testid="dashboard-panel-organizations-toggle"]');
    const pluginsToggle = host.querySelector<HTMLInputElement>('[data-testid="dashboard-panel-plugins-toggle"]');

    expect(healthToggle?.checked).toBe(false);
    expect(adminToggle?.checked).toBe(true);
    expect(orgToggle?.checked).toBe(false);
    expect(pluginsToggle?.checked).toBe(true);
  });

  it('submits loadPlugin kind using backend-compatible lowercase values', () => {
    const fixture = TestBed.createComponent(DashboardComponent);
    fixture.detectChanges();

    const component = fixture.componentInstance;
    component.pluginForm.setValue({
      libraryPath: '/opt/isched/plugins/libresolver_plugin.so',
      kind: 'RESOLVER',
      pluginId: 'resolver-plugin',
    });

    component.loadPlugin();

    expect(gqlMock.mutate).toHaveBeenCalled();
    const [, variables] = gqlMock.mutate.mock.calls.at(-1) as [string, { input: { kind: string } }];
    expect(variables.input.kind).toBe('resolver');
  });
});


import { Component } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { of } from 'rxjs';
import { AuthService } from '../../services/auth.service';
import { ShellStatusService } from '../../services/shell-status.service';
import { AuthenticatedShellComponent } from './authenticated-shell.component';

@Component({ standalone: true, template: '<p>Dashboard</p>' })
class DashboardRouteStubComponent {}

@Component({ standalone: true, template: '<p>Organizations</p>' })
class OrganizationsRouteStubComponent {}

@Component({ standalone: true, template: '<p>Users</p>' })
class UsersRouteStubComponent {}

@Component({ standalone: true, template: '<p>RBAC</p>' })
class RbacRouteStubComponent {}

@Component({ standalone: true, template: '<p>Login</p>' })
class LoginRouteStubComponent {}

@Component({ standalone: true, template: '<p>Playground</p>' })
class PlaygroundRouteStubComponent {}

describe('AuthenticatedShellComponent', () => {
  const auth = {
    signOut: jest.fn(() => of(true)),
  };

  beforeEach(async () => {
    auth.signOut.mockClear();

    await TestBed.configureTestingModule({
      imports: [AuthenticatedShellComponent],
      providers: [
        ShellStatusService,
        provideRouter([
          { path: 'dashboard', component: DashboardRouteStubComponent },
          { path: 'admin/organizations', component: OrganizationsRouteStubComponent },
          { path: 'admin/users', component: UsersRouteStubComponent },
          { path: 'admin/rbac', component: RbacRouteStubComponent },
          { path: 'playground', component: PlaygroundRouteStubComponent },
          { path: 'login', component: LoginRouteStubComponent },
        ]),
        {
          provide: AuthService,
          useValue: auth,
        },
      ],
    }).compileComponents();
  });

  async function createComponent(initialUrl = '/dashboard') {
    const router = TestBed.inject(Router);
    const shellStatus = TestBed.inject(ShellStatusService);
    shellStatus.reset();
    shellStatus.setIdentity({ userId: 'user-1', displayName: 'Jane Admin' });
    await router.navigateByUrl(initialUrl);

    const fixture = TestBed.createComponent(AuthenticatedShellComponent);
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    return { fixture, router, shellStatus };
  }

  it('renders the logo, primary navigation, latest digest, and current user label', async () => {
    const { fixture } = await createComponent('/dashboard');
    const host = fixture.nativeElement as HTMLElement;

    expect(host.querySelector('[data-testid="shell-logo"]')?.getAttribute('src')).toContain(
      'assets/isched_logo.jpg',
    );
    expect(host.querySelector('[data-testid="shell-nav-dashboard"]')?.textContent).toContain('Dashboard');
    expect(host.querySelector('[data-testid="shell-nav-organizations"]')?.textContent).toContain(
      'Organizations',
    );
    expect(host.querySelector('[data-testid="shell-nav-users"]')?.textContent).toContain('Users');
    expect(host.querySelector('[data-testid="shell-nav-rbac"]')?.textContent).toContain('RBAC');
    expect(host.querySelector('[data-testid="shell-nav-playground"]')?.textContent).toContain('Playground');
    expect(host.querySelector('[data-testid="shell-status-digest"]')?.textContent).toContain('Ready');
    expect(host.querySelector('[data-testid="shell-current-user"]')?.textContent).toContain('Jane Admin');
  });

  it('marks the active route and updates navigation state after route changes', async () => {
    const { fixture, router } = await createComponent('/admin/users');
    const host = fixture.nativeElement as HTMLElement;

    expect(host.querySelector('[data-testid="shell-nav-users"]')?.getAttribute('aria-current')).toBe('page');
    expect(host.querySelector('[data-testid="shell-nav-dashboard"]')?.getAttribute('aria-current')).toBeNull();

    await router.navigateByUrl('/admin/organizations');
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    expect(
      host.querySelector('[data-testid="shell-nav-organizations"]')?.getAttribute('aria-current'),
    ).toBe('page');
    expect(host.querySelector('[data-testid="shell-nav-users"]')?.getAttribute('aria-current')).toBeNull();
  });

  it('signs out through AuthService and routes back to login', async () => {
    const { fixture, router } = await createComponent('/dashboard');
    const navigateSpy = jest.spyOn(router, 'navigateByUrl').mockResolvedValue(true);

    const signOutButton = (fixture.nativeElement as HTMLElement).querySelector<HTMLButtonElement>(
      '[data-testid="shell-sign-out"]',
    );
    signOutButton?.click();
    fixture.detectChanges();
    await fixture.whenStable();

    expect(auth.signOut).toHaveBeenCalledTimes(1);
    expect(navigateSpy).toHaveBeenCalledWith('/login');
  });
});


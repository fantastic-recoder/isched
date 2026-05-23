import { Routes } from '@angular/router';
import { authGuard } from './guards/auth.guard';
import { inject } from '@angular/core';
import { catchError, map, of } from 'rxjs';
import { BootstrapService } from './services/bootstrap.service';
import { Router } from '@angular/router';
import { SessionBootstrapStateService } from './services/session-bootstrap-state.service';
import { AuthService } from './services/auth.service';

const resolveBootstrapAvailability = (source: 'StartupProbe' | 'GuardProbe' | 'ActionProbe') => {
  const bootstrapService = inject(BootstrapService);

  return bootstrapService.bootstrapStatus(source).pipe(
    map(({ systemState }) => systemState.seedModeActive),
    catchError(() => {
      return of(false);
    }),
  );
};

const hasResolvedStartupState = () => {
  const sessionBootstrapState = inject(SessionBootstrapStateService);
  return sessionBootstrapState.sessionBootstrapState().initialRouteResolved;
};

const bootstrapGate = () => {
  const router = inject(Router);
  const sessionBootstrapState = inject(SessionBootstrapStateService);

  if (hasResolvedStartupState()) {
    return of(
      sessionBootstrapState.sessionBootstrapState().seedModeActive
        ? true
        : router.createUrlTree(['/login']),
    );
  }

  return resolveBootstrapAvailability('GuardProbe').pipe(
    map((seedModeActive) => {
      sessionBootstrapState.markInitialRouteResolved();
      return seedModeActive ? true : router.createUrlTree(['/login']);
    }),
    catchError(() => of(router.createUrlTree(['/login']))),
  );
};

/**
 * Prevents navigating to /login while seed mode is still active.
 * If no platform admin exists yet the user must complete bootstrap first.
 */
const loginGate = () => {
  const router = inject(Router);
  const auth = inject(AuthService);
  const sessionBootstrapState = inject(SessionBootstrapStateService);
  const startupState = sessionBootstrapState.sessionBootstrapState();

  if (startupState.initialRouteResolved) {
    if (startupState.seedModeActive) {
      return of(router.createUrlTree(['/bootstrap']));
    }

    return of(startupState.sessionAuthenticated ? router.createUrlTree(['/dashboard']) : true);
  }

  return resolveBootstrapAvailability('GuardProbe').pipe(
    map((seedModeActive) => {
      if (seedModeActive) {
        sessionBootstrapState.markInitialRouteResolved();
        return router.createUrlTree(['/bootstrap']);
      }

      const isAuthenticated = auth.isLoggedIn();
      sessionBootstrapState.markSessionKnown(isAuthenticated);
      sessionBootstrapState.markInitialRouteResolved();
      return isAuthenticated ? router.createUrlTree(['/dashboard']) : true;
    }),
    catchError(() => {
      sessionBootstrapState.markSessionKnown(false);
      sessionBootstrapState.markInitialRouteResolved();
      return of(true);
    }),
  );
};

export const routes: Routes = [
  { path: '', redirectTo: 'login', pathMatch: 'full' },
  {
    path: 'bootstrap',
    canMatch: [bootstrapGate],
    loadComponent: () =>
      import('./pages/bootstrap/bootstrap.page').then((m) => m.BootstrapPage),
  },
  {
    path: 'seed',
    loadComponent: () =>
      import('./pages/seed/seed').then((m) => m.SeedComponent),
  },
  {
    path: 'login',
    canActivate: [loginGate],
    loadComponent: () =>
      import('./pages/login/login').then((m) => m.LoginComponent),
  },
  {
    path: 'dashboard',
    canActivate: [authGuard],
    loadComponent: () =>
      import('./pages/dashboard/dashboard').then((m) => m.DashboardComponent),
  },
  {
    path: 'admin/organizations',
    canActivate: [authGuard],
    loadComponent: () =>
      import('./pages/admin/organizations.page').then((m) => m.OrganizationsPage),
  },
  {
    path: 'admin/users',
    canActivate: [authGuard],
    loadComponent: () =>
      import('./pages/admin/users.page').then((m) => m.UsersPage),
  },
  {
    path: 'admin/rbac',
    canActivate: [authGuard],
    loadComponent: () =>
      import('./pages/admin/rbac.page').then((m) => m.RbacPage),
  },
  {
    path: 'playground',
    canActivate: [authGuard],
    loadComponent: () =>
      import('./pages/playground/playground').then((m) => m.PlaygroundPage),
  },
  {
    path: 'dev/proxy',
    loadComponent: () =>
      import('./pages/dev/dev-proxy-health.page').then((m) => m.DevProxyHealthPage),
  },
  { path: '**', redirectTo: 'login' },
];

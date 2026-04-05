import { Routes } from '@angular/router';
import { authGuard } from './guards/auth.guard';
import { inject } from '@angular/core';
import { catchError, map, of } from 'rxjs';
import { BootstrapService } from './services/bootstrap.service';
import { Router } from '@angular/router';

const bootstrapGate = () => {
  const bootstrapService = inject(BootstrapService);
  const router = inject(Router);
  return bootstrapService.bootstrapStatus().pipe(
    map(({ systemState }) => {
      return systemState.seedModeActive ? true : router.createUrlTree(['/login']);
    }),
    catchError(() => of(router.createUrlTree(['/login']))),
  );
};

/**
 * Prevents navigating to /login while seed mode is still active.
 * If no platform admin exists yet the user must complete bootstrap first.
 */
const loginGate = () => {
  const bootstrapService = inject(BootstrapService);
  const router = inject(Router);
  return bootstrapService.bootstrapStatus().pipe(
    map(({ systemState }) => {
      return systemState.seedModeActive ? router.createUrlTree(['/bootstrap']) : true;
    }),
    catchError(() => of(true)),
  );
};

export const routes: Routes = [
  { path: '', redirectTo: 'bootstrap', pathMatch: 'full' },
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
    path: 'dev/proxy',
    loadComponent: () =>
      import('./pages/dev/dev-proxy-health.page').then((m) => m.DevProxyHealthPage),
  },
  { path: '**', redirectTo: 'login' },
];

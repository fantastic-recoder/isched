import { inject } from '@angular/core';
import { CanActivateFn, Router } from '@angular/router';
import { catchError, map, of, switchMap } from 'rxjs';
import { AuthService } from '../services/auth.service';
import { BootstrapService } from '../services/bootstrap.service';
import { SessionBootstrapStateService } from '../services/session-bootstrap-state.service';

export const authGuard: CanActivateFn = (_route, _state) => {
  const bootstrapService = inject(BootstrapService);
  const auth = inject(AuthService);
  const router = inject(Router);
  const sessionBootstrapState = inject(SessionBootstrapStateService);

  return bootstrapService.bootstrapStatus('GuardProbe').pipe(
    switchMap(({ systemState }) => {
      if (systemState.seedModeActive) {
        sessionBootstrapState.markInitialRouteResolved();
        return of(router.createUrlTree(['/bootstrap']));
      }

      const state = sessionBootstrapState.sessionBootstrapState();
      if (!state.firstGuardRevalidationComplete) {
        return auth.bootstrapSession().pipe(
          map((isAuthenticated) => {
            sessionBootstrapState.markSessionKnown(isAuthenticated);
            sessionBootstrapState.markFirstGuardRevalidationComplete();
            sessionBootstrapState.markInitialRouteResolved();
            return isAuthenticated ? true : router.createUrlTree(['/login']);
          }),
          catchError(() => {
            sessionBootstrapState.markSessionKnown(false);
            sessionBootstrapState.markFirstGuardRevalidationComplete();
            sessionBootstrapState.markInitialRouteResolved();
            return of(router.createUrlTree(['/login']));
          }),
        );
      }

      const isAuthenticated = auth.isLoggedIn();
      sessionBootstrapState.markSessionKnown(isAuthenticated);
      sessionBootstrapState.markInitialRouteResolved();
      return of(isAuthenticated ? true : router.createUrlTree(['/login']));
    }),
    catchError(() => {
      sessionBootstrapState.markSessionKnown(false);
      sessionBootstrapState.markInitialRouteResolved();
      return of(router.createUrlTree(['/login']));
    }),
  );
};

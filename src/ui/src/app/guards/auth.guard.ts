import { inject } from '@angular/core';
import { CanActivateFn, Router } from '@angular/router';
import { catchError, map, of, switchMap } from 'rxjs';
import { AuthService } from '../services/auth.service';
import { BootstrapService } from '../services/bootstrap.service';
import { SessionBootstrapStateService, isSafeReturnUrl } from '../services/session-bootstrap-state.service';

function loginTree(router: Router, returnUrl: string | undefined) {
  const safe = returnUrl && isSafeReturnUrl(returnUrl) && returnUrl !== '/' ? returnUrl : null;
  return router.createUrlTree(['/login'], safe ? { queryParams: { returnUrl: safe } } : {});
}

export const authGuard: CanActivateFn = (_route, routerState) => {
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
            return isAuthenticated ? true : loginTree(router, routerState?.url);
          }),
          catchError((err: unknown) => {
            if (auth.isReauthError(err)) {
              auth.handleReauthRequired(routerState?.url);
            }
            sessionBootstrapState.markSessionKnown(false);
            sessionBootstrapState.markFirstGuardRevalidationComplete();
            sessionBootstrapState.markInitialRouteResolved();
            return of(loginTree(router, routerState?.url));
          }),
        );
      }

      const isAuthenticated = auth.isLoggedIn();
      sessionBootstrapState.markSessionKnown(isAuthenticated);
      sessionBootstrapState.markInitialRouteResolved();
      return of(isAuthenticated ? true : loginTree(router, routerState?.url));
    }),
    catchError(() => {
      sessionBootstrapState.markSessionKnown(false);
      sessionBootstrapState.markInitialRouteResolved();
      return of(router.createUrlTree(['/login']));
    }),
  );
};

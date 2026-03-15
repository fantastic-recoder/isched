import { inject } from '@angular/core';
import { CanActivateFn, Router } from '@angular/router';
import { map, catchError, of } from 'rxjs';
import { GraphQLService } from '../services/graphql.service';
import { AuthService } from '../services/auth.service';

const SYSTEM_STATE_QUERY = `{ systemState { seedModeActive } }`;

interface SystemStateResponse {
  systemState: { seedModeActive: boolean };
}

export const authGuard: CanActivateFn = (_route, _state) => {
  const gql = inject(GraphQLService);
  const auth = inject(AuthService);
  const router = inject(Router);

  return gql.query<SystemStateResponse>(SYSTEM_STATE_QUERY).pipe(
    map(({ systemState }) => {
      if (systemState.seedModeActive) {
        return router.createUrlTree(['/seed']);
      }
      if (!auth.isLoggedIn()) {
        return router.createUrlTree(['/login']);
      }
      return true;
    }),
    catchError(() => of(router.createUrlTree(['/login']))),
  );
};

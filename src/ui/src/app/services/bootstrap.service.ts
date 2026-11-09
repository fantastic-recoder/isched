import { Injectable, inject } from '@angular/core';
import { BehaviorSubject, Observable, map, tap } from 'rxjs';
import { BootstrapEligibilitySource } from './auth-bootstrap.models';
import { GraphQLService } from './graphql.service';
import { SessionBootstrapStateService } from './session-bootstrap-state.service';

interface BootstrapStatusPayload {
  isched: {
    systemState: {
      seedModeActive: boolean;
    };
  };
}

interface BootstrapStatusResponse {
  systemState: {
    seedModeActive: boolean;
  };
}

interface CompleteBootstrapPayload {
  isched: {
    bootstrapPlatformAdmin: {
      csrfToken: string;
    };
  };
}

interface CompleteBootstrapResponse {
  bootstrapPlatformAdmin: {
    csrfToken: string;
  };
}


@Injectable({ providedIn: 'root' })
export class BootstrapService {
  private readonly gql = inject(GraphQLService);
  private readonly sessionBootstrapState = inject(SessionBootstrapStateService);
  // Null means "no local override"; true/false can be used by UI as an immediate hint.
  private readonly seedModeHint = new BehaviorSubject<boolean | null>(null);

  bootstrapStatus(source: BootstrapEligibilitySource = 'StartupProbe'): Observable<BootstrapStatusResponse> {
    return this.gql
      .query<BootstrapStatusPayload>('query BootstrapStatus { isched { systemState { seedModeActive } } }')
      .pipe(
        tap(({ isched }) => {
          this.seedModeHint.next(isched.systemState.seedModeActive);
          this.sessionBootstrapState.markBootstrapAvailability(isched.systemState.seedModeActive, source);
        }),
        map(({ isched }) => ({ systemState: isched.systemState })),
      );
  }

  seedModeHint$(): Observable<boolean | null> {
    return this.seedModeHint.asObservable();
  }

  completeBootstrap(input: {
    email: string;
    password: string;
    displayName: string;
  }): Observable<CompleteBootstrapResponse> {
    return this.gql
      .mutate<CompleteBootstrapPayload>(
        `mutation($email: String!, $password: String!, $displayName: String) {
           isched {
             bootstrapPlatformAdmin(input: { email: $email, password: $password, displayName: $displayName }) {
                csrfToken
              }
           }
         }`,
        input,
      )
      .pipe(
        // Bootstrap completion means seed mode must be considered inactive in the UI.
        tap(() => {
          this.seedModeHint.next(false);
          this.sessionBootstrapState.markBootstrapAvailability(false, 'ActionProbe');
        }),
        map(({ isched }) => ({ bootstrapPlatformAdmin: isched.bootstrapPlatformAdmin })),
      );
  }
}

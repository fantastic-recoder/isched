import { Injectable, inject } from '@angular/core';
import { Observable, catchError, map, switchMap, tap, throwError } from 'rxjs';
import { GRAPHQL_ERROR_CODES, GraphQLRequestError, GraphQLService } from './graphql.service';
import { mapAuthAttemptOutcome, mapAuthErrorToAlert } from './auth-alert.mapper';
import { AuthAttemptOutcome, UserFacingAlert } from './auth-bootstrap.models';
import { SessionBootstrapStateService } from './session-bootstrap-state.service';
import { ShellStatusService } from './shell-status.service';

export class AuthSignInError extends Error {
  constructor(
    message: string,
    public readonly outcome: AuthAttemptOutcome,
    public readonly alert: UserFacingAlert,
  ) {
    super(message);
    this.name = 'AuthSignInError';
  }
}

interface CurrentSessionResponse {
  isched: {
    currentUser: { id: string; name?: string | null } | null;
    currentSessionCsrfToken: string | null;
  };
}

interface SignInResponse {
  isched: {
    login: {
      csrfToken?: string;
    };
  };
}

interface SignOutResponse {
  isched: {
    logout: boolean;
  };
}

@Injectable({ providedIn: 'root' })
export class AuthService {
  private readonly gql = inject(GraphQLService);
  private readonly sessionBootstrapState = inject(SessionBootstrapStateService);
  private readonly shellStatus = inject(ShellStatusService);
  private csrfToken: string | null = null;
  private authenticated = false;

  signIn(email: string, password: string): Observable<boolean> {
    return this.gql
      .mutate<SignInResponse>(
        `mutation($email: String!, $password: String!) {
          isched {
            login(email: $email, password: $password) {
              csrfToken
            }
          }
        }`,
        { email, password },
      )
      .pipe(
        map((res) => {
          if (res.isched?.login?.csrfToken) {
            this.csrfToken = res.isched.login.csrfToken;
            return true;
          }
          this.clearAuthState();
          return false;
        }),
        switchMap((ok) => {
          if (!ok) {
            return [false];
          }
          return this.bootstrapSession();
        }),
        catchError((err: unknown) => {
          const alert = mapAuthErrorToAlert(err);
          const outcome = this.mapAuthFailureToOutcome(err, alert);
          return throwError(() => new AuthSignInError(alert.body, outcome, alert));
        }),
      );
  }

  bootstrapSession(): Observable<boolean> {
    return this.gql
      .query<CurrentSessionResponse>(
        'query { isched { currentUser { id name } currentSessionCsrfToken } }',
      )
      .pipe(
        map((res) => ({
          currentUser: res.isched?.currentUser ?? null,
          csrfToken: res.isched?.currentSessionCsrfToken ?? null,
        })),
        tap(({ currentUser, csrfToken }) => {
          const isAuthenticated = !!currentUser && !!csrfToken;
          this.authenticated = isAuthenticated;
          this.csrfToken = isAuthenticated ? csrfToken : null;

          if (isAuthenticated && currentUser) {
            this.shellStatus.setIdentity({
              userId: currentUser.id,
              displayName: currentUser.name,
            });
          } else {
            this.shellStatus.reset();
          }

          this.sessionBootstrapState.markSessionKnown(isAuthenticated);
        }),
        map(({ currentUser, csrfToken }) => !!currentUser && !!csrfToken),
        catchError((error: unknown) => {
          this.clearEphemeralAuthState();
          this.shellStatus.reset();
          this.sessionBootstrapState.clearSessionIndicators();
          return throwError(() => error);
        }),
      );
  }

  isLoggedIn(): boolean {
    return this.authenticated;
  }

  setCsrfToken(token: string): void {
    this.csrfToken = token;
  }

  getCsrfToken(): string | null {
    return this.csrfToken;
  }

  /**
   * Handles a protected-operation failure. UNAUTHENTICATED / HTTP 401 means the session is
   * gone: clear the in-memory CSRF and non-secret session state and flag `reauth-required`.
   */
  handleReauthRequired(returnUrl?: string | null): void {
    this.clearAuthState();
    this.sessionBootstrapState.markReauthRequired(returnUrl);
  }

  /** True when the given error maps to the `reauth-required` outcome. */
  isReauthError(error: unknown): boolean {
    return (
      error instanceof GraphQLRequestError && error.code === GRAPHQL_ERROR_CODES.UNAUTHENTICATED
    );
  }

  clearAuthState(): void {
    this.clearEphemeralAuthState();
    this.shellStatus.reset();
    this.sessionBootstrapState.clearSessionIndicators();
  }

  signOut(): Observable<boolean> {
    return this.gql
      .mutate<SignOutResponse>('mutation { isched { logout } }')
      .pipe(
        map((res) => res.isched?.logout ?? false),
        tap({
          next: () => {
            this.clearAuthState();
          },
          error: () => {
            this.clearAuthState();
          },
        }),
      );
  }

  private clearEphemeralAuthState(): void {
    this.authenticated = false;
    this.csrfToken = null;
  }

  private mapAuthFailureToOutcome(error: unknown, alert: UserFacingAlert): AuthAttemptOutcome {
    if (error instanceof GraphQLRequestError) {
      return mapAuthAttemptOutcome(error);
    }

    return {
      status: 'TransportFailure',
      errorCode: undefined,
      message: alert.body,
      guidanceText: alert.body,
      occurredAt: new Date().toISOString(),
    };
  }

}

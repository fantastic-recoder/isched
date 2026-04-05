import { Injectable, inject } from '@angular/core';
import { Observable, catchError, map, switchMap, tap, throwError } from 'rxjs';
import { GraphQLRequestError, GraphQLService } from './graphql.service';
import { mapAuthAttemptOutcome, mapAuthErrorToAlert } from './auth-alert.mapper';
import { AuthAttemptOutcome, UserFacingAlert } from './auth-bootstrap.models';
import { SessionBootstrapStateService } from './session-bootstrap-state.service';

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
  currentUser: { id: string } | null;
}

interface SignInResponse {
  login: {
    token?: string;
  };
}

interface SignOutResponse {
  logout: boolean;
}

@Injectable({ providedIn: 'root' })
export class AuthService {
  private readonly gql = inject(GraphQLService);
  private readonly sessionBootstrapState = inject(SessionBootstrapStateService);
  private csrfToken: string | null = null;
  private accessToken: string | null = null;
  private authenticated = false;

  signIn(email: string, password: string): Observable<boolean> {
    return this.gql
      .mutate<SignInResponse>(
        `mutation($email: String!, $password: String!) {
          login(email: $email, password: $password) {
            token
          }
        }`,
        { email, password },
      )
      .pipe(
        map((res) => {
          if (res.login?.token) {
            this.accessToken = res.login.token;
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
      .query<CurrentSessionResponse>('query { currentUser { id } }')
      .pipe(
        map((res) => !!res.currentUser),
        tap((isAuthenticated) => {
          this.authenticated = isAuthenticated;
          if (isAuthenticated && !this.csrfToken) {
            // Keep CSRF material ephemeral and in-memory only.
            this.csrfToken = this.createEphemeralCsrfToken();
          }
          if (!isAuthenticated) {
            this.accessToken = null;
            this.csrfToken = null;
          }
          this.sessionBootstrapState.markSessionKnown(isAuthenticated);
        }),
        catchError((error: unknown) => {
          this.clearEphemeralAuthState();
          this.sessionBootstrapState.clearSessionIndicators();
          return throwError(() => error);
        }),
      );
  }

  isLoggedIn(): boolean {
    return this.authenticated;
  }

  getToken(): string | null {
    return this.accessToken;
  }

  setCsrfToken(token: string): void {
    this.csrfToken = token;
  }

  getCsrfToken(): string | null {
    return this.csrfToken;
  }

  clearAuthState(): void {
    this.clearEphemeralAuthState();
    this.sessionBootstrapState.clearSessionIndicators();
  }

  signOut(): Observable<boolean> {
    return this.gql
      .mutate<SignOutResponse>('mutation { logout }')
      .pipe(
        map((res) => res.logout),
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
    this.accessToken = null;
    this.csrfToken = null;
  }

  private createEphemeralCsrfToken(): string {
    return `csrf_${Date.now()}_${Math.random().toString(36).slice(2, 12)}`;
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

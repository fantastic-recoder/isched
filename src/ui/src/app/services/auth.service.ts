import { Injectable, inject } from '@angular/core';
import { Observable, catchError, map, switchMap, tap, throwError } from 'rxjs';
import { GraphQLRequestError, GraphQLService, GRAPHQL_ERROR_CODES } from './graphql.service';

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
          if (err instanceof GraphQLRequestError && err.code === GRAPHQL_ERROR_CODES.RATE_LIMITED) {
            return throwError(() => new Error(this.buildRateLimitedGuidance(err.retryAfterMs)));
          }
          return throwError(() => err);
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
            this.csrfToken = null;
          }
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
    this.authenticated = false;
    this.accessToken = null;
    this.csrfToken = null;
  }

  signOut(): Observable<boolean> {
    return this.gql
      .mutate<SignOutResponse>('mutation { logout }')
      .pipe(
        map((res) => res.logout),
        tap(() => {
          this.clearAuthState();
        }),
      );
  }

  private createEphemeralCsrfToken(): string {
    return `csrf_${Date.now()}_${Math.random().toString(36).slice(2, 12)}`;
  }

  private buildRateLimitedGuidance(retryAfterMs?: number): string {
    if (typeof retryAfterMs !== 'number' || retryAfterMs <= 0) {
      return 'Too many failed sign-in attempts. Please wait a few minutes before trying again.';
    }

    const retryAfterSeconds = Math.max(1, Math.ceil(retryAfterMs / 1000));
    return `Too many failed sign-in attempts. Try again in about ${retryAfterSeconds} second${retryAfterSeconds === 1 ? '' : 's'}.`;
  }
}

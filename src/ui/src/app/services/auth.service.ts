import { Injectable, inject } from '@angular/core';
import { Observable, map, switchMap, tap } from 'rxjs';
import { GraphQLService } from './graphql.service';

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
        map((res) => !!res.login),
        switchMap((ok) => {
          if (!ok) {
            this.clearAuthState();
            return [false];
          }
          return this.bootstrapSession();
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

  setCsrfToken(token: string): void {
    this.csrfToken = token;
  }

  getCsrfToken(): string | null {
    return this.csrfToken;
  }

  clearAuthState(): void {
    this.authenticated = false;
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
}

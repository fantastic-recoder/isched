import { Injectable, inject } from '@angular/core';
import { Observable, map, tap } from 'rxjs';
import { GraphQLService } from './graphql.service';

interface CurrentSessionResponse {
  currentUser: { id: string } | null;
}

interface SignOutResponse {
  logout: boolean;
}

@Injectable({ providedIn: 'root' })
export class AuthService {
  private readonly gql = inject(GraphQLService);
  private csrfToken: string | null = null;
  private authenticated = false;

  bootstrapSession(): Observable<boolean> {
    return this.gql
      .query<CurrentSessionResponse>('query { currentUser { id } }')
      .pipe(
        map((res) => !!res.currentUser),
        tap((isAuthenticated) => {
          this.authenticated = isAuthenticated;
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
        map((res) => !!res.logout),
        tap(() => {
          this.clearAuthState();
        }),
      );
  }
}

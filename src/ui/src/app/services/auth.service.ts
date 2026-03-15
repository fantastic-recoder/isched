import { Injectable, inject } from '@angular/core';
import { GraphQLService } from './graphql.service';

const SESSION_KEY = 'isched_token';

@Injectable({ providedIn: 'root' })
export class AuthService {
  private readonly gql = inject(GraphQLService);

  setToken(token: string): void {
    sessionStorage.setItem(SESSION_KEY, token);
  }

  getToken(): string | null {
    return sessionStorage.getItem(SESSION_KEY);
  }

  clearToken(): void {
    sessionStorage.removeItem(SESSION_KEY);
  }

  isLoggedIn(): boolean {
    return !!this.getToken();
  }

  logout(): void {
    this.clearToken();
    this.gql.mutate<unknown>('mutation { logout }').subscribe({ error: () => {} });
  }
}

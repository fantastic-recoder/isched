import { HttpInterceptorFn } from '@angular/common/http';
import { inject } from '@angular/core';
import { AuthService } from '../services/auth.service';

function isGraphqlEndpoint(url: string): boolean {
  return url === '/graphql' || url.startsWith('/graphql?');
}

export const authInterceptor: HttpInterceptorFn = (req, next) => {
  const auth = inject(AuthService);

  if (!isGraphqlEndpoint(req.url)) {
    return next(req);
  }

  const body = req.body as { query?: string } | null;
  const isMutation =
    typeof body?.query === 'string' &&
    body.query.trimStart().startsWith('mutation');

  // Build headers: attach Bearer token + CSRF token as needed.
  const headers: Record<string, string> = {};
  const token = auth.getToken();
  if (token) {
    headers['Authorization'] = `Bearer ${token}`;
  }
  if (isMutation) {
    const csrf = auth.getCsrfToken();
    if (csrf) {
      headers['X-CSRF-Token'] = csrf;
    }
  }

  return next(req.clone({ setHeaders: headers, withCredentials: true }));
};

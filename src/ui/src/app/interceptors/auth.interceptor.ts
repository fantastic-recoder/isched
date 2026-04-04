import { HttpInterceptorFn } from '@angular/common/http';
import { inject } from '@angular/core';
import { AuthService } from '../services/auth.service';

export const authInterceptor: HttpInterceptorFn = (req, next) => {
  const auth = inject(AuthService);
  const body = req.body as { query?: string } | null;
  const isMutation =
    req.url.endsWith('/graphql') &&
    typeof body?.query === 'string' &&
    body.query.trimStart().startsWith('mutation');

  if (isMutation) {
    const csrf = auth.getCsrfToken();
    if (csrf) {
      return next(req.clone({ setHeaders: { 'X-CSRF-Token': csrf }, withCredentials: true }));
    }
    return next(req.clone({ withCredentials: true }));
  }
  return next(req.clone({ withCredentials: true }));
};

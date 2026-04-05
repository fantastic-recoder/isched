import { HttpInterceptorFn } from '@angular/common/http';
import { inject } from '@angular/core';
import { AuthService } from '../services/auth.service';

function isGraphqlEndpoint(url: string): boolean {
  return url === '/graphql' || url.startsWith('/graphql?');
}

export const authInterceptor: HttpInterceptorFn = (req, next) => {
  const auth = inject(AuthService);
  const body = req.body as { query?: string } | null;
  const isMutation =
    isGraphqlEndpoint(req.url) &&
    typeof body?.query === 'string' &&
    body.query.trimStart().startsWith('mutation');

  if (!isGraphqlEndpoint(req.url)) {
    return next(req);
  }

  if (isMutation) {
    const csrf = auth.getCsrfToken();
    if (csrf) {
      return next(req.clone({ setHeaders: { 'X-CSRF-Token': csrf }, withCredentials: true }));
    }
    return next(req.clone({ withCredentials: true }));
  }
  return next(req.clone({ withCredentials: true }));
};

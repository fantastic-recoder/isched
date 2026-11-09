import { HttpInterceptorFn } from '@angular/common/http';
import { inject } from '@angular/core';
import { from, switchMap } from 'rxjs';
import { AuthService } from '../services/auth.service';

function isGraphqlEndpoint(url: string): boolean {
  return url === '/graphql' || url.startsWith('/graphql?');
}

export const authInterceptor: HttpInterceptorFn = (req, next) => {
  const auth = inject(AuthService);

  if (!isGraphqlEndpoint(req.url)) {
    return next(req);
  }

  const body = req.body as { query?: string; operationName?: string } | null;
  return from(import('graphql')).pipe(
    switchMap(({ getOperationAST, parse }) => {
      let isMutation = false;
      if (typeof body?.query === 'string') {
        try {
          const operation = getOperationAST(parse(body.query), body.operationName);
          isMutation = operation?.operation === 'mutation';
        } catch {
          // Invalid documents are rejected by GraphQL; do not infer an operation from text.
        }
      }

      // The browser manages the HttpOnly cookie. JavaScript sends no bearer token.
      const headers: Record<string, string> = {};
      if (isMutation) {
        const csrf = auth.getCsrfToken();
        if (csrf) {
          headers['X-CSRF-Token'] = csrf;
        }
      }

      return next(req.clone({ setHeaders: headers, withCredentials: true }));
    }),
  );
};

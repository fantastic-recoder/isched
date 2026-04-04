import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, throwError, switchMap } from 'rxjs';

export const GRAPHQL_ERROR_CODES = {
  UNAUTHENTICATED: 'UNAUTHENTICATED',
  FORBIDDEN: 'FORBIDDEN',
  VALIDATION_FAILED: 'VALIDATION_FAILED',
  CONFLICT: 'CONFLICT',
  CSRF_FAILED: 'CSRF_FAILED',
  CONTEXT_MISMATCH: 'CONTEXT_MISMATCH',
  TRANSIENT_NETWORK: 'TRANSIENT_NETWORK',
} as const;

export type GraphQLErrorCode = (typeof GRAPHQL_ERROR_CODES)[keyof typeof GRAPHQL_ERROR_CODES];

export class GraphQLRequestError extends Error {
  constructor(
    message: string,
    public readonly code: GraphQLErrorCode,
    public readonly fieldErrors: Record<string, string[]> = {},
  ) {
    super(message);
    this.name = 'GraphQLRequestError';
  }
}

interface GraphQLResponse<T> {
  data?: T;
  errors?: Array<{
    message: string;
    extensions?: {
      code?: string;
      fieldErrors?: Record<string, string[]>;
    };
  }>;
}

@Injectable({ providedIn: 'root' })
export class GraphQLService {
  private readonly http = inject(HttpClient);
  private readonly endpoint = '/graphql';

  query<T>(doc: string, variables?: object): Observable<T> {
    return this.execute<T>(doc, variables);
  }

  mutate<T>(doc: string, variables?: object): Observable<T> {
    return this.execute<T>(doc, variables);
  }

  private execute<T>(doc: string, variables?: object): Observable<T> {
    return this.http
      .post<GraphQLResponse<T>>(this.endpoint, { query: doc, variables })
      .pipe(
        switchMap((res) => {
          if (res.errors && res.errors.length > 0) {
            const first = res.errors[0];
            const code = this.asKnownCode(first.extensions?.code);
            return throwError(
              () => new GraphQLRequestError(first.message, code, first.extensions?.fieldErrors ?? {}),
            );
          }
          return [res.data as T];
        }),
      );
  }

  private asKnownCode(maybeCode?: string): GraphQLErrorCode {
    if (!maybeCode) {
      return GRAPHQL_ERROR_CODES.TRANSIENT_NETWORK;
    }
    return (Object.values(GRAPHQL_ERROR_CODES) as string[]).includes(maybeCode)
      ? (maybeCode as GraphQLErrorCode)
      : GRAPHQL_ERROR_CODES.TRANSIENT_NETWORK;
  }
}

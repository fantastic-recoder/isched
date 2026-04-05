import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, catchError, switchMap, throwError } from 'rxjs';

export const GRAPHQL_ERROR_CODES = {
  UNAUTHENTICATED: 'UNAUTHENTICATED',
  FORBIDDEN: 'FORBIDDEN',
  VALIDATION_FAILED: 'VALIDATION_FAILED',
  CONFLICT: 'CONFLICT',
  CSRF_FAILED: 'CSRF_FAILED',
  CONTEXT_MISMATCH: 'CONTEXT_MISMATCH',
  RATE_LIMITED: 'RATE_LIMITED',
  TRANSIENT_NETWORK: 'TRANSIENT_NETWORK',
} as const;

export type GraphQLErrorCode = (typeof GRAPHQL_ERROR_CODES)[keyof typeof GRAPHQL_ERROR_CODES];

interface GraphQLErrorExtensions {
  code?: string;
  fieldErrors?: Record<string, unknown>;
  retryAfterMs?: unknown;
}

interface GraphQLErrorPayload {
  message?: string;
  extensions?: GraphQLErrorExtensions;
}

export interface GraphQLNormalizedError {
  message: string;
  code: GraphQLErrorCode;
  fieldErrors: Record<string, string[]>;
  retryAfterMs?: number;
}

const KNOWN_GRAPHQL_ERROR_CODES: ReadonlySet<string> = new Set(Object.values(GRAPHQL_ERROR_CODES));

export function asKnownGraphQLErrorCode(maybeCode?: string): GraphQLErrorCode {
  if (!maybeCode) {
    return GRAPHQL_ERROR_CODES.TRANSIENT_NETWORK;
  }
  return KNOWN_GRAPHQL_ERROR_CODES.has(maybeCode)
    ? (maybeCode as GraphQLErrorCode)
    : GRAPHQL_ERROR_CODES.TRANSIENT_NETWORK;
}

export function normalizeGraphQLError(error?: GraphQLErrorPayload): GraphQLNormalizedError {
  return {
    message: error?.message || 'GraphQL request failed',
    code: asKnownGraphQLErrorCode(error?.extensions?.code),
    fieldErrors: normalizeFieldErrors(error?.extensions?.fieldErrors),
    retryAfterMs: normalizeRetryAfterMs(error?.extensions?.retryAfterMs),
  };
}

function normalizeRetryAfterMs(retryAfterMs: unknown): number | undefined {
  return typeof retryAfterMs === 'number' && Number.isFinite(retryAfterMs) && retryAfterMs > 0
    ? Math.floor(retryAfterMs)
    : undefined;
}

function normalizeFieldErrors(fieldErrors: Record<string, unknown> | undefined): Record<string, string[]> {
  if (!fieldErrors) {
    return {};
  }

  return Object.entries(fieldErrors).reduce<Record<string, string[]>>((acc, [key, value]) => {
    if (!Array.isArray(value)) {
      return acc;
    }

    const messages = value.filter((entry): entry is string => typeof entry === 'string' && entry.length > 0);
    if (messages.length > 0) {
      acc[key] = messages;
    }

    return acc;
  }, {});
}

export class GraphQLRequestError extends Error {
  constructor(
    message: string,
    public readonly code: GraphQLErrorCode,
    public readonly fieldErrors: Record<string, string[]> = {},
    public readonly retryAfterMs?: number, // For RATE_LIMITED responses
  ) {
    super(message);
    this.name = 'GraphQLRequestError';
  }
}

interface GraphQLResponse<T> {
  data?: T;
  errors?: GraphQLErrorPayload[];
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
            const normalizedError = normalizeGraphQLError(res.errors[0]);
            return throwError(
              () => new GraphQLRequestError(
                normalizedError.message,
                normalizedError.code,
                normalizedError.fieldErrors,
                normalizedError.retryAfterMs,
              ),
            );
          }
          if (typeof res.data === 'undefined') {
            return throwError(
              () =>
                new GraphQLRequestError(
                  'GraphQL response did not include data',
                  GRAPHQL_ERROR_CODES.TRANSIENT_NETWORK,
                ),
            );
          }
          return [res.data as T];
        }),
        catchError((err: unknown) => {
          if (err instanceof GraphQLRequestError) {
            return throwError(() => err);
          }
          return throwError(
            () =>
              new GraphQLRequestError(
                'GraphQL request failed',
                GRAPHQL_ERROR_CODES.TRANSIENT_NETWORK,
              ),
          );
        }),
      );
  }
}

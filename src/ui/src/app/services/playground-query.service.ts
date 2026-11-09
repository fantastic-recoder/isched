// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground-query.service.ts
 * @brief Executes ad-hoc GraphQL queries from the playground editor (SP-011).
 *        Detects subscription stubs and returns an advisory result without
 *        making a network request.
 */

import { Injectable, inject } from '@angular/core';
import { HttpClient, HttpErrorResponse } from '@angular/common/http';
import { Observable, catchError, map, of, startWith } from 'rxjs';
import {
  GRAPHQL_ERROR_CODES,
  GraphQLErrorCode,
  normalizeGraphQLError,
} from './graphql.service';

export type PlaygroundErrorCategory = 'validation' | 'authorization' | 'transport' | 'unknown';

export interface PlaygroundQueryResult {
  loading: boolean;
  data?: unknown;
  errors?: string[];
  isSubscriptionAdvisory?: boolean;
  /** GraphQLErrorPresentation-shaped fields (spec 013). */
  errorCategory?: PlaygroundErrorCategory;
  retryable?: boolean;
  /** True when the failure means the session expired / user must sign in again. */
  reauthRequired?: boolean;
  code?: GraphQLErrorCode;
  fieldErrors?: Record<string, string[]>;
  retryAfterMs?: number;
}

/** Maps a normalized GraphQL error code to a presentation category + retryability. */
export function categorizeGraphQLErrorCode(code: string | undefined): {
  category: PlaygroundErrorCategory;
  retryable: boolean;
} {
  switch (code) {
    case GRAPHQL_ERROR_CODES.VALIDATION_FAILED:
    case 'BAD_USER_INPUT':
    case GRAPHQL_ERROR_CODES.CONFLICT:
      return { category: 'validation', retryable: false };
    case GRAPHQL_ERROR_CODES.UNAUTHENTICATED:
    case GRAPHQL_ERROR_CODES.FORBIDDEN:
    case GRAPHQL_ERROR_CODES.CSRF_FAILED:
    case GRAPHQL_ERROR_CODES.CONTEXT_MISMATCH:
      return { category: 'authorization', retryable: false };
    case GRAPHQL_ERROR_CODES.TRANSIENT_NETWORK:
    case GRAPHQL_ERROR_CODES.RATE_LIMITED:
      return { category: 'transport', retryable: true };
    default:
      return { category: 'unknown', retryable: false };
  }
}

const SUBSCRIPTION_PATTERN = /^\s*subscription\b/i;

@Injectable({ providedIn: 'root' })
export class PlaygroundQueryService {
  private readonly http = inject(HttpClient);
  private readonly endpoint = '/graphql';

  /**
   * Execute a raw GraphQL operation string.
   * Emits `{ loading: true }` immediately, then the final result.
   * Subscription operations return an advisory without a network request.
   */
  execute(queryText: string): Observable<PlaygroundQueryResult> {
    if (SUBSCRIPTION_PATTERN.test(queryText)) {
      return of<PlaygroundQueryResult>({ loading: false, isSubscriptionAdvisory: true });
    }

    return this.http
      .post<{ data?: unknown; errors?: { message: string; extensions?: { code?: string; fieldErrors?: Record<string, unknown>; retryAfterMs?: unknown } }[] }>(this.endpoint, {
        query: queryText,
      })
      .pipe(
        map((res): PlaygroundQueryResult => {
          if (res.errors && res.errors.length > 0) {
            const first = normalizeGraphQLError(res.errors[0]);
            const rawCode = (res.errors[0] as { extensions?: { code?: string } }).extensions?.code;
            const { category, retryable } = categorizeGraphQLErrorCode(rawCode ?? undefined);
            return {
              loading: false,
              errors: res.errors.map((e) => e.message),
              errorCategory: rawCode ? category : 'unknown',
              retryable: rawCode ? retryable : false,
              reauthRequired: rawCode === GRAPHQL_ERROR_CODES.UNAUTHENTICATED,
              code: first.code,
              fieldErrors: first.fieldErrors,
              retryAfterMs: first.retryAfterMs,
            };
          }
          return { loading: false, data: res.data ?? null };
        }),
        catchError((err: unknown): Observable<PlaygroundQueryResult> => {
          const msg =
            err && typeof err === 'object' && 'message' in err
              ? String((err as { message: unknown }).message)
              : 'Network error';
          const status = err instanceof HttpErrorResponse ? err.status : undefined;
          if (status === 401) {
            return of<PlaygroundQueryResult>({
              loading: false,
              errors: [msg],
              errorCategory: 'authorization',
              retryable: false,
              reauthRequired: true,
              code: GRAPHQL_ERROR_CODES.UNAUTHENTICATED,
            });
          }
          if (status === 403) {
            return of<PlaygroundQueryResult>({
              loading: false,
              errors: [msg],
              errorCategory: 'authorization',
              retryable: false,
              code: GRAPHQL_ERROR_CODES.FORBIDDEN,
            });
          }
          return of<PlaygroundQueryResult>({
            loading: false,
            errors: [msg],
            errorCategory: 'transport',
            retryable: true,
            code: GRAPHQL_ERROR_CODES.TRANSIENT_NETWORK,
          });
        }),
        startWith<PlaygroundQueryResult>({ loading: true }),
      );
  }
}


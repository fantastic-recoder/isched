// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground-query.service.ts
 * @brief Executes ad-hoc GraphQL queries from the playground editor (SP-011).
 *        Detects subscription stubs and returns an advisory result without
 *        making a network request.
 */

import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, catchError, map, of, startWith } from 'rxjs';

export interface PlaygroundQueryResult {
  loading: boolean;
  data?: unknown;
  errors?: string[];
  isSubscriptionAdvisory?: boolean;
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
      .post<{ data?: unknown; errors?: { message: string }[] }>(this.endpoint, {
        query: queryText,
      })
      .pipe(
        map((res): PlaygroundQueryResult => {
          if (res.errors && res.errors.length > 0) {
            return { loading: false, errors: res.errors.map((e) => e.message) };
          }
          return { loading: false, data: res.data ?? null };
        }),
        catchError((err: unknown): Observable<PlaygroundQueryResult> => {
          const msg =
            err && typeof err === 'object' && 'message' in err
              ? String((err as { message: unknown }).message)
              : 'Network error';
          return of({ loading: false, errors: [msg] });
        }),
        startWith<PlaygroundQueryResult>({ loading: true }),
      );
  }
}


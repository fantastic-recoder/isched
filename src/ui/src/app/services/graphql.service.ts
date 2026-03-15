import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, map, throwError, switchMap } from 'rxjs';

interface GraphQLResponse<T> {
  data?: T;
  errors?: Array<{ message: string }>;
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
            return throwError(() => new Error(res.errors![0].message));
          }
          return [res.data as T];
        }),
      );
  }
}

import { Injectable, inject } from '@angular/core';
import { BehaviorSubject, Observable, tap } from 'rxjs';
import { GraphQLService } from './graphql.service';

interface BootstrapStatusResponse {
  systemState: {
    seedModeActive: boolean;
  };
}

interface CompleteBootstrapResponse {
  bootstrapPlatformAdmin: {
    token: string;
    expiresAt: string;
  };
}

@Injectable({ providedIn: 'root' })
export class BootstrapService {
  private readonly gql = inject(GraphQLService);
  // Null means "no local override"; true/false can be used by UI as an immediate hint.
  private readonly seedModeHint = new BehaviorSubject<boolean | null>(null);

  bootstrapStatus(): Observable<BootstrapStatusResponse> {
    return this.gql.query<BootstrapStatusResponse>('query { systemState { seedModeActive } }');
  }

  seedModeHint$(): Observable<boolean | null> {
    return this.seedModeHint.asObservable();
  }

  completeBootstrap(input: {
    email: string;
    password: string;
    displayName: string;
  }): Observable<CompleteBootstrapResponse> {
    return this.gql.mutate<CompleteBootstrapResponse>(
      `mutation($email: String!, $password: String!, $displayName: String) {
         bootstrapPlatformAdmin(input: { email: $email, password: $password, displayName: $displayName }) {
           token
           expiresAt
         }
       }`,
      input,
    ).pipe(
      // Bootstrap completion means seed mode must be considered inactive in the UI.
      tap(() => this.seedModeHint.next(false)),
    );
  }
}
